#pragma once

#include <cstdint>

namespace tgd::contracts {

using TickIndex = std::uint64_t;
using CommandSequence = std::uint64_t;
using StableActorKey = std::uint64_t;
using StableContentKey = std::uint64_t;
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

struct CameraBasisQ15 final {
    GroundVectorQ15 screen_right_world{ground_axis_one, 0};
    GroundVectorQ15 screen_forward_world{0, ground_axis_one};
    std::uint32_t revision{1};

    [[nodiscard]] friend constexpr bool operator==(
        const CameraBasisQ15&,
        const CameraBasisQ15&
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

enum class SafePointRetryReason : std::uint8_t {
    player_defeated,
};

struct SafePointRetryCommand final {
    TickIndex completed_tick{};
    StableActorKey actor{};
    CommandSequence sequence{};
    SafePointRetryReason reason{SafePointRetryReason::player_defeated};
};

enum class EncounterActivationMode : std::uint8_t {
    replace,
    reinforce,
};

struct EncounterActivationCommand final {
    TickIndex completed_tick{};
    StableActorKey actor{};
    CommandSequence sequence{};
    EncounterActivationMode mode{EncounterActivationMode::replace};
};

struct SafePointCommitCommand final {
    TickIndex completed_tick{};
    StableActorKey actor{};
    CommandSequence sequence{};
    StableContentKey safe_point{};
    GroundPoseMm pose{};
};

}  // namespace tgd::contracts
