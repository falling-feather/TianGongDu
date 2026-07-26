#pragma once

#include <tgd/content/sandbox_package.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace tgd::content {

struct SandboxAuthoringRegion final {
    std::string_view id{};
    contracts::SandboxBoundsMm bounds{};
};

struct SandboxAuthoringAsset final {
    std::string_view id{};
    contracts::SandboxAssetKind kind{contracts::SandboxAssetKind::obstacle};
};

struct SandboxAuthoringPlayer final {
    std::string_view id{};
    std::string_view region_id{};
    std::string_view asset_id{};
    std::string_view initial_safe_point_id{};
    contracts::GroundPoseMm pose{};
    std::uint32_t facing_millidegrees{};
};

// Actor, safe-point, interaction, and mechanism authoring records share this
// placement-only shape. Their containing span fixes the target domain.
struct SandboxAuthoringPlacement final {
    std::string_view id{};
    std::string_view region_id{};
    std::string_view asset_id{};
    contracts::GroundPoseMm pose{};
    std::uint32_t facing_millidegrees{};
};

struct SandboxAuthoringGroundBlocker final {
    std::string_view id{};
    std::string_view region_id{};
    std::string_view asset_id{};
    std::int32_t min_x{};
    std::int32_t max_x{};
    std::int32_t min_y{};
    std::int32_t max_y{};
    std::int32_t min_height{};
    std::int32_t max_height{};
    std::int16_t floor_layer{};
};

struct SandboxAuthoringTrigger final {
    contracts::SandboxTriggerKind kind{contracts::SandboxTriggerKind::session_started};
    std::string_view target_id{};
};

struct SandboxAuthoringWave final {
    std::string_view id{};
    std::string_view region_id{};
    std::string_view predecessor_wave_id{};
    SandboxAuthoringTrigger trigger{};
};

struct SandboxAuthoringWaveSpawn final {
    std::string_view wave_id{};
    std::string_view actor_id{};
    std::uint32_t delay_ticks{};
    std::uint16_t spawn_order{};
};

struct SandboxAuthoringObjectiveCompletion final {
    contracts::SandboxObjectiveCompletionKind kind{
        contracts::SandboxObjectiveCompletionKind::interaction_completed
    };
    std::string_view target_id{};
};

struct SandboxAuthoringObjective final {
    std::string_view id{};
    std::string_view region_id{};
    std::string_view predecessor_objective_id{};
    SandboxAuthoringObjectiveCompletion completion{};
};

struct SandboxAuthoringInteractionBinding final {
    std::string_view interaction_id{};
    contracts::SandboxInteractionOperation operation{
        contracts::SandboxInteractionOperation::invalid
    };
    std::int32_t range_mm{};
    std::string_view target_mechanism_id{};
};

struct SandboxAuthoringMechanismBinding final {
    std::string_view mechanism_id{};
    contracts::SandboxMechanismActivation activation{
        contracts::SandboxMechanismActivation::invalid
    };
    std::string_view target_ground_blocker_id{};
};

struct SandboxAuthoringActorBinding final {
    std::string_view actor_id{};
    std::string_view profile_id{};
    contracts::CombatFaction faction{contracts::CombatFaction::neutral};
    contracts::EncounterTacticalDuty duty{
        contracts::EncounterTacticalDuty::pressure
    };
    std::int32_t max_health{};
};

struct SandboxAuthoringCraftMaterial final {
    std::string_view id{};
};

struct SandboxAuthoringCraftWorkstation final {
    std::string_view id{};
    std::string_view region_id{};
    std::string_view asset_id{};
    contracts::GroundPoseMm pose{};
    std::uint32_t facing_millidegrees{};
};

struct SandboxAuthoringCraftProcess final {
    std::string_view id{};
    std::string_view workstation_id{};
    std::string_view need_id{};
    std::string_view output_item_id{};
    std::string_view trial_step_id{};
};

struct SandboxAuthoringCraftMaterialChoice final {
    std::string_view process_id{};
    std::string_view material_id{};
    contracts::CraftMaterialOutcome outcome{
        contracts::CraftMaterialOutcome::invalid
    };
};

struct SandboxAuthoringCraftStep final {
    std::string_view id{};
    std::string_view process_id{};
    std::string_view predecessor_step_id{};
    std::string_view action_id{};
    contracts::CraftStepKind kind{contracts::CraftStepKind::invalid};
};

struct SandboxAuthoringWorkshop final {
    std::string_view id{};
    std::string_view workstation_id{};
    std::int32_t initial_funds{};
};

struct SandboxAuthoringWorkshopMaterialStock final {
    std::string_view workshop_id{};
    std::string_view material_id{};
    std::int32_t unit_cost{};
    std::uint16_t initial_quantity{};
    std::uint16_t base_quality{};
    std::uint16_t rework_quality_gain{};
};

struct SandboxAuthoringWorkshopOrder final {
    std::string_view id{};
    std::string_view workshop_id{};
    std::string_view process_id{};
    std::string_view consequence_target_id{};
    std::uint16_t required_quantity{};
    std::uint16_t minimum_quality{};
    std::int32_t reward_funds{};
    contracts::WorkshopOrderConsequenceKind consequence_kind{
        contracts::WorkshopOrderConsequenceKind::invalid
    };
};

// Synchronous, non-owning projection of the normalized authoring runtime domain.
// The caller retains every string and array until compile_sandbox_package returns.
// JSON parsing and closed-shape validation remain owned by Content Workbench.
struct SandboxAuthoringRuntimeView final {
    std::string_view package_id{};
    std::string_view sandbox_id{};
    contracts::SandboxBoundsMm bounds{};
    std::string_view completion_objective_id{};
    SandboxAuthoringPlayer player{};
    std::span<const SandboxAuthoringRegion> regions{};
    std::span<const SandboxAuthoringAsset> assets{};
    std::span<const SandboxAuthoringPlacement> actors{};
    std::span<const SandboxAuthoringGroundBlocker> ground_blockers{};
    std::span<const SandboxAuthoringPlacement> safe_points{};
    std::span<const SandboxAuthoringPlacement> interactions{};
    std::span<const SandboxAuthoringPlacement> mechanisms{};
    std::span<const SandboxAuthoringWave> waves{};
    std::span<const SandboxAuthoringWaveSpawn> wave_spawns{};
    std::span<const SandboxAuthoringObjective> objectives{};
    std::span<const SandboxAuthoringInteractionBinding> interaction_bindings{};
    std::span<const SandboxAuthoringMechanismBinding> mechanism_bindings{};
    std::span<const SandboxAuthoringActorBinding> actor_bindings{};
    std::span<const SandboxAuthoringCraftMaterial> craft_materials{};
    std::span<const SandboxAuthoringCraftWorkstation> craft_workstations{};
    std::span<const SandboxAuthoringCraftProcess> craft_processes{};
    std::span<const SandboxAuthoringCraftMaterialChoice> craft_material_choices{};
    std::span<const SandboxAuthoringCraftStep> craft_steps{};
    std::span<const SandboxAuthoringWorkshop> workshops{};
    std::span<const SandboxAuthoringWorkshopMaterialStock>
        workshop_material_stocks{};
    std::span<const SandboxAuthoringWorkshopOrder> workshop_orders{};
};

enum class SandboxPackageCompileStatus : std::uint8_t {
    succeeded = 1,
    producer_rejected = 2,
    decoder_rejected = 3,
    fingerprint_mismatch = 4,
    invalid = 255,
};

class SandboxPackageCompileResult;

class SandboxPackageCandidate final {
  public:
    SandboxPackageCandidate(const SandboxPackageCandidate&) = delete;
    SandboxPackageCandidate& operator=(const SandboxPackageCandidate&) = delete;
    SandboxPackageCandidate(SandboxPackageCandidate&&) noexcept = default;
    SandboxPackageCandidate& operator=(SandboxPackageCandidate&&) noexcept = default;

    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept;
    [[nodiscard]] const SandboxPackageDocument& document() const noexcept;
    [[nodiscard]] const contracts::Sha256Digest& fingerprint() const noexcept;

  private:
    SandboxPackageCandidate(
        std::vector<std::uint8_t> bytes,
        std::unique_ptr<SandboxPackageDocument> document
    ) noexcept;

    std::vector<std::uint8_t> bytes_{};
    std::unique_ptr<SandboxPackageDocument> document_{};

    friend class SandboxPackageCompileResult;
    friend SandboxPackageCompileResult compile_sandbox_package(
        const SandboxAuthoringRuntimeView& runtime
    ) noexcept;
};

class SandboxPackageCompileResult final {
  public:
    SandboxPackageCompileResult(const SandboxPackageCompileResult&) = delete;
    SandboxPackageCompileResult& operator=(const SandboxPackageCompileResult&) = delete;
    SandboxPackageCompileResult(SandboxPackageCompileResult&& other) noexcept;
    SandboxPackageCompileResult& operator=(SandboxPackageCompileResult&& other) noexcept;

    [[nodiscard]] SandboxPackageCompileStatus status() const noexcept;
    [[nodiscard]] bool succeeded() const noexcept;
    [[nodiscard]] const SandboxPackageValidation& validation() const noexcept;
    [[nodiscard]] const SandboxPackageCandidate* candidate() const noexcept;
    [[nodiscard]] std::unique_ptr<SandboxPackageCandidate> take_candidate() && noexcept;

  private:
    SandboxPackageCompileResult(
        SandboxPackageCompileStatus status,
        SandboxPackageValidation validation,
        std::unique_ptr<SandboxPackageCandidate> candidate
    ) noexcept;

    SandboxPackageCompileStatus status_{SandboxPackageCompileStatus::invalid};
    SandboxPackageValidation validation_{};
    std::unique_ptr<SandboxPackageCandidate> candidate_{};

    friend SandboxPackageCompileResult compile_sandbox_package(
        const SandboxAuthoringRuntimeView& runtime
    ) noexcept;
};

// Generates Stable keys only in C++, then calls the sole existing producer and
// decoder. A failure never exposes canonical bytes, a document, or a partial
// candidate. A successful candidate owns both bytes and every returned view.
[[nodiscard]] SandboxPackageCompileResult compile_sandbox_package(
    const SandboxAuthoringRuntimeView& runtime
) noexcept;

}  // namespace tgd::content
