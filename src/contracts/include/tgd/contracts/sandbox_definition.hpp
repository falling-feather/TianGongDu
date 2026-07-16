#pragma once

#include <tgd/contracts/content_definition.hpp>

#include <cstdint>
#include <span>

namespace tgd::contracts {

struct SandboxBoundsMm final {
    std::int32_t min_x{};
    std::int32_t max_x{};
    std::int32_t min_y{};
    std::int32_t max_y{};
    std::int32_t min_height{};
    std::int32_t max_height{};
    std::int16_t min_floor_layer{};
    std::int16_t max_floor_layer{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxBoundsMm&,
        const SandboxBoundsMm&
    ) noexcept = default;
};

// Append-only: serialized values are part of .tgdsbx v1.
enum class SandboxAssetKind : std::uint8_t {
    player = 1,
    actor = 2,
    obstacle = 3,
    interaction = 4,
    mechanism = 5,
    safe_point = 6,
    effect = 7,
};

// The kind fixes the target_id domain. session_started is the only kind whose
// target_id must be empty.
enum class SandboxTriggerKind : std::uint8_t {
    session_started = 1,
    interaction_completed = 2,
    mechanism_activated = 3,
    objective_completed = 4,
    wave_completed = 5,
};

enum class SandboxObjectiveCompletionKind : std::uint8_t {
    interaction_completed = 1,
    mechanism_activated = 2,
    wave_completed = 3,
};

enum class SandboxReferenceDomain : std::uint8_t {
    none = 0,
    interaction = 1,
    mechanism = 2,
    objective = 3,
    wave = 4,
};

[[nodiscard]] constexpr SandboxReferenceDomain sandbox_trigger_reference_domain(
    SandboxTriggerKind kind
) noexcept {
    switch (kind) {
        case SandboxTriggerKind::session_started:
            return SandboxReferenceDomain::none;
        case SandboxTriggerKind::interaction_completed:
            return SandboxReferenceDomain::interaction;
        case SandboxTriggerKind::mechanism_activated:
            return SandboxReferenceDomain::mechanism;
        case SandboxTriggerKind::objective_completed:
            return SandboxReferenceDomain::objective;
        case SandboxTriggerKind::wave_completed:
            return SandboxReferenceDomain::wave;
    }
    return SandboxReferenceDomain::none;
}

[[nodiscard]] constexpr SandboxReferenceDomain sandbox_completion_reference_domain(
    SandboxObjectiveCompletionKind kind
) noexcept {
    switch (kind) {
        case SandboxObjectiveCompletionKind::interaction_completed:
            return SandboxReferenceDomain::interaction;
        case SandboxObjectiveCompletionKind::mechanism_activated:
            return SandboxReferenceDomain::mechanism;
        case SandboxObjectiveCompletionKind::wave_completed:
            return SandboxReferenceDomain::wave;
    }
    return SandboxReferenceDomain::none;
}

struct SandboxTriggerDefinition final {
    SandboxTriggerKind kind{SandboxTriggerKind::session_started};
    ContentId target_id{};
};

struct SandboxObjectiveCompletionDefinition final {
    SandboxObjectiveCompletionKind kind{
        SandboxObjectiveCompletionKind::interaction_completed
    };
    ContentId target_id{};
};

struct SandboxRegionDefinition final {
    ContentId id{};
    SandboxBoundsMm bounds{};
};

struct SandboxAssetReferenceDefinition final {
    ContentId id{};
    SandboxAssetKind kind{SandboxAssetKind::obstacle};
};

struct SandboxPlayerDefinition final {
    ContentId id{};
    ContentId region_id{};
    ContentId asset_id{};
    ContentId initial_safe_point_id{};
    GroundPoseMm pose{};
    std::uint32_t facing_millidegrees{};
};

struct SandboxActorDefinition final {
    ContentId id{};
    ContentId region_id{};
    ContentId asset_id{};
    GroundPoseMm pose{};
    std::uint32_t facing_millidegrees{};
};

struct SandboxGroundBlockerDefinition final {
    ContentId id{};
    ContentId region_id{};
    ContentId asset_id{};
    std::int32_t min_x{};
    std::int32_t max_x{};
    std::int32_t min_y{};
    std::int32_t max_y{};
    std::int32_t min_height{};
    std::int32_t max_height{};
    std::int16_t floor_layer{};
};

struct SandboxSafePointDefinition final {
    ContentId id{};
    ContentId region_id{};
    ContentId asset_id{};
    GroundPoseMm pose{};
    std::uint32_t facing_millidegrees{};
};

struct SandboxInteractionDefinition final {
    ContentId id{};
    ContentId region_id{};
    ContentId asset_id{};
    GroundPoseMm pose{};
    std::uint32_t facing_millidegrees{};
};

struct SandboxMechanismDefinition final {
    ContentId id{};
    ContentId region_id{};
    ContentId asset_id{};
    GroundPoseMm pose{};
    std::uint32_t facing_millidegrees{};
};

struct SandboxWaveDefinition final {
    ContentId id{};
    ContentId region_id{};
    // Empty means no predecessor. v1 permits at most one predecessor.
    ContentId predecessor_wave_id{};
    SandboxTriggerDefinition trigger{};
};

struct SandboxWaveSpawnDefinition final {
    ContentId wave_id{};
    ContentId actor_id{};
    std::uint32_t delay_ticks{};
    std::uint16_t spawn_order{};
};

struct SandboxObjectiveDefinition final {
    ContentId id{};
    ContentId region_id{};
    // Empty means no predecessor. v1 permits at most one predecessor.
    ContentId predecessor_objective_id{};
    SandboxObjectiveCompletionDefinition completion{};
};

// Immutable, non-owning package-core view. A future owning package document must
// retain every string and array referenced here for the full view lifetime. This
// contract does not provide that document, a decoder, a producer, or a Session.
struct SandboxDefinition final {
    ContentId package_id{};
    ContentId id{};
    SandboxBoundsMm bounds{};
    ContentId completion_objective_id{};
    SandboxPlayerDefinition player{};
    std::span<const SandboxRegionDefinition> regions{};
    std::span<const SandboxAssetReferenceDefinition> assets{};
    std::span<const SandboxActorDefinition> actors{};
    std::span<const SandboxGroundBlockerDefinition> ground_blockers{};
    std::span<const SandboxSafePointDefinition> safe_points{};
    std::span<const SandboxInteractionDefinition> interactions{};
    std::span<const SandboxMechanismDefinition> mechanisms{};
    std::span<const SandboxWaveDefinition> waves{};
    std::span<const SandboxWaveSpawnDefinition> wave_spawns{};
    std::span<const SandboxObjectiveDefinition> objectives{};
};

}  // namespace tgd::contracts
