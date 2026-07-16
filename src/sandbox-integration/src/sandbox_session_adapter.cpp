#include <tgd/integration/sandbox_session_adapter.hpp>

#include <tgd/content/sandbox_package.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <utility>

namespace tgd::integration {

SandboxSessionBlueprint::SandboxSessionBlueprint(
    SandboxSessionBlueprint&& other
) noexcept {
    *this = std::move(other);
}

SandboxSessionBlueprint& SandboxSessionBlueprint::operator=(
    SandboxSessionBlueprint&& other
) noexcept {
    if (this != &other) {
        player_ = std::move(other.player_);
        safe_points_ = std::move(other.safe_points_);
        interactions_ = std::move(other.interactions_);
        mechanisms_ = std::move(other.mechanisms_);
        ground_blockers_ = std::move(other.ground_blockers_);
        interaction_bindings_ = std::move(other.interaction_bindings_);
        mechanism_bindings_ = std::move(other.mechanism_bindings_);
        safe_point_count_ = std::exchange(other.safe_point_count_, 0);
        interaction_count_ = std::exchange(other.interaction_count_, 0);
        mechanism_count_ = std::exchange(other.mechanism_count_, 0);
        ground_blocker_count_ = std::exchange(other.ground_blocker_count_, 0);
        interaction_binding_count_ =
            std::exchange(other.interaction_binding_count_, 0);
        mechanism_binding_count_ =
            std::exchange(other.mechanism_binding_count_, 0);
        valid_ = std::exchange(other.valid_, false);
    }
    return *this;
}

SandboxSessionBlueprintBuildError SandboxSessionBlueprint::copy_content_id(
    OwnedContentId& destination,
    contracts::ContentId source
) noexcept {
    if (source.name.size() > id_byte_capacity) {
        return SandboxSessionBlueprintBuildError::id_byte_capacity_exceeded;
    }
    if (source.key == 0 || source.name.empty() ||
        contracts::stable_content_key(source.name) != source.key) {
        return SandboxSessionBlueprintBuildError::invalid_owned_content_id;
    }
    destination = {};
    destination.key = source.key;
    destination.length = static_cast<std::uint16_t>(source.name.size());
    std::copy(source.name.begin(), source.name.end(), destination.bytes.begin());
    return SandboxSessionBlueprintBuildError::none;
}

contracts::ContentId SandboxSessionBlueprint::content_id_view(
    const OwnedContentId& source
) noexcept {
    return {source.key, std::string_view{source.bytes.data(), source.length}};
}

SandboxSessionBlueprintBuildResult::SandboxSessionBlueprintBuildResult(
    SandboxSessionBlueprintBuildError error,
    std::optional<SandboxSessionBlueprint> blueprint
) noexcept : error_(error), blueprint_(std::move(blueprint)) {}

SandboxSessionBlueprintBuildResult::SandboxSessionBlueprintBuildResult(
    SandboxSessionBlueprintBuildResult&& other
) noexcept
    : error_(std::exchange(other.error_, SandboxSessionBlueprintBuildError::invalid)),
      blueprint_(std::move(other.blueprint_)) {
    other.blueprint_.reset();
}

SandboxSessionBlueprintBuildResult& SandboxSessionBlueprintBuildResult::operator=(
    SandboxSessionBlueprintBuildResult&& other
) noexcept {
    if (this != &other) {
        error_ = std::exchange(other.error_, SandboxSessionBlueprintBuildError::invalid);
        blueprint_ = std::move(other.blueprint_);
        other.blueprint_.reset();
    }
    return *this;
}

SandboxSessionBlueprintBuildError SandboxSessionBlueprintBuildResult::error()
    const noexcept {
    return error_;
}

bool SandboxSessionBlueprintBuildResult::succeeded() const noexcept {
    return error_ == SandboxSessionBlueprintBuildError::none &&
           blueprint_.has_value() && blueprint_->valid_;
}

const SandboxSessionBlueprint* SandboxSessionBlueprintBuildResult::blueprint()
    const noexcept {
    return succeeded() ? &*blueprint_ : nullptr;
}

std::optional<SandboxSessionBlueprint>
SandboxSessionBlueprintBuildResult::take_blueprint() && noexcept {
    std::optional<SandboxSessionBlueprint> result{};
    if (succeeded()) {
        result.emplace(std::move(*blueprint_));
    }
    error_ = SandboxSessionBlueprintBuildError::invalid;
    blueprint_.reset();
    return result;
}

SandboxSessionBlueprintBuildResult build_sandbox_session_blueprint(
    const content::SandboxPackageDocument& document
) noexcept {
    using Error = SandboxSessionBlueprintBuildError;
    const auto& core = document.definition();
    const auto& binding = document.gameplay_binding();
    if (core.safe_points.size() > SandboxSessionBlueprint::safe_point_capacity ||
        core.interactions.size() > SandboxSessionBlueprint::interaction_capacity ||
        core.mechanisms.size() > SandboxSessionBlueprint::mechanism_capacity ||
        core.ground_blockers.size() >
            SandboxSessionBlueprint::ground_blocker_capacity ||
        binding.interaction_bindings.size() >
            SandboxSessionBlueprint::interaction_capacity ||
        binding.mechanism_bindings.size() >
            SandboxSessionBlueprint::mechanism_capacity) {
        return {Error::capacity_exceeded, std::nullopt};
    }

    SandboxSessionBlueprint blueprint{};
    Error copy_error = Error::none;
    const auto copy_id = [&copy_error](
                             SandboxSessionBlueprint::OwnedContentId& destination,
                             contracts::ContentId source
                         ) noexcept {
        if (copy_error == Error::none) {
            copy_error = SandboxSessionBlueprint::copy_content_id(destination, source);
        }
    };

    copy_id(blueprint.player_.id, core.player.id);
    copy_id(blueprint.player_.region_id, core.player.region_id);
    copy_id(
        blueprint.player_.initial_safe_point_id,
        core.player.initial_safe_point_id
    );
    blueprint.player_.pose = core.player.pose;
    blueprint.player_.facing_millidegrees = core.player.facing_millidegrees;

    blueprint.safe_point_count_ = core.safe_points.size();
    for (std::size_t index = 0; index < blueprint.safe_point_count_; ++index) {
        const auto& source = core.safe_points[index];
        auto& destination = blueprint.safe_points_[index];
        copy_id(destination.id, source.id);
        copy_id(destination.region_id, source.region_id);
        destination.pose = source.pose;
        destination.facing_millidegrees = source.facing_millidegrees;
    }
    blueprint.interaction_count_ = core.interactions.size();
    for (std::size_t index = 0; index < blueprint.interaction_count_; ++index) {
        copy_id(blueprint.interactions_[index].id, core.interactions[index].id);
        blueprint.interactions_[index].pose = core.interactions[index].pose;
    }
    blueprint.mechanism_count_ = core.mechanisms.size();
    for (std::size_t index = 0; index < blueprint.mechanism_count_; ++index) {
        copy_id(blueprint.mechanisms_[index].id, core.mechanisms[index].id);
    }
    blueprint.ground_blocker_count_ = core.ground_blockers.size();
    for (std::size_t index = 0; index < blueprint.ground_blocker_count_; ++index) {
        copy_id(
            blueprint.ground_blockers_[index].id,
            core.ground_blockers[index].id
        );
    }
    blueprint.interaction_binding_count_ = binding.interaction_bindings.size();
    for (std::size_t index = 0; index < blueprint.interaction_binding_count_; ++index) {
        const auto& source = binding.interaction_bindings[index];
        auto& destination = blueprint.interaction_bindings_[index];
        copy_id(destination.interaction_id, source.interaction_id);
        destination.operation = source.operation;
        destination.range_mm = source.range_mm;
        copy_id(destination.target_mechanism_id, source.target_mechanism_id);
    }
    blueprint.mechanism_binding_count_ = binding.mechanism_bindings.size();
    for (std::size_t index = 0; index < blueprint.mechanism_binding_count_; ++index) {
        const auto& source = binding.mechanism_bindings[index];
        auto& destination = blueprint.mechanism_bindings_[index];
        copy_id(destination.mechanism_id, source.mechanism_id);
        destination.activation = source.activation;
        copy_id(
            destination.target_ground_blocker_id,
            source.target_ground_blocker_id
        );
    }
    if (copy_error != Error::none) {
        return {copy_error, std::nullopt};
    }

    std::sort(
        blueprint.safe_points_.begin(),
        blueprint.safe_points_.begin() + blueprint.safe_point_count_,
        [](const auto& left, const auto& right) { return left.id.key < right.id.key; }
    );
    std::sort(
        blueprint.interactions_.begin(),
        blueprint.interactions_.begin() + blueprint.interaction_count_,
        [](const auto& left, const auto& right) { return left.id.key < right.id.key; }
    );
    std::sort(
        blueprint.mechanisms_.begin(),
        blueprint.mechanisms_.begin() + blueprint.mechanism_count_,
        [](const auto& left, const auto& right) { return left.id.key < right.id.key; }
    );
    std::sort(
        blueprint.ground_blockers_.begin(),
        blueprint.ground_blockers_.begin() + blueprint.ground_blocker_count_,
        [](const auto& left, const auto& right) { return left.id.key < right.id.key; }
    );
    std::sort(
        blueprint.interaction_bindings_.begin(),
        blueprint.interaction_bindings_.begin() +
            blueprint.interaction_binding_count_,
        [](const auto& left, const auto& right) {
            return left.interaction_id.key < right.interaction_id.key;
        }
    );
    std::sort(
        blueprint.mechanism_bindings_.begin(),
        blueprint.mechanism_bindings_.begin() + blueprint.mechanism_binding_count_,
        [](const auto& left, const auto& right) {
            return left.mechanism_id.key < right.mechanism_id.key;
        }
    );
    blueprint.valid_ = true;
    return {Error::none, std::optional<SandboxSessionBlueprint>{std::move(blueprint)}};
}

gameplay::SandboxSessionBuildResult initialize_sandbox_session_from_blueprint(
    gameplay::SandboxSession& destination,
    const SandboxSessionBlueprint& blueprint,
    const gameplay::SandboxPlayerRuntimeBinding& player_binding
) noexcept {
    using gameplay::SandboxSessionBuildError;
    if (!blueprint.valid_) {
        return {SandboxSessionBuildError::invalid_owned_state, {}};
    }

    std::array<contracts::SandboxSafePointDefinition,
               SandboxSessionBlueprint::safe_point_capacity> safe_points{};
    std::array<contracts::SandboxInteractionDefinition,
               SandboxSessionBlueprint::interaction_capacity> interactions{};
    std::array<contracts::SandboxMechanismDefinition,
               SandboxSessionBlueprint::mechanism_capacity> mechanisms{};
    std::array<contracts::SandboxGroundBlockerDefinition,
               SandboxSessionBlueprint::ground_blocker_capacity> ground_blockers{};
    std::array<contracts::SandboxInteractionGameplayBinding,
               SandboxSessionBlueprint::interaction_capacity> interaction_bindings{};
    std::array<contracts::SandboxMechanismGameplayBinding,
               SandboxSessionBlueprint::mechanism_capacity> mechanism_bindings{};

    contracts::SandboxDefinition core{};
    core.player.id = SandboxSessionBlueprint::content_id_view(blueprint.player_.id);
    core.player.region_id =
        SandboxSessionBlueprint::content_id_view(blueprint.player_.region_id);
    core.player.initial_safe_point_id = SandboxSessionBlueprint::content_id_view(
        blueprint.player_.initial_safe_point_id
    );
    core.player.pose = blueprint.player_.pose;
    core.player.facing_millidegrees = blueprint.player_.facing_millidegrees;

    for (std::size_t index = 0; index < blueprint.safe_point_count_; ++index) {
        safe_points[index].id =
            SandboxSessionBlueprint::content_id_view(blueprint.safe_points_[index].id);
        safe_points[index].region_id = SandboxSessionBlueprint::content_id_view(
            blueprint.safe_points_[index].region_id
        );
        safe_points[index].pose = blueprint.safe_points_[index].pose;
        safe_points[index].facing_millidegrees =
            blueprint.safe_points_[index].facing_millidegrees;
    }
    for (std::size_t index = 0; index < blueprint.interaction_count_; ++index) {
        interactions[index].id = SandboxSessionBlueprint::content_id_view(
            blueprint.interactions_[index].id
        );
        interactions[index].pose = blueprint.interactions_[index].pose;
    }
    for (std::size_t index = 0; index < blueprint.mechanism_count_; ++index) {
        mechanisms[index].id = SandboxSessionBlueprint::content_id_view(
            blueprint.mechanisms_[index].id
        );
    }
    for (std::size_t index = 0; index < blueprint.ground_blocker_count_; ++index) {
        ground_blockers[index].id = SandboxSessionBlueprint::content_id_view(
            blueprint.ground_blockers_[index].id
        );
    }
    for (std::size_t index = 0; index < blueprint.interaction_binding_count_; ++index) {
        const auto& source = blueprint.interaction_bindings_[index];
        auto& target = interaction_bindings[index];
        target.interaction_id = SandboxSessionBlueprint::content_id_view(
            source.interaction_id
        );
        target.operation = source.operation;
        target.range_mm = source.range_mm;
        target.target_mechanism_id = SandboxSessionBlueprint::content_id_view(
            source.target_mechanism_id
        );
    }
    for (std::size_t index = 0; index < blueprint.mechanism_binding_count_; ++index) {
        const auto& source = blueprint.mechanism_bindings_[index];
        auto& target = mechanism_bindings[index];
        target.mechanism_id = SandboxSessionBlueprint::content_id_view(
            source.mechanism_id
        );
        target.activation = source.activation;
        target.target_ground_blocker_id = SandboxSessionBlueprint::content_id_view(
            source.target_ground_blocker_id
        );
    }

    core.safe_points = std::span<const contracts::SandboxSafePointDefinition>{
        safe_points.data(), blueprint.safe_point_count_
    };
    core.interactions = std::span<const contracts::SandboxInteractionDefinition>{
        interactions.data(), blueprint.interaction_count_
    };
    core.mechanisms = std::span<const contracts::SandboxMechanismDefinition>{
        mechanisms.data(), blueprint.mechanism_count_
    };
    core.ground_blockers = std::span<const contracts::SandboxGroundBlockerDefinition>{
        ground_blockers.data(), blueprint.ground_blocker_count_
    };
    const contracts::SandboxGameplayBindingDefinition binding{
        std::span<const contracts::SandboxInteractionGameplayBinding>{
            interaction_bindings.data(), blueprint.interaction_binding_count_
        },
        std::span<const contracts::SandboxMechanismGameplayBinding>{
            mechanism_bindings.data(), blueprint.mechanism_binding_count_
        },
    };

    gameplay::SandboxSession candidate{};
    const auto result = candidate.initialize(core, binding, player_binding);
    if (result.error != SandboxSessionBuildError::none) {
        return result;
    }
    destination = std::move(candidate);
    return result;
}

}  // namespace tgd::integration
