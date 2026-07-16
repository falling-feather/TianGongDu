#pragma once

#include <tgd/content/sandbox_package_compiler.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

namespace tgd::content {

class SandboxPackagePublicationIdentity final {
  public:
    constexpr SandboxPackagePublicationIdentity() noexcept = default;

    constexpr SandboxPackagePublicationIdentity(
        std::uint32_t generation,
        contracts::Sha256Digest checksum
    ) noexcept
        : generation_(generation), checksum_(checksum) {}

    [[nodiscard]] constexpr std::uint32_t generation() const noexcept {
        return generation_;
    }

    [[nodiscard]] constexpr const contracts::Sha256Digest& checksum() const noexcept {
        return checksum_;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxPackagePublicationIdentity&,
        const SandboxPackagePublicationIdentity&
    ) noexcept = default;

  private:
    std::uint32_t generation_{};
    contracts::Sha256Digest checksum_{};
};

enum class SandboxPackageGenerationAdvanceStatus : std::uint8_t {
    advanced = 1,
    exhausted = 2,
    invalid = 255,
};

[[nodiscard]] constexpr bool sandbox_package_generation_advance_status_valid(
    SandboxPackageGenerationAdvanceStatus status
) noexcept {
    switch (status) {
        case SandboxPackageGenerationAdvanceStatus::advanced:
        case SandboxPackageGenerationAdvanceStatus::exhausted:
            return true;
        case SandboxPackageGenerationAdvanceStatus::invalid:
            return false;
    }
    return false;
}

class SandboxPackageGenerationAdvance final {
  public:
    [[nodiscard]] constexpr SandboxPackageGenerationAdvanceStatus status() const
        noexcept {
        return status_;
    }

    [[nodiscard]] constexpr std::uint32_t generation() const noexcept {
        return generation_;
    }

  private:
    constexpr SandboxPackageGenerationAdvance(
        SandboxPackageGenerationAdvanceStatus status,
        std::uint32_t generation
    ) noexcept
        : status_(status), generation_(generation) {}

    SandboxPackageGenerationAdvanceStatus status_{
        SandboxPackageGenerationAdvanceStatus::invalid
    };
    std::uint32_t generation_{};

    friend constexpr SandboxPackageGenerationAdvance sandbox_next_package_generation(
        std::uint32_t current
    ) noexcept;
};

[[nodiscard]] constexpr SandboxPackageGenerationAdvance sandbox_next_package_generation(
    std::uint32_t current
) noexcept {
    if (current == UINT32_MAX) {
        return {SandboxPackageGenerationAdvanceStatus::exhausted, 0};
    }
    return {SandboxPackageGenerationAdvanceStatus::advanced, current + 1U};
}

enum class SandboxPackagePrepareStatus : std::uint8_t {
    prepared = 1,
    stale_generation = 2,
    stale_checksum = 3,
    missing_candidate = 4,
    generation_exhausted = 5,
    invalid = 255,
};

[[nodiscard]] constexpr bool sandbox_package_prepare_status_valid(
    SandboxPackagePrepareStatus status
) noexcept {
    switch (status) {
        case SandboxPackagePrepareStatus::prepared:
        case SandboxPackagePrepareStatus::stale_generation:
        case SandboxPackagePrepareStatus::stale_checksum:
        case SandboxPackagePrepareStatus::missing_candidate:
        case SandboxPackagePrepareStatus::generation_exhausted:
            return true;
        case SandboxPackagePrepareStatus::invalid:
            return false;
    }
    return false;
}

enum class SandboxPackageCommitStatus : std::uint8_t {
    committed = 1,
    foreign_provider = 2,
    stale_generation = 3,
    stale_checksum = 4,
    invalid_prepared_update = 5,
    invalid = 255,
};

[[nodiscard]] constexpr bool sandbox_package_commit_status_valid(
    SandboxPackageCommitStatus status
) noexcept {
    switch (status) {
        case SandboxPackageCommitStatus::committed:
        case SandboxPackageCommitStatus::foreign_provider:
        case SandboxPackageCommitStatus::stale_generation:
        case SandboxPackageCommitStatus::stale_checksum:
        case SandboxPackageCommitStatus::invalid_prepared_update:
            return true;
        case SandboxPackageCommitStatus::invalid:
            return false;
    }
    return false;
}

class SandboxPackageProvider;

// A prepared update owns its compiler-produced candidate. It may be inspected
// synchronously while preparing downstream state, but only its originating
// provider may consume it. Moving or committing invalidates the source token.
class SandboxPackagePreparedUpdate final {
  public:
    SandboxPackagePreparedUpdate(const SandboxPackagePreparedUpdate&) = delete;
    SandboxPackagePreparedUpdate& operator=(const SandboxPackagePreparedUpdate&) =
        delete;
    SandboxPackagePreparedUpdate(SandboxPackagePreparedUpdate&& other) noexcept;
    SandboxPackagePreparedUpdate& operator=(SandboxPackagePreparedUpdate&& other)
        noexcept;

    [[nodiscard]] const SandboxPackagePublicationIdentity& expected_identity() const
        noexcept;
    [[nodiscard]] const SandboxPackagePublicationIdentity& next_identity() const
        noexcept;
    [[nodiscard]] const SandboxPackageCandidate* candidate() const noexcept;
    [[nodiscard]] const SandboxPackageDocument* document() const noexcept;

  private:
    SandboxPackagePreparedUpdate(
        SandboxPackageProvider* provider,
        SandboxPackagePublicationIdentity expected_identity,
        SandboxPackagePublicationIdentity next_identity,
        std::unique_ptr<SandboxPackageCandidate> candidate
    ) noexcept;

    void invalidate() noexcept;

    SandboxPackageProvider* provider_{};
    SandboxPackagePublicationIdentity expected_identity_{};
    SandboxPackagePublicationIdentity next_identity_{};
    std::unique_ptr<SandboxPackageCandidate> candidate_{};

    friend class SandboxPackageProvider;
};

class SandboxPackagePrepareResult final {
  public:
    SandboxPackagePrepareResult(const SandboxPackagePrepareResult&) = delete;
    SandboxPackagePrepareResult& operator=(const SandboxPackagePrepareResult&) =
        delete;
    SandboxPackagePrepareResult(SandboxPackagePrepareResult&& other) noexcept;
    SandboxPackagePrepareResult& operator=(SandboxPackagePrepareResult&& other)
        noexcept;

    [[nodiscard]] SandboxPackagePrepareStatus status() const noexcept;
    [[nodiscard]] const SandboxPackagePreparedUpdate* prepared_update() const noexcept;
    [[nodiscard]] std::optional<SandboxPackagePreparedUpdate> take_prepared_update() &&
        noexcept;

  private:
    SandboxPackagePrepareResult(
        SandboxPackagePrepareStatus status,
        std::optional<SandboxPackagePreparedUpdate> prepared_update
    ) noexcept;

    SandboxPackagePrepareStatus status_{SandboxPackagePrepareStatus::invalid};
    std::optional<SandboxPackagePreparedUpdate> prepared_update_{};

    friend class SandboxPackageProvider;
};

class SandboxPackageCommitResult final {
  public:
    [[nodiscard]] SandboxPackageCommitStatus status() const noexcept;

  private:
    explicit SandboxPackageCommitResult(SandboxPackageCommitStatus status) noexcept;

    SandboxPackageCommitStatus status_{SandboxPackageCommitStatus::invalid};

    friend class SandboxPackageProvider;
};

// ContentCore owns the last-valid package publication. The provider is used by
// one coordinator thread only; it performs no locking, callbacks, or async work.
// Returned candidate/document addresses remain valid only until a successful
// commit replaces the publication.
class SandboxPackageProvider final {
  public:
    SandboxPackageProvider() noexcept = default;
    SandboxPackageProvider(const SandboxPackageProvider&) = delete;
    SandboxPackageProvider& operator=(const SandboxPackageProvider&) = delete;
    SandboxPackageProvider(SandboxPackageProvider&&) = delete;
    SandboxPackageProvider& operator=(SandboxPackageProvider&&) = delete;

    [[nodiscard]] const SandboxPackagePublicationIdentity& identity() const noexcept;
    [[nodiscard]] const SandboxPackageCandidate* candidate() const noexcept;
    [[nodiscard]] const SandboxPackageDocument* document() const noexcept;

    [[nodiscard]] SandboxPackagePrepareResult prepare(
        const SandboxPackagePublicationIdentity& expected_identity,
        std::unique_ptr<SandboxPackageCandidate> candidate
    ) noexcept;

    [[nodiscard]] SandboxPackageCommitResult commit(
        SandboxPackagePreparedUpdate&& prepared_update
    ) noexcept;

  private:
    SandboxPackagePublicationIdentity identity_{};
    std::unique_ptr<SandboxPackageCandidate> candidate_{};
};

}  // namespace tgd::content
