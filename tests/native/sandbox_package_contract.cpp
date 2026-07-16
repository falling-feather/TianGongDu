#include <tgd/contracts/sandbox_definition.hpp>
#include <tgd/contracts/sandbox_pack.hpp>

#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace {

using namespace tgd::contracts;

template <typename Type>
concept HasGameplayProfile = requires(Type value) { value.gameplay_profile_id; };

template <typename Type>
concept HasLoadout = requires(Type value) { value.loadout; };

template <typename Type>
concept HasSkillIds = requires(Type value) { value.skill_ids; };

template <typename Type>
concept HasTacticalDuty = requires(Type value) { value.duty; };

template <typename Type>
concept HasAiDefinition = requires(Type value) { value.ai; };

template <typename Type>
concept HasMutableState = requires(Type value) { value.mutable_state; };

template <typename Type>
concept HasTelegraphAsset = requires(Type value) { value.telegraph_asset_id; };

template <typename Type>
concept HasHitFeedbackAsset = requires(Type value) { value.hit_feedback_asset_id; };

template <typename Type>
concept HasGameplayBindings = requires(Type value) { value.gameplay_bindings; };

static_assert(std::is_aggregate_v<SandboxDefinition>);
static_assert(std::is_aggregate_v<SandboxWaveDefinition>);
static_assert(std::is_aggregate_v<SandboxObjectiveDefinition>);

static_assert(!HasGameplayProfile<SandboxPlayerDefinition>);
static_assert(!HasGameplayProfile<SandboxActorDefinition>);
static_assert(!HasLoadout<SandboxPlayerDefinition>);
static_assert(!HasLoadout<SandboxActorDefinition>);
static_assert(!HasSkillIds<SandboxPlayerDefinition>);
static_assert(!HasSkillIds<SandboxActorDefinition>);
static_assert(!HasTacticalDuty<SandboxActorDefinition>);
static_assert(!HasAiDefinition<SandboxActorDefinition>);
static_assert(!HasMutableState<SandboxDefinition>);
static_assert(!HasTelegraphAsset<SandboxPlayerDefinition>);
static_assert(!HasTelegraphAsset<SandboxActorDefinition>);
static_assert(!HasHitFeedbackAsset<SandboxPlayerDefinition>);
static_assert(!HasHitFeedbackAsset<SandboxActorDefinition>);
static_assert(!HasGameplayBindings<SandboxDefinition>);

static_assert(sandbox_pack_magic[0] == 'T');
static_assert(sandbox_pack_magic[5] == 'X');
static_assert(sandbox_pack_magic[6] == 0);
static_assert(sandbox_pack_byte_order == SandboxPackByteOrder::little_endian);
static_assert(sandbox_pack_header_bytes == 96);
static_assert(sandbox_pack_directory_entry_bytes == 24);
static_assert(sandbox_pack_alignment_bytes == 8);
static_assert(sandbox_pack_hash_offset == 64);
static_assert(sandbox_pack_hash_bytes == 32);
static_assert(sandbox_pack_max_bytes == 4U * 1024U * 1024U);
static_assert(sandbox_pack_max_sections == 32);
static_assert(sandbox_pack_max_strings == 2'048);
static_assert(sandbox_pack_max_string_bytes == 256U * 1024U);
static_assert(sandbox_pack_max_id_bytes == 96);

static_assert(sandbox_player_capacity == 1);
static_assert(sandbox_region_capacity == 16);
static_assert(sandbox_asset_capacity == 128);
static_assert(sandbox_actor_capacity == 15);
static_assert(sandbox_ground_blocker_capacity == 64);
static_assert(sandbox_safe_point_capacity == 16);
static_assert(sandbox_interaction_capacity == 64);
static_assert(sandbox_mechanism_capacity == 16);
static_assert(sandbox_wave_capacity == 16);
static_assert(sandbox_wave_spawn_capacity == 15);
static_assert(sandbox_objective_capacity == 64);

static_assert(static_cast<std::uint8_t>(SandboxAssetKind::player) == 1);
static_assert(static_cast<std::uint8_t>(SandboxAssetKind::safe_point) == 6);
static_assert(static_cast<std::uint8_t>(SandboxAssetKind::effect) == 7);
static_assert(static_cast<std::uint8_t>(SandboxTriggerKind::session_started) == 1);
static_assert(static_cast<std::uint8_t>(SandboxTriggerKind::interaction_completed) == 2);
static_assert(static_cast<std::uint8_t>(SandboxTriggerKind::mechanism_activated) == 3);
static_assert(static_cast<std::uint8_t>(SandboxTriggerKind::objective_completed) == 4);
static_assert(static_cast<std::uint8_t>(SandboxTriggerKind::wave_completed) == 5);
static_assert(
    static_cast<std::uint8_t>(SandboxObjectiveCompletionKind::interaction_completed) == 1
);
static_assert(
    static_cast<std::uint8_t>(SandboxObjectiveCompletionKind::mechanism_activated) == 2
);
static_assert(static_cast<std::uint8_t>(SandboxObjectiveCompletionKind::wave_completed) == 3);
static_assert(static_cast<std::uint16_t>(SandboxPackSectionType::strings) == 1);
static_assert(static_cast<std::uint16_t>(SandboxPackSectionType::safe_points) == 8);
static_assert(static_cast<std::uint16_t>(SandboxPackSectionType::interactions) == 9);
static_assert(static_cast<std::uint16_t>(SandboxPackSectionType::mechanisms) == 10);
static_assert(static_cast<std::uint16_t>(SandboxPackSectionType::waves) == 11);
static_assert(static_cast<std::uint16_t>(SandboxPackSectionType::wave_spawns) == 12);
static_assert(static_cast<std::uint16_t>(SandboxPackSectionType::objectives) == 13);

static_assert(sandbox_pack_metadata_record_bytes == 64);
static_assert(sandbox_pack_region_record_bytes == 48);
static_assert(sandbox_pack_asset_record_bytes == 16);
static_assert(sandbox_pack_player_record_bytes == 72);
static_assert(sandbox_pack_actor_record_bytes == 64);
static_assert(sandbox_pack_ground_blocker_record_bytes == 64);
static_assert(sandbox_pack_safe_point_record_bytes == 64);
static_assert(sandbox_pack_interaction_record_bytes == 64);
static_assert(sandbox_pack_mechanism_record_bytes == 64);
static_assert(sandbox_pack_wave_record_bytes == 64);
static_assert(sandbox_pack_wave_spawn_record_bytes == 32);
static_assert(sandbox_pack_objective_record_bytes == 64);
static_assert(static_cast<std::uint16_t>(SandboxDiagnosticCode::duplicate_id) == 1);
static_assert(static_cast<std::uint16_t>(SandboxDiagnosticCode::retry_inconsistent) == 25);
static_assert(static_cast<std::uint16_t>(SandboxDiagnosticCode::web_budget_exceeded) == 32);

int failures = 0;

void expect(bool condition, std::string_view label) {
    if (!condition) {
        std::cerr << "FAIL: " << label << '\n';
        ++failures;
    }
}

void test_ground_pose_axes_are_explicit() {
    const GroundPoseMm pose{1'100, -2'200, 300, 4};
    expect(pose.x == 1'100, "GroundPose x is ground x in millimetres");
    expect(pose.y == -2'200, "GroundPose y is ground y in millimetres");
    expect(pose.height == 300, "GroundPose height is independent from ground y");
    expect(pose.floor_layer == 4, "GroundPose floor layer is explicit");
}

void test_typed_reference_domains() {
    expect(
        sandbox_trigger_reference_domain(SandboxTriggerKind::session_started) ==
            SandboxReferenceDomain::none,
        "session_started has no target"
    );
    expect(
        sandbox_trigger_reference_domain(SandboxTriggerKind::interaction_completed) ==
            SandboxReferenceDomain::interaction,
        "interaction trigger targets an interaction"
    );
    expect(
        sandbox_trigger_reference_domain(SandboxTriggerKind::mechanism_activated) ==
            SandboxReferenceDomain::mechanism,
        "mechanism trigger targets a mechanism"
    );
    expect(
        sandbox_trigger_reference_domain(SandboxTriggerKind::objective_completed) ==
            SandboxReferenceDomain::objective,
        "objective trigger targets an objective"
    );
    expect(
        sandbox_trigger_reference_domain(SandboxTriggerKind::wave_completed) ==
            SandboxReferenceDomain::wave,
        "wave trigger targets a wave"
    );
    expect(
        sandbox_completion_reference_domain(
            SandboxObjectiveCompletionKind::interaction_completed
        ) == SandboxReferenceDomain::interaction,
        "objective interaction completion is typed"
    );
    expect(
        sandbox_completion_reference_domain(
            SandboxObjectiveCompletionKind::mechanism_activated
        ) == SandboxReferenceDomain::mechanism,
        "objective mechanism completion is typed"
    );
    expect(
        sandbox_completion_reference_domain(SandboxObjectiveCompletionKind::wave_completed) ==
            SandboxReferenceDomain::wave,
        "objective wave completion is typed"
    );
}

void test_single_predecessor_and_tick_delay_shape() {
    const SandboxWaveDefinition wave{
        content_id("wave.second"),
        content_id("region.arena"),
        content_id("wave.first"),
        {SandboxTriggerKind::wave_completed, content_id("wave.first")},
    };
    const SandboxWaveSpawnDefinition spawn{
        wave.id,
        content_id("actor.flank"),
        30,
        1,
    };
    const SandboxObjectiveDefinition objective{
        content_id("objective.clear"),
        content_id("region.arena"),
        content_id("objective.activate"),
        {SandboxObjectiveCompletionKind::wave_completed, wave.id},
    };

    expect(wave.predecessor_wave_id == content_id("wave.first"), "wave has one predecessor");
    expect(spawn.delay_ticks == 30, "wave spawn delay is expressed in ticks");
    expect(spawn.wave_id == wave.id, "wave spawn has an explicit typed wave reference");
    expect(
        objective.predecessor_objective_id == content_id("objective.activate"),
        "objective has one predecessor"
    );
    expect(objective.completion.target_id == wave.id, "objective completion target is explicit");
}

}  // namespace

int main() {
    test_ground_pose_axes_are_explicit();
    test_typed_reference_domains();
    test_single_predecessor_and_tick_delay_shape();

    if (failures != 0) {
        std::cerr << failures << " sandbox package-core contract checks failed\n";
        return 1;
    }
    std::cout << "sandbox package-core contract checks passed\n";
    return 0;
}
