#pragma once

#include <tgd/contracts/sandbox_definition.hpp>
#include <tgd/contracts/sandbox_gameplay_binding.hpp>
#include <tgd/contracts/sandbox_pack.hpp>
#include <tgd/contracts/sha256.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace tgd::content {

struct SandboxPackageValidation final {
    contracts::SandboxPackageError error{contracts::SandboxPackageError::none};
    std::vector<contracts::SandboxDiagnostic> diagnostics{};
    contracts::SandboxGameplayBindingValidationResult gameplay_binding_validation{};

    [[nodiscard]] bool valid() const noexcept {
        return error == contracts::SandboxPackageError::none;
    }
};

// This is the sole package-level semantic validator. The producer and decoder
// call it, and later authoring tools may call the same API. Gameplay binding
// rules remain owned by validate_sandbox_gameplay_binding.
[[nodiscard]] SandboxPackageValidation validate_sandbox_package(
    const contracts::SandboxDefinition& definition,
    const contracts::SandboxGameplayBindingDefinition& gameplay_binding
) noexcept;

struct EncodeSandboxPackageResult final {
    SandboxPackageValidation validation{};
    std::vector<std::uint8_t> bytes{};
    contracts::Sha256Digest fingerprint{};
};

[[nodiscard]] EncodeSandboxPackageResult encode_sandbox_package(
    const contracts::SandboxDefinition& definition,
    const contracts::SandboxGameplayBindingDefinition& gameplay_binding
) noexcept;

struct DecodeSandboxPackageResult;

class SandboxPackageDocument final {
  public:
    struct Storage;

    ~SandboxPackageDocument();

    SandboxPackageDocument(const SandboxPackageDocument&) = delete;
    SandboxPackageDocument& operator=(const SandboxPackageDocument&) = delete;
    SandboxPackageDocument(SandboxPackageDocument&&) = delete;
    SandboxPackageDocument& operator=(SandboxPackageDocument&&) = delete;

    [[nodiscard]] const contracts::SandboxDefinition& definition() const noexcept;
    [[nodiscard]] const contracts::SandboxGameplayBindingDefinition& gameplay_binding()
        const noexcept;
    [[nodiscard]] const contracts::Sha256Digest& fingerprint() const noexcept;

  private:
    explicit SandboxPackageDocument(std::unique_ptr<Storage> storage) noexcept;

    std::unique_ptr<Storage> storage_{};

    friend DecodeSandboxPackageResult decode_sandbox_package(
        std::span<const std::uint8_t> bytes
    ) noexcept;
};

struct DecodeSandboxPackageResult final {
    SandboxPackageValidation validation{};
    std::unique_ptr<SandboxPackageDocument> document{};
};

// Decoding owns every string and record referenced by both returned views. A
// failure never exposes a partial document.
[[nodiscard]] DecodeSandboxPackageResult decode_sandbox_package(
    std::span<const std::uint8_t> bytes
) noexcept;

}  // namespace tgd::content
