#include <tgd/content/sandbox_package_provider.hpp>

#include <optional>
#include <utility>

namespace tgd::content {

namespace {

[[nodiscard]] bool same_shared_owner(
    const std::shared_ptr<const void>& lhs,
    const std::shared_ptr<const void>& rhs
) noexcept {
    return !lhs.owner_before(rhs) && !rhs.owner_before(lhs);
}

}  // namespace

SandboxPackagePreparedUpdate::SandboxPackagePreparedUpdate(
    std::shared_ptr<const void> provider_lifetime_capability,
    SandboxPackagePublicationIdentity expected_identity,
    SandboxPackagePublicationIdentity next_identity,
    std::unique_ptr<SandboxPackageCandidate> candidate
) noexcept
    : provider_lifetime_capability_(std::move(provider_lifetime_capability)),
      expected_identity_(expected_identity),
      next_identity_(next_identity),
      candidate_(std::move(candidate)) {}

SandboxPackagePreparedUpdate::SandboxPackagePreparedUpdate(
    SandboxPackagePreparedUpdate&& other
) noexcept
    : provider_lifetime_capability_(
          std::move(other.provider_lifetime_capability_)
      ),
      expected_identity_(std::exchange(
          other.expected_identity_, SandboxPackagePublicationIdentity{}
      )),
      next_identity_(std::exchange(
          other.next_identity_, SandboxPackagePublicationIdentity{}
      )),
      candidate_(std::move(other.candidate_)) {}

SandboxPackagePreparedUpdate& SandboxPackagePreparedUpdate::operator=(
    SandboxPackagePreparedUpdate&& other
) noexcept {
    if (this != &other) {
        provider_lifetime_capability_ =
            std::move(other.provider_lifetime_capability_);
        expected_identity_ = std::exchange(
            other.expected_identity_, SandboxPackagePublicationIdentity{}
        );
        next_identity_ = std::exchange(
            other.next_identity_, SandboxPackagePublicationIdentity{}
        );
        candidate_ = std::move(other.candidate_);
    }
    return *this;
}

const SandboxPackagePublicationIdentity&
SandboxPackagePreparedUpdate::expected_identity() const noexcept {
    return expected_identity_;
}

const SandboxPackagePublicationIdentity& SandboxPackagePreparedUpdate::next_identity()
    const noexcept {
    return next_identity_;
}

const SandboxPackageCandidate* SandboxPackagePreparedUpdate::candidate() const noexcept {
    return candidate_.get();
}

const SandboxPackageDocument* SandboxPackagePreparedUpdate::document() const noexcept {
    return candidate_ == nullptr ? nullptr : &candidate_->document();
}

void SandboxPackagePreparedUpdate::invalidate() noexcept {
    provider_lifetime_capability_.reset();
    expected_identity_ = {};
    next_identity_ = {};
    candidate_.reset();
}

SandboxPackagePrepareResult::SandboxPackagePrepareResult(
    SandboxPackagePrepareStatus status,
    std::optional<SandboxPackagePreparedUpdate> prepared_update
) noexcept
    : status_(status), prepared_update_(std::move(prepared_update)) {}

SandboxPackagePrepareResult::SandboxPackagePrepareResult(
    SandboxPackagePrepareResult&& other
) noexcept
    : status_(std::exchange(other.status_, SandboxPackagePrepareStatus::invalid)),
      prepared_update_(std::move(other.prepared_update_)) {
    other.prepared_update_.reset();
}

SandboxPackagePrepareResult& SandboxPackagePrepareResult::operator=(
    SandboxPackagePrepareResult&& other
) noexcept {
    if (this != &other) {
        status_ = std::exchange(other.status_, SandboxPackagePrepareStatus::invalid);
        prepared_update_ = std::move(other.prepared_update_);
        other.prepared_update_.reset();
    }
    return *this;
}

SandboxPackagePrepareStatus SandboxPackagePrepareResult::status() const noexcept {
    return status_;
}

const SandboxPackagePreparedUpdate* SandboxPackagePrepareResult::prepared_update() const
    noexcept {
    if (status_ != SandboxPackagePrepareStatus::prepared ||
        !prepared_update_.has_value()) {
        return nullptr;
    }
    return &*prepared_update_;
}

std::optional<SandboxPackagePreparedUpdate>
SandboxPackagePrepareResult::take_prepared_update() && noexcept {
    status_ = SandboxPackagePrepareStatus::invalid;
    auto result = std::move(prepared_update_);
    prepared_update_.reset();
    return result;
}

SandboxPackageCommitResult::SandboxPackageCommitResult(
    SandboxPackageCommitStatus status
) noexcept
    : status_(status) {}

SandboxPackageCommitStatus SandboxPackageCommitResult::status() const noexcept {
    return status_;
}

SandboxPackageProvider::SandboxPackageProvider()
    : lifetime_capability_(
          std::make_shared<const std::uint8_t>(std::uint8_t{0})
      ) {}

const SandboxPackagePublicationIdentity& SandboxPackageProvider::identity() const noexcept {
    return identity_;
}

const SandboxPackageCandidate* SandboxPackageProvider::candidate() const noexcept {
    return candidate_.get();
}

const SandboxPackageDocument* SandboxPackageProvider::document() const noexcept {
    return candidate_ == nullptr ? nullptr : &candidate_->document();
}

SandboxPackagePrepareResult SandboxPackageProvider::prepare(
    const SandboxPackagePublicationIdentity& expected_identity,
    std::unique_ptr<SandboxPackageCandidate> candidate
) noexcept {
    if (expected_identity.generation() != identity_.generation()) {
        return {SandboxPackagePrepareStatus::stale_generation, std::nullopt};
    }
    if (expected_identity.checksum() != identity_.checksum()) {
        return {SandboxPackagePrepareStatus::stale_checksum, std::nullopt};
    }
    if (candidate == nullptr) {
        return {SandboxPackagePrepareStatus::missing_candidate, std::nullopt};
    }
    const auto advance = sandbox_next_package_generation(identity_.generation());
    if (advance.status() != SandboxPackageGenerationAdvanceStatus::advanced) {
        return {SandboxPackagePrepareStatus::generation_exhausted, std::nullopt};
    }

    SandboxPackagePublicationIdentity next_identity{
        advance.generation(), candidate->fingerprint()
    };
    SandboxPackagePreparedUpdate prepared_update{
        lifetime_capability_, expected_identity, next_identity, std::move(candidate)
    };
    return {
        SandboxPackagePrepareStatus::prepared,
        std::optional<SandboxPackagePreparedUpdate>{std::move(prepared_update)},
    };
}

SandboxPackageCommitResult SandboxPackageProvider::commit(
    SandboxPackagePreparedUpdate&& prepared_update
) noexcept {
    if (prepared_update.provider_lifetime_capability_ == nullptr ||
        prepared_update.candidate_ == nullptr) {
        prepared_update.invalidate();
        return SandboxPackageCommitResult{
            SandboxPackageCommitStatus::invalid_prepared_update
        };
    }
    if (!same_shared_owner(
            prepared_update.provider_lifetime_capability_, lifetime_capability_
        )) {
        prepared_update.invalidate();
        return SandboxPackageCommitResult{SandboxPackageCommitStatus::foreign_provider};
    }
    if (prepared_update.expected_identity_.generation() != identity_.generation()) {
        prepared_update.invalidate();
        return SandboxPackageCommitResult{SandboxPackageCommitStatus::stale_generation};
    }
    if (prepared_update.expected_identity_.checksum() != identity_.checksum()) {
        prepared_update.invalidate();
        return SandboxPackageCommitResult{SandboxPackageCommitStatus::stale_checksum};
    }

    const auto advance = sandbox_next_package_generation(identity_.generation());
    if (advance.status() != SandboxPackageGenerationAdvanceStatus::advanced ||
        prepared_update.next_identity_.generation() != advance.generation() ||
        prepared_update.next_identity_.checksum() !=
            prepared_update.candidate_->fingerprint()) {
        prepared_update.invalidate();
        return SandboxPackageCommitResult{
            SandboxPackageCommitStatus::invalid_prepared_update
        };
    }

    auto next_candidate = std::move(prepared_update.candidate_);
    const auto next_identity = prepared_update.next_identity_;
    prepared_update.invalidate();
    candidate_.swap(next_candidate);
    identity_ = next_identity;
    return SandboxPackageCommitResult{SandboxPackageCommitStatus::committed};
}

}  // namespace tgd::content
