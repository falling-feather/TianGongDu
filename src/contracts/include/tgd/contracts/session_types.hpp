#pragma once

#include <cstdint>

namespace tgd::contracts {

using TickIndex = std::uint64_t;
using CommandSequence = std::uint64_t;
using StableActorKey = std::uint64_t;
using CollisionShapeId = std::uint32_t;

inline constexpr std::int32_t ground_axis_one = 32'767;

struct GroundVectorQ15 final {
    std::int32_t x{};
    std::int32_t y{};

    [[nodiscard]] friend constexpr bool operator==(
        const GroundVectorQ15&,
        const GroundVectorQ15&
    ) noexcept = default;
};

struct GroundPoseMm final {
    std::int32_t x{};
    std::int32_t y{};
    std::int32_t height{};
    std::int16_t floor_layer{};

    [[nodiscard]] friend constexpr bool operator==(
        const GroundPoseMm&,
        const GroundPoseMm&
    ) noexcept = default;
};

enum class SessionCommandType : std::uint8_t {
    move_intent,
    jump_pressed,
};

struct CommandHeader final {
    TickIndex tick{};
    StableActorKey actor{};
    CommandSequence sequence{};
    SessionCommandType type{SessionCommandType::move_intent};
};

struct SessionCommand final {
    CommandHeader header{};
    GroundVectorQ15 ground_direction{};
};

}  // namespace tgd::contracts
