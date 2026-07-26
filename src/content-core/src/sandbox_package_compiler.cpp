#include <tgd/content/sandbox_package_compiler.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace tgd::content {
namespace {

[[nodiscard]] constexpr contracts::ContentId authored_id(std::string_view name) noexcept {
    return name.empty() ? contracts::ContentId{} : contracts::content_id(name);
}

template <typename Output, typename Input, typename Project>
[[nodiscard]] std::vector<Output> project_records(
    std::span<const Input> input,
    std::size_t capacity,
    Project project
) {
    std::vector<Output> output;
    const auto projected_size = std::min(input.size(), capacity + 1U);
    output.reserve(projected_size);
    for (std::size_t index = 0; index < projected_size; ++index) {
        output.push_back(project(input[index]));
    }
    return output;
}

struct ProjectedPackage final {
    std::vector<contracts::SandboxRegionDefinition> regions{};
    std::vector<contracts::SandboxAssetReferenceDefinition> assets{};
    std::vector<contracts::SandboxActorDefinition> actors{};
    std::vector<contracts::SandboxGroundBlockerDefinition> ground_blockers{};
    std::vector<contracts::SandboxSafePointDefinition> safe_points{};
    std::vector<contracts::SandboxInteractionDefinition> interactions{};
    std::vector<contracts::SandboxMechanismDefinition> mechanisms{};
    std::vector<contracts::SandboxWaveDefinition> waves{};
    std::vector<contracts::SandboxWaveSpawnDefinition> wave_spawns{};
    std::vector<contracts::SandboxObjectiveDefinition> objectives{};
    std::vector<contracts::SandboxInteractionGameplayBinding> interaction_bindings{};
    std::vector<contracts::SandboxMechanismGameplayBinding> mechanism_bindings{};
    std::vector<contracts::SandboxActorGameplayBinding> actor_bindings{};
    std::vector<contracts::CraftMaterialDefinition> craft_materials{};
    std::vector<contracts::CraftWorkstationDefinition> craft_workstations{};
    std::vector<contracts::CraftProcessDefinition> craft_processes{};
    std::vector<contracts::CraftMaterialChoiceDefinition> craft_material_choices{};
    std::vector<contracts::CraftStepDefinition> craft_steps{};
    contracts::SandboxDefinition definition{};
    contracts::SandboxGameplayBindingDefinition gameplay_binding{};
    contracts::CraftDefinition craft_definition{};

    explicit ProjectedPackage(const SandboxAuthoringRuntimeView& runtime)
        : regions(project_records<contracts::SandboxRegionDefinition>(
              runtime.regions,
              contracts::sandbox_region_capacity,
              [](const auto& value) {
                  return contracts::SandboxRegionDefinition{
                      authored_id(value.id), value.bounds
                  };
              }
          )),
          assets(project_records<contracts::SandboxAssetReferenceDefinition>(
              runtime.assets,
              contracts::sandbox_asset_capacity,
              [](const auto& value) {
                  return contracts::SandboxAssetReferenceDefinition{
                      authored_id(value.id), value.kind
                  };
              }
          )),
          actors(project_records<contracts::SandboxActorDefinition>(
              runtime.actors,
              contracts::sandbox_actor_capacity,
              [](const auto& value) {
                  return contracts::SandboxActorDefinition{
                      authored_id(value.id),
                      authored_id(value.region_id),
                      authored_id(value.asset_id),
                      value.pose,
                      value.facing_millidegrees,
                  };
              }
          )),
          ground_blockers(project_records<contracts::SandboxGroundBlockerDefinition>(
              runtime.ground_blockers,
              contracts::sandbox_ground_blocker_capacity,
              [](const auto& value) {
                  return contracts::SandboxGroundBlockerDefinition{
                      authored_id(value.id),
                      authored_id(value.region_id),
                      authored_id(value.asset_id),
                      value.min_x,
                      value.max_x,
                      value.min_y,
                      value.max_y,
                      value.min_height,
                      value.max_height,
                      value.floor_layer,
                  };
              }
          )),
          safe_points(project_records<contracts::SandboxSafePointDefinition>(
              runtime.safe_points,
              contracts::sandbox_safe_point_capacity,
              [](const auto& value) {
                  return contracts::SandboxSafePointDefinition{
                      authored_id(value.id),
                      authored_id(value.region_id),
                      authored_id(value.asset_id),
                      value.pose,
                      value.facing_millidegrees,
                  };
              }
          )),
          interactions(project_records<contracts::SandboxInteractionDefinition>(
              runtime.interactions,
              contracts::sandbox_interaction_capacity,
              [](const auto& value) {
                  return contracts::SandboxInteractionDefinition{
                      authored_id(value.id),
                      authored_id(value.region_id),
                      authored_id(value.asset_id),
                      value.pose,
                      value.facing_millidegrees,
                  };
              }
          )),
          mechanisms(project_records<contracts::SandboxMechanismDefinition>(
              runtime.mechanisms,
              contracts::sandbox_mechanism_capacity,
              [](const auto& value) {
                  return contracts::SandboxMechanismDefinition{
                      authored_id(value.id),
                      authored_id(value.region_id),
                      authored_id(value.asset_id),
                      value.pose,
                      value.facing_millidegrees,
                  };
              }
          )),
          waves(project_records<contracts::SandboxWaveDefinition>(
              runtime.waves,
              contracts::sandbox_wave_capacity,
              [](const auto& value) {
                  return contracts::SandboxWaveDefinition{
                      authored_id(value.id),
                      authored_id(value.region_id),
                      authored_id(value.predecessor_wave_id),
                      {value.trigger.kind, authored_id(value.trigger.target_id)},
                  };
              }
          )),
          wave_spawns(project_records<contracts::SandboxWaveSpawnDefinition>(
              runtime.wave_spawns,
              contracts::sandbox_wave_spawn_capacity,
              [](const auto& value) {
                  return contracts::SandboxWaveSpawnDefinition{
                      authored_id(value.wave_id),
                      authored_id(value.actor_id),
                      value.delay_ticks,
                      value.spawn_order,
                  };
              }
          )),
          objectives(project_records<contracts::SandboxObjectiveDefinition>(
              runtime.objectives,
              contracts::sandbox_objective_capacity,
              [](const auto& value) {
                  return contracts::SandboxObjectiveDefinition{
                      authored_id(value.id),
                      authored_id(value.region_id),
                      authored_id(value.predecessor_objective_id),
                      {value.completion.kind, authored_id(value.completion.target_id)},
                  };
              }
          )),
          interaction_bindings(
              project_records<contracts::SandboxInteractionGameplayBinding>(
                  runtime.interaction_bindings,
                  contracts::sandbox_interaction_capacity,
                  [](const auto& value) {
                      return contracts::SandboxInteractionGameplayBinding{
                          authored_id(value.interaction_id),
                          value.operation,
                          value.range_mm,
                          authored_id(value.target_mechanism_id),
                      };
                  }
              )
          ),
          mechanism_bindings(project_records<contracts::SandboxMechanismGameplayBinding>(
              runtime.mechanism_bindings,
              contracts::sandbox_mechanism_capacity,
              [](const auto& value) {
                  return contracts::SandboxMechanismGameplayBinding{
                      authored_id(value.mechanism_id),
                      value.activation,
                      authored_id(value.target_ground_blocker_id),
                  };
              }
          )),
          actor_bindings(project_records<contracts::SandboxActorGameplayBinding>(
              runtime.actor_bindings,
              contracts::sandbox_actor_capacity,
              [](const auto& value) {
                  return contracts::SandboxActorGameplayBinding{
                      authored_id(value.actor_id),
                      authored_id(value.profile_id),
                      value.faction,
                      value.duty,
                      value.max_health,
                  };
              }
          )),
          craft_materials(project_records<contracts::CraftMaterialDefinition>(
              runtime.craft_materials,
              contracts::sandbox_craft_material_capacity,
              [](const auto& value) {
                  return contracts::CraftMaterialDefinition{authored_id(value.id)};
              }
          )),
          craft_workstations(project_records<contracts::CraftWorkstationDefinition>(
              runtime.craft_workstations,
              contracts::sandbox_craft_workstation_capacity,
              [](const auto& value) {
                  return contracts::CraftWorkstationDefinition{
                      authored_id(value.id),
                      authored_id(value.region_id),
                      authored_id(value.asset_id),
                      value.pose,
                      value.facing_millidegrees,
                  };
              }
          )),
          craft_processes(project_records<contracts::CraftProcessDefinition>(
              runtime.craft_processes,
              contracts::sandbox_craft_process_capacity,
              [](const auto& value) {
                  return contracts::CraftProcessDefinition{
                      authored_id(value.id),
                      authored_id(value.workstation_id),
                      authored_id(value.need_id),
                      authored_id(value.output_item_id),
                      authored_id(value.trial_step_id),
                  };
              }
          )),
          craft_material_choices(
              project_records<contracts::CraftMaterialChoiceDefinition>(
                  runtime.craft_material_choices,
                  contracts::sandbox_craft_material_choice_capacity,
                  [](const auto& value) {
                      return contracts::CraftMaterialChoiceDefinition{
                          authored_id(value.process_id),
                          authored_id(value.material_id),
                          value.outcome,
                      };
                  }
              )
          ),
          craft_steps(project_records<contracts::CraftStepDefinition>(
              runtime.craft_steps,
              contracts::sandbox_craft_step_capacity,
              [](const auto& value) {
                  return contracts::CraftStepDefinition{
                      authored_id(value.id),
                      authored_id(value.process_id),
                      authored_id(value.predecessor_step_id),
                      authored_id(value.action_id),
                      value.kind,
                  };
              }
          )) {
        definition = {
            authored_id(runtime.package_id),
            authored_id(runtime.sandbox_id),
            runtime.bounds,
            authored_id(runtime.completion_objective_id),
            {
                authored_id(runtime.player.id),
                authored_id(runtime.player.region_id),
                authored_id(runtime.player.asset_id),
                authored_id(runtime.player.initial_safe_point_id),
                runtime.player.pose,
                runtime.player.facing_millidegrees,
            },
            regions,
            assets,
            actors,
            ground_blockers,
            safe_points,
            interactions,
            mechanisms,
            waves,
            wave_spawns,
            objectives,
        };
        gameplay_binding = {
            interaction_bindings,
            mechanism_bindings,
            actor_bindings,
        };
        craft_definition = {
            craft_materials,
            craft_workstations,
            craft_processes,
            craft_material_choices,
            craft_steps,
        };
    }
};

[[nodiscard]] SandboxPackageValidation allocation_failure() noexcept {
    SandboxPackageValidation validation;
    validation.error = contracts::SandboxPackageError::allocation_failed;
    return validation;
}

}  // namespace

SandboxPackageCandidate::SandboxPackageCandidate(
    std::vector<std::uint8_t> bytes,
    std::unique_ptr<SandboxPackageDocument> document
) noexcept
    : bytes_(std::move(bytes)), document_(std::move(document)) {}

std::span<const std::uint8_t> SandboxPackageCandidate::bytes() const noexcept {
    return bytes_;
}

const SandboxPackageDocument& SandboxPackageCandidate::document() const noexcept {
    return *document_;
}

const contracts::Sha256Digest& SandboxPackageCandidate::fingerprint() const noexcept {
    return document_->fingerprint();
}

SandboxPackageCompileResult::SandboxPackageCompileResult(
    SandboxPackageCompileStatus status,
    SandboxPackageValidation validation,
    std::unique_ptr<SandboxPackageCandidate> candidate
) noexcept
    : status_(status),
      validation_(std::move(validation)),
      candidate_(std::move(candidate)) {}

SandboxPackageCompileResult::SandboxPackageCompileResult(
    SandboxPackageCompileResult&& other
) noexcept
    : status_(std::exchange(other.status_, SandboxPackageCompileStatus::invalid)),
      validation_(std::move(other.validation_)),
      candidate_(std::move(other.candidate_)) {}

SandboxPackageCompileResult& SandboxPackageCompileResult::operator=(
    SandboxPackageCompileResult&& other
) noexcept {
    if (this != &other) {
        status_ = std::exchange(other.status_, SandboxPackageCompileStatus::invalid);
        validation_ = std::move(other.validation_);
        candidate_ = std::move(other.candidate_);
    }
    return *this;
}

SandboxPackageCompileStatus SandboxPackageCompileResult::status() const noexcept {
    return status_;
}

bool SandboxPackageCompileResult::succeeded() const noexcept {
    return status_ == SandboxPackageCompileStatus::succeeded && candidate_ != nullptr;
}

const SandboxPackageValidation& SandboxPackageCompileResult::validation() const noexcept {
    return validation_;
}

const SandboxPackageCandidate* SandboxPackageCompileResult::candidate() const noexcept {
    return candidate_.get();
}

std::unique_ptr<SandboxPackageCandidate> SandboxPackageCompileResult::take_candidate() &&
    noexcept {
    status_ = SandboxPackageCompileStatus::invalid;
    return std::move(candidate_);
}

SandboxPackageCompileResult compile_sandbox_package(
    const SandboxAuthoringRuntimeView& runtime
) noexcept {
    try {
        const ProjectedPackage projected{runtime};
        auto encoded = encode_sandbox_package(
            projected.definition,
            projected.gameplay_binding,
            projected.craft_definition
        );
        if (!encoded.validation.valid()) {
            return {
                SandboxPackageCompileStatus::producer_rejected,
                std::move(encoded.validation),
                nullptr,
            };
        }

        auto decoded = decode_sandbox_package(encoded.bytes);
        if (!decoded.validation.valid() || decoded.document == nullptr) {
            return {
                SandboxPackageCompileStatus::decoder_rejected,
                std::move(decoded.validation),
                nullptr,
            };
        }
        if (decoded.document->fingerprint() != encoded.fingerprint) {
            return {
                SandboxPackageCompileStatus::fingerprint_mismatch,
                std::move(decoded.validation),
                nullptr,
            };
        }

        auto candidate = std::unique_ptr<SandboxPackageCandidate>(
            new SandboxPackageCandidate{
                std::move(encoded.bytes), std::move(decoded.document)
            }
        );
        return {
            SandboxPackageCompileStatus::succeeded,
            std::move(decoded.validation),
            std::move(candidate),
        };
    } catch (const std::bad_alloc&) {
        return {
            SandboxPackageCompileStatus::producer_rejected,
            allocation_failure(),
            nullptr,
        };
    }
}

}  // namespace tgd::content
