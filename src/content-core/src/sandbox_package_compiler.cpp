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
    contracts::SandboxDefinition definition{};
    contracts::SandboxGameplayBindingDefinition gameplay_binding{};

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
        gameplay_binding = {interaction_bindings, mechanism_bindings};
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
        auto encoded = encode_sandbox_package(projected.definition, projected.gameplay_binding);
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
