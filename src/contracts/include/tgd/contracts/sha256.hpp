#pragma once

#include <array>
#include <cstdint>
#include <span>

namespace tgd::contracts {

using Sha256Digest = std::array<std::uint8_t, 32>;

[[nodiscard]] Sha256Digest sha256(std::span<const std::uint8_t> bytes) noexcept;

}  // namespace tgd::contracts
