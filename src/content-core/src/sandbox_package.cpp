#include <tgd/content/sandbox_package.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace tgd::content {
namespace {

using contracts::ContentId;
using contracts::SandboxDiagnostic;
using contracts::SandboxDiagnosticCode;
using contracts::SandboxDiagnosticDomain;
using contracts::SandboxDiagnosticField;
using contracts::SandboxPackageError;
using contracts::SandboxPackSectionType;

constexpr std::uint16_t sandbox_known_section_count = 15;

[[nodiscard]] bool byte_less(std::string_view left, std::string_view right) noexcept {
    return std::lexicographical_compare(
        left.begin(),
        left.end(),
        right.begin(),
        right.end(),
        [](char lhs, char rhs) {
            return static_cast<unsigned char>(lhs) < static_cast<unsigned char>(rhs);
        }
    );
}

[[nodiscard]] bool valid_utf8(std::string_view value) noexcept {
    std::size_t index = 0;
    while (index < value.size()) {
        const auto first = static_cast<std::uint8_t>(value[index]);
        std::uint32_t code_point = 0;
        std::size_t continuation_count = 0;
        if (first <= 0x7fU) {
            code_point = first;
        } else if (first >= 0xc2U && first <= 0xdfU) {
            code_point = first & 0x1fU;
            continuation_count = 1;
        } else if (first >= 0xe0U && first <= 0xefU) {
            code_point = first & 0x0fU;
            continuation_count = 2;
        } else if (first >= 0xf0U && first <= 0xf4U) {
            code_point = first & 0x07U;
            continuation_count = 3;
        } else {
            return false;
        }
        if (index + continuation_count >= value.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
            const auto next = static_cast<std::uint8_t>(value[index + offset]);
            if ((next & 0xc0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (next & 0x3fU);
        }
        if ((continuation_count == 2 && code_point < 0x800U) ||
            (continuation_count == 3 && code_point < 0x1'0000U) ||
            (code_point >= 0xd800U && code_point <= 0xdfffU) || code_point > 0x10'ffffU) {
            return false;
        }
        index += continuation_count + 1;
    }
    return true;
}

[[nodiscard]] bool empty_id(ContentId id) noexcept {
    return id.key == 0 && id.name.empty();
}

[[nodiscard]] bool valid_id(ContentId id) noexcept {
    return id.key != 0 && !id.name.empty() &&
           id.name.size() <= contracts::sandbox_pack_max_id_bytes && valid_utf8(id.name) &&
           contracts::stable_content_key(id.name) == id.key;
}

template <typename Definition>
[[nodiscard]] const Definition* find_id(
    std::span<const Definition> definitions,
    ContentId id
) noexcept {
    const auto found = std::find_if(definitions.begin(), definitions.end(), [&id](const auto& value) {
        return value.id == id;
    });
    return found == definitions.end() ? nullptr : &*found;
}

[[nodiscard]] bool valid_bounds(const contracts::SandboxBoundsMm& bounds) noexcept {
    return bounds.min_x <= bounds.max_x && bounds.min_y <= bounds.max_y &&
           bounds.min_height <= bounds.max_height &&
           bounds.min_floor_layer <= bounds.max_floor_layer;
}

[[nodiscard]] bool contains_bounds(
    const contracts::SandboxBoundsMm& outer,
    const contracts::SandboxBoundsMm& inner
) noexcept {
    return outer.min_x <= inner.min_x && inner.max_x <= outer.max_x &&
           outer.min_y <= inner.min_y && inner.max_y <= outer.max_y &&
           outer.min_height <= inner.min_height && inner.max_height <= outer.max_height &&
           outer.min_floor_layer <= inner.min_floor_layer &&
           inner.max_floor_layer <= outer.max_floor_layer;
}

[[nodiscard]] bool contains_pose(
    const contracts::SandboxBoundsMm& bounds,
    contracts::GroundPoseMm pose
) noexcept {
    return pose.x >= bounds.min_x && pose.x <= bounds.max_x && pose.y >= bounds.min_y &&
           pose.y <= bounds.max_y && pose.height >= bounds.min_height &&
           pose.height <= bounds.max_height && pose.floor_layer >= bounds.min_floor_layer &&
           pose.floor_layer <= bounds.max_floor_layer;
}

[[nodiscard]] bool valid_asset_kind(contracts::SandboxAssetKind kind) noexcept {
    switch (kind) {
        case contracts::SandboxAssetKind::player:
        case contracts::SandboxAssetKind::actor:
        case contracts::SandboxAssetKind::obstacle:
        case contracts::SandboxAssetKind::interaction:
        case contracts::SandboxAssetKind::mechanism:
        case contracts::SandboxAssetKind::safe_point:
        case contracts::SandboxAssetKind::effect:
            return true;
    }
    return false;
}

[[nodiscard]] bool valid_trigger_kind(contracts::SandboxTriggerKind kind) noexcept {
    return contracts::sandbox_trigger_reference_domain(kind) !=
           contracts::SandboxReferenceDomain::invalid;
}

[[nodiscard]] bool valid_completion_kind(
    contracts::SandboxObjectiveCompletionKind kind
) noexcept {
    return contracts::sandbox_completion_reference_domain(kind) !=
           contracts::SandboxReferenceDomain::invalid;
}

void add_diagnostic(
    std::vector<SandboxDiagnostic>& diagnostics,
    SandboxDiagnosticCode code,
    SandboxDiagnosticDomain domain,
    std::size_t record_index,
    SandboxDiagnosticField field,
    contracts::StableContentKey subject = 0,
    contracts::StableContentKey related = 0
) {
    diagnostics.push_back(
        {code, subject, related, static_cast<std::uint32_t>(record_index), domain, field}
    );
}

void validate_required_id(
    std::vector<SandboxDiagnostic>& diagnostics,
    ContentId id,
    SandboxDiagnosticCode code,
    SandboxDiagnosticDomain domain,
    std::size_t record_index,
    SandboxDiagnosticField field
) {
    if (!valid_id(id)) {
        add_diagnostic(diagnostics, code, domain, record_index, field, id.key);
    }
}

[[nodiscard]] SandboxDiagnosticField invalid_bounds_field(
    const contracts::SandboxBoundsMm& bounds
) noexcept {
    if (bounds.min_x > bounds.max_x) {
        return SandboxDiagnosticField::min_x;
    }
    if (bounds.min_y > bounds.max_y) {
        return SandboxDiagnosticField::min_y;
    }
    if (bounds.min_height > bounds.max_height) {
        return SandboxDiagnosticField::min_height;
    }
    return SandboxDiagnosticField::min_floor_layer;
}

template <typename Definition>
void validate_unique_ids(
    std::vector<SandboxDiagnostic>& diagnostics,
    std::span<const Definition> definitions,
    SandboxDiagnosticDomain domain
) {
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definitions[index].id.key != 0 &&
                definitions[index].id.key == definitions[prior].id.key) {
                add_diagnostic(
                    diagnostics,
                    SandboxDiagnosticCode::duplicate_id,
                    domain,
                    index,
                    SandboxDiagnosticField::id,
                    definitions[index].id.key,
                    definitions[prior].id.key
                );
                break;
            }
        }
    }
}

[[nodiscard]] bool reference_exists(
    const contracts::SandboxDefinition& definition,
    contracts::SandboxReferenceDomain domain,
    ContentId id
) noexcept {
    switch (domain) {
        case contracts::SandboxReferenceDomain::none:
            return empty_id(id);
        case contracts::SandboxReferenceDomain::interaction:
            return find_id(definition.interactions, id) != nullptr;
        case contracts::SandboxReferenceDomain::mechanism:
            return find_id(definition.mechanisms, id) != nullptr;
        case contracts::SandboxReferenceDomain::objective:
            return find_id(definition.objectives, id) != nullptr;
        case contracts::SandboxReferenceDomain::wave:
            return find_id(definition.waves, id) != nullptr;
        case contracts::SandboxReferenceDomain::invalid:
            return false;
    }
    return false;
}

template <typename Definition, typename Predecessor>
void validate_predecessor_graph(
    std::vector<SandboxDiagnostic>& diagnostics,
    std::span<const Definition> definitions,
    Predecessor predecessor,
    SandboxDiagnosticDomain domain,
    SandboxDiagnosticField field
) {
    for (std::size_t start = 0; start < definitions.size(); ++start) {
        std::size_t steps = 0;
        ContentId current = predecessor(definitions[start]);
        while (!empty_id(current) && steps <= definitions.size()) {
            const auto* node = find_id(definitions, current);
            if (node == nullptr) {
                break;
            }
            if (node->id == definitions[start].id || ++steps > definitions.size()) {
                add_diagnostic(
                    diagnostics,
                    SandboxDiagnosticCode::dependency_cycle,
                    domain,
                    start,
                    field,
                    definitions[start].id.key,
                    current.key
                );
                break;
            }
            current = predecessor(*node);
        }
    }
}

void sort_diagnostics(std::vector<SandboxDiagnostic>& diagnostics) {
    std::sort(diagnostics.begin(), diagnostics.end(), [](const auto& left, const auto& right) {
        return std::tuple{
                   static_cast<std::uint16_t>(left.code),
                   static_cast<std::uint8_t>(left.domain),
                   left.record_index,
                   static_cast<std::uint16_t>(left.field),
                   left.subject,
                   left.related,
               } <
               std::tuple{
                   static_cast<std::uint16_t>(right.code),
                   static_cast<std::uint8_t>(right.domain),
                   right.record_index,
                   static_cast<std::uint16_t>(right.field),
                   right.subject,
                   right.related,
               };
    });
    diagnostics.erase(std::unique(diagnostics.begin(), diagnostics.end()), diagnostics.end());
}

[[nodiscard]] std::vector<SandboxDiagnostic> validate_package_core(
    const contracts::SandboxDefinition& definition
) {
    std::vector<SandboxDiagnostic> diagnostics;

    const auto capacity = [&](std::size_t size,
                              std::size_t maximum,
                              SandboxDiagnosticDomain domain) {
        if (size > maximum) {
            add_diagnostic(
                diagnostics,
                SandboxDiagnosticCode::capacity_exceeded,
                domain,
                size,
                SandboxDiagnosticField::none
            );
        }
    };
    capacity(definition.regions.size(), contracts::sandbox_region_capacity, SandboxDiagnosticDomain::regions);
    capacity(definition.assets.size(), contracts::sandbox_asset_capacity, SandboxDiagnosticDomain::assets);
    capacity(definition.actors.size(), contracts::sandbox_actor_capacity, SandboxDiagnosticDomain::actors);
    capacity(definition.ground_blockers.size(), contracts::sandbox_ground_blocker_capacity, SandboxDiagnosticDomain::ground_blockers);
    capacity(definition.safe_points.size(), contracts::sandbox_safe_point_capacity, SandboxDiagnosticDomain::safe_points);
    capacity(definition.interactions.size(), contracts::sandbox_interaction_capacity, SandboxDiagnosticDomain::interactions);
    capacity(definition.mechanisms.size(), contracts::sandbox_mechanism_capacity, SandboxDiagnosticDomain::mechanisms);
    capacity(definition.waves.size(), contracts::sandbox_wave_capacity, SandboxDiagnosticDomain::waves);
    capacity(definition.wave_spawns.size(), contracts::sandbox_wave_spawn_capacity, SandboxDiagnosticDomain::wave_spawns);
    capacity(definition.objectives.size(), contracts::sandbox_objective_capacity, SandboxDiagnosticDomain::objectives);

    validate_required_id(diagnostics, definition.package_id, SandboxDiagnosticCode::invalid_stable_id, SandboxDiagnosticDomain::metadata, 0, SandboxDiagnosticField::package_id);
    validate_required_id(diagnostics, definition.id, SandboxDiagnosticCode::invalid_stable_id, SandboxDiagnosticDomain::metadata, 0, SandboxDiagnosticField::sandbox_id);
    if (!valid_bounds(definition.bounds)) {
        add_diagnostic(diagnostics, SandboxDiagnosticCode::invalid_world_bounds, SandboxDiagnosticDomain::metadata, 0, invalid_bounds_field(definition.bounds), definition.id.key);
    }

    validate_unique_ids(diagnostics, definition.regions, SandboxDiagnosticDomain::regions);
    validate_unique_ids(diagnostics, definition.assets, SandboxDiagnosticDomain::assets);
    validate_unique_ids(diagnostics, definition.actors, SandboxDiagnosticDomain::actors);
    validate_unique_ids(diagnostics, definition.ground_blockers, SandboxDiagnosticDomain::ground_blockers);
    validate_unique_ids(diagnostics, definition.safe_points, SandboxDiagnosticDomain::safe_points);
    validate_unique_ids(diagnostics, definition.interactions, SandboxDiagnosticDomain::interactions);
    validate_unique_ids(diagnostics, definition.mechanisms, SandboxDiagnosticDomain::mechanisms);
    validate_unique_ids(diagnostics, definition.waves, SandboxDiagnosticDomain::waves);
    validate_unique_ids(diagnostics, definition.objectives, SandboxDiagnosticDomain::objectives);

    struct RegisteredId final {
        contracts::StableContentKey key{};
        SandboxDiagnosticDomain domain{};
        std::size_t index{};
    };
    std::vector<RegisteredId> registered_ids;
    const auto register_id = [&](ContentId id,
                                 SandboxDiagnosticDomain domain,
                                 std::size_t index,
                                 SandboxDiagnosticField field) {
        if (id.key == 0) return;
        const auto prior = std::find_if(
            registered_ids.begin(), registered_ids.end(),
            [id](const auto& value) { return value.key == id.key; }
        );
        if (prior != registered_ids.end()) {
            add_diagnostic(
                diagnostics,
                SandboxDiagnosticCode::duplicate_id,
                domain,
                index,
                field,
                id.key,
                prior->key
            );
        } else {
            registered_ids.push_back({id.key, domain, index});
        }
    };
    register_id(definition.package_id, SandboxDiagnosticDomain::metadata, 0,
                SandboxDiagnosticField::package_id);
    register_id(definition.id, SandboxDiagnosticDomain::metadata, 0,
                SandboxDiagnosticField::sandbox_id);
    register_id(definition.player.id, SandboxDiagnosticDomain::player, 0,
                SandboxDiagnosticField::id);
    const auto register_span = [&](const auto& values, SandboxDiagnosticDomain domain) {
        for (std::size_t index = 0; index < values.size(); ++index) {
            register_id(values[index].id, domain, index, SandboxDiagnosticField::id);
        }
    };
    register_span(definition.regions, SandboxDiagnosticDomain::regions);
    register_span(definition.assets, SandboxDiagnosticDomain::assets);
    register_span(definition.actors, SandboxDiagnosticDomain::actors);
    register_span(definition.ground_blockers, SandboxDiagnosticDomain::ground_blockers);
    register_span(definition.safe_points, SandboxDiagnosticDomain::safe_points);
    register_span(definition.interactions, SandboxDiagnosticDomain::interactions);
    register_span(definition.mechanisms, SandboxDiagnosticDomain::mechanisms);
    register_span(definition.waves, SandboxDiagnosticDomain::waves);
    register_span(definition.objectives, SandboxDiagnosticDomain::objectives);

    for (std::size_t index = 0; index < definition.regions.size(); ++index) {
        const auto& value = definition.regions[index];
        validate_required_id(diagnostics, value.id, SandboxDiagnosticCode::invalid_region_bounds, SandboxDiagnosticDomain::regions, index, SandboxDiagnosticField::id);
        if (!valid_bounds(value.bounds) ||
            (valid_bounds(definition.bounds) && !contains_bounds(definition.bounds, value.bounds))) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::invalid_region_bounds, SandboxDiagnosticDomain::regions, index, valid_bounds(value.bounds) ? SandboxDiagnosticField::none : invalid_bounds_field(value.bounds), value.id.key, definition.id.key);
        }
    }

    for (std::size_t index = 0; index < definition.assets.size(); ++index) {
        const auto& value = definition.assets[index];
        validate_required_id(diagnostics, value.id, SandboxDiagnosticCode::missing_asset, SandboxDiagnosticDomain::assets, index, SandboxDiagnosticField::id);
        if (!valid_asset_kind(value.kind)) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::asset_kind_mismatch, SandboxDiagnosticDomain::assets, index, SandboxDiagnosticField::asset_kind, value.id.key);
        }
    }

    const auto validate_placement = [&](ContentId id,
                                        ContentId region_id,
                                        ContentId asset_id,
                                        contracts::GroundPoseMm pose,
                                        std::uint32_t facing,
                                        contracts::SandboxAssetKind expected_kind,
                                        SandboxDiagnosticCode invalid_code,
                                        SandboxDiagnosticDomain domain,
                                        std::size_t index) {
        validate_required_id(diagnostics, id, invalid_code, domain, index, SandboxDiagnosticField::id);
        const auto* region = find_id(definition.regions, region_id);
        const auto* asset = find_id(definition.assets, asset_id);
        if (region == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_region, domain, index, SandboxDiagnosticField::region_id, id.key, region_id.key);
        }
        if (asset == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_asset, domain, index, SandboxDiagnosticField::asset_id, id.key, asset_id.key);
        } else if (asset->kind != expected_kind) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::asset_kind_mismatch, domain, index, SandboxDiagnosticField::asset_id, id.key, asset_id.key);
        }
        if (facing >= 360'000U) {
            add_diagnostic(diagnostics, invalid_code, domain, index, SandboxDiagnosticField::facing_millidegrees, id.key);
        }
        if ((valid_bounds(definition.bounds) && !contains_pose(definition.bounds, pose)) ||
            (region != nullptr && !contains_pose(region->bounds, pose))) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::object_out_of_bounds, domain, index, SandboxDiagnosticField::x, id.key, region_id.key);
        }
    };

    const auto& player = definition.player;
    validate_placement(player.id, player.region_id, player.asset_id, player.pose, player.facing_millidegrees, contracts::SandboxAssetKind::player, SandboxDiagnosticCode::invalid_player, SandboxDiagnosticDomain::player, 0);
    if (find_id(definition.safe_points, player.initial_safe_point_id) == nullptr) {
        add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_safe_point, SandboxDiagnosticDomain::player, 0, SandboxDiagnosticField::initial_safe_point_id, player.id.key, player.initial_safe_point_id.key);
    } else {
        const auto* initial = find_id(definition.safe_points, player.initial_safe_point_id);
        if (initial->region_id != player.region_id) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::retry_inconsistent,
                           SandboxDiagnosticDomain::player, 0,
                           SandboxDiagnosticField::initial_safe_point_id,
                           player.id.key, initial->id.key);
        }
    }
    for (std::size_t index = 0; index < definition.actors.size(); ++index) {
        const auto& value = definition.actors[index];
        validate_placement(value.id, value.region_id, value.asset_id, value.pose, value.facing_millidegrees, contracts::SandboxAssetKind::actor, SandboxDiagnosticCode::invalid_actor, SandboxDiagnosticDomain::actors, index);
    }
    for (std::size_t index = 0; index < definition.safe_points.size(); ++index) {
        const auto& value = definition.safe_points[index];
        validate_placement(value.id, value.region_id, value.asset_id, value.pose, value.facing_millidegrees, contracts::SandboxAssetKind::safe_point, SandboxDiagnosticCode::invalid_safe_point, SandboxDiagnosticDomain::safe_points, index);
    }
    for (std::size_t index = 0; index < definition.interactions.size(); ++index) {
        const auto& value = definition.interactions[index];
        validate_placement(value.id, value.region_id, value.asset_id, value.pose, value.facing_millidegrees, contracts::SandboxAssetKind::interaction, SandboxDiagnosticCode::invalid_interaction, SandboxDiagnosticDomain::interactions, index);
    }
    for (std::size_t index = 0; index < definition.mechanisms.size(); ++index) {
        const auto& value = definition.mechanisms[index];
        validate_placement(value.id, value.region_id, value.asset_id, value.pose, value.facing_millidegrees, contracts::SandboxAssetKind::mechanism, SandboxDiagnosticCode::invalid_mechanism, SandboxDiagnosticDomain::mechanisms, index);
    }

    for (std::size_t index = 0; index < definition.ground_blockers.size(); ++index) {
        const auto& value = definition.ground_blockers[index];
        validate_required_id(diagnostics, value.id, SandboxDiagnosticCode::invalid_blocker, SandboxDiagnosticDomain::ground_blockers, index, SandboxDiagnosticField::id);
        const auto* region = find_id(definition.regions, value.region_id);
        const auto* asset = find_id(definition.assets, value.asset_id);
        if (region == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_region, SandboxDiagnosticDomain::ground_blockers, index, SandboxDiagnosticField::region_id, value.id.key, value.region_id.key);
        }
        if (asset == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_asset, SandboxDiagnosticDomain::ground_blockers, index, SandboxDiagnosticField::asset_id, value.id.key, value.asset_id.key);
        } else if (asset->kind != contracts::SandboxAssetKind::obstacle) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::asset_kind_mismatch, SandboxDiagnosticDomain::ground_blockers, index, SandboxDiagnosticField::asset_id, value.id.key, value.asset_id.key);
        }
        const contracts::SandboxBoundsMm bounds{value.min_x, value.max_x, value.min_y, value.max_y, value.min_height, value.max_height, value.floor_layer, value.floor_layer};
        if (!valid_bounds(bounds) || (valid_bounds(definition.bounds) && !contains_bounds(definition.bounds, bounds)) || (region != nullptr && !contains_bounds(region->bounds, bounds))) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::invalid_blocker, SandboxDiagnosticDomain::ground_blockers, index, valid_bounds(bounds) ? SandboxDiagnosticField::none : invalid_bounds_field(bounds), value.id.key, value.region_id.key);
        }
    }

    const auto point_blocked = [&](ContentId region_id, contracts::GroundPoseMm pose) {
        return std::any_of(
            definition.ground_blockers.begin(), definition.ground_blockers.end(),
            [region_id, pose](const auto& blocker) {
                return blocker.region_id == region_id &&
                       pose.floor_layer == blocker.floor_layer && pose.x >= blocker.min_x &&
                       pose.x <= blocker.max_x && pose.y >= blocker.min_y &&
                       pose.y <= blocker.max_y && pose.height >= blocker.min_height &&
                       pose.height <= blocker.max_height;
            }
        );
    };
    if (point_blocked(player.region_id, player.pose)) {
        add_diagnostic(diagnostics, SandboxDiagnosticCode::player_start_blocked,
                       SandboxDiagnosticDomain::player, 0, SandboxDiagnosticField::x,
                       player.id.key);
    }
    for (std::size_t index = 0; index < definition.safe_points.size(); ++index) {
        if (point_blocked(definition.safe_points[index].region_id,
                          definition.safe_points[index].pose)) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::safe_point_blocked,
                           SandboxDiagnosticDomain::safe_points, index,
                           SandboxDiagnosticField::x, definition.safe_points[index].id.key);
        }
    }

    for (std::size_t index = 0; index < definition.waves.size(); ++index) {
        const auto& value = definition.waves[index];
        validate_required_id(diagnostics, value.id, SandboxDiagnosticCode::invalid_wave, SandboxDiagnosticDomain::waves, index, SandboxDiagnosticField::id);
        if (find_id(definition.regions, value.region_id) == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_region, SandboxDiagnosticDomain::waves, index, SandboxDiagnosticField::region_id, value.id.key, value.region_id.key);
        }
        if (!empty_id(value.predecessor_wave_id) && find_id(definition.waves, value.predecessor_wave_id) == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_reference, SandboxDiagnosticDomain::waves, index, SandboxDiagnosticField::predecessor_wave_id, value.id.key, value.predecessor_wave_id.key);
        }
        if (!valid_trigger_kind(value.trigger.kind)) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::invalid_wave, SandboxDiagnosticDomain::waves, index, SandboxDiagnosticField::trigger_kind, value.id.key);
        } else {
            const auto domain = contracts::sandbox_trigger_reference_domain(value.trigger.kind);
            if (!reference_exists(definition, domain, value.trigger.target_id)) {
                add_diagnostic(diagnostics, SandboxDiagnosticCode::reference_kind_mismatch, SandboxDiagnosticDomain::waves, index, SandboxDiagnosticField::trigger_target_id, value.id.key, value.trigger.target_id.key);
            }
        }
    }
    validate_predecessor_graph(diagnostics, definition.waves, [](const auto& value) { return value.predecessor_wave_id; }, SandboxDiagnosticDomain::waves, SandboxDiagnosticField::predecessor_wave_id);

    for (std::size_t index = 0; index < definition.wave_spawns.size(); ++index) {
        const auto& value = definition.wave_spawns[index];
        if (find_id(definition.waves, value.wave_id) == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_reference, SandboxDiagnosticDomain::wave_spawns, index, SandboxDiagnosticField::wave_id, value.wave_id.key, value.wave_id.key);
        }
        if (find_id(definition.actors, value.actor_id) == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::reference_kind_mismatch, SandboxDiagnosticDomain::wave_spawns, index, SandboxDiagnosticField::actor_id, value.wave_id.key, value.actor_id.key);
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definition.wave_spawns[prior].wave_id == value.wave_id &&
                definition.wave_spawns[prior].spawn_order == value.spawn_order) {
                add_diagnostic(diagnostics, SandboxDiagnosticCode::invalid_wave_spawn, SandboxDiagnosticDomain::wave_spawns, index, SandboxDiagnosticField::spawn_order, value.wave_id.key, value.actor_id.key);
            }
            if (definition.wave_spawns[prior].actor_id == value.actor_id) {
                add_diagnostic(diagnostics, SandboxDiagnosticCode::invalid_wave_spawn,
                               SandboxDiagnosticDomain::wave_spawns, index,
                               SandboxDiagnosticField::actor_id, value.wave_id.key,
                               value.actor_id.key);
            }
        }
    }
    for (std::size_t index = 0; index < definition.waves.size(); ++index) {
        const auto& wave = definition.waves[index];
        if (std::none_of(definition.wave_spawns.begin(), definition.wave_spawns.end(),
                         [&wave](const auto& spawn) { return spawn.wave_id == wave.id; })) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::unreachable_node,
                           SandboxDiagnosticDomain::waves, index,
                           SandboxDiagnosticField::wave_id, wave.id.key);
        }
    }
    for (std::size_t index = 0; index < definition.actors.size(); ++index) {
        const auto& actor = definition.actors[index];
        if (std::none_of(definition.wave_spawns.begin(), definition.wave_spawns.end(),
                         [&actor](const auto& spawn) { return spawn.actor_id == actor.id; })) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::unreachable_node,
                           SandboxDiagnosticDomain::actors, index,
                           SandboxDiagnosticField::actor_id, actor.id.key);
        }
    }

    for (std::size_t index = 0; index < definition.objectives.size(); ++index) {
        const auto& value = definition.objectives[index];
        validate_required_id(diagnostics, value.id, SandboxDiagnosticCode::invalid_objective, SandboxDiagnosticDomain::objectives, index, SandboxDiagnosticField::id);
        if (find_id(definition.regions, value.region_id) == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_region, SandboxDiagnosticDomain::objectives, index, SandboxDiagnosticField::region_id, value.id.key, value.region_id.key);
        }
        if (!empty_id(value.predecessor_objective_id) && find_id(definition.objectives, value.predecessor_objective_id) == nullptr) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_reference, SandboxDiagnosticDomain::objectives, index, SandboxDiagnosticField::predecessor_objective_id, value.id.key, value.predecessor_objective_id.key);
        }
        if (!valid_completion_kind(value.completion.kind)) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::invalid_objective, SandboxDiagnosticDomain::objectives, index, SandboxDiagnosticField::completion_kind, value.id.key);
        } else {
            const auto domain = contracts::sandbox_completion_reference_domain(value.completion.kind);
            if (!reference_exists(definition, domain, value.completion.target_id)) {
                add_diagnostic(diagnostics, SandboxDiagnosticCode::reference_kind_mismatch, SandboxDiagnosticDomain::objectives, index, SandboxDiagnosticField::completion_target_id, value.id.key, value.completion.target_id.key);
            }
        }
    }
    validate_predecessor_graph(diagnostics, definition.objectives, [](const auto& value) { return value.predecessor_objective_id; }, SandboxDiagnosticDomain::objectives, SandboxDiagnosticField::predecessor_objective_id);

    struct GraphNode final {
        SandboxDiagnosticDomain domain{};
        std::size_t record_index{};
        contracts::StableContentKey key{};
        bool external_root{};
        std::vector<std::pair<std::size_t, SandboxDiagnosticField>> dependencies{};
    };
    std::vector<GraphNode> graph;
    graph.reserve(definition.waves.size() + definition.objectives.size());
    for (std::size_t index = 0; index < definition.waves.size(); ++index) {
        const auto& wave = definition.waves[index];
        const auto domain = contracts::sandbox_trigger_reference_domain(wave.trigger.kind);
        graph.push_back({SandboxDiagnosticDomain::waves, index, wave.id.key,
                         domain == contracts::SandboxReferenceDomain::none ||
                             domain == contracts::SandboxReferenceDomain::interaction ||
                             domain == contracts::SandboxReferenceDomain::mechanism,
                         {}});
    }
    for (std::size_t index = 0; index < definition.objectives.size(); ++index) {
        const auto& objective = definition.objectives[index];
        const auto domain = contracts::sandbox_completion_reference_domain(
            objective.completion.kind
        );
        graph.push_back({SandboxDiagnosticDomain::objectives, index, objective.id.key,
                         domain == contracts::SandboxReferenceDomain::interaction ||
                             domain == contracts::SandboxReferenceDomain::mechanism,
                         {}});
    }
    const auto graph_index = [&](ContentId id, SandboxDiagnosticDomain domain) {
        const auto found = std::find_if(graph.begin(), graph.end(), [&](const auto& node) {
            return node.domain == domain && node.key == id.key;
        });
        return found == graph.end() ? graph.size()
                                    : static_cast<std::size_t>(found - graph.begin());
    };
    const auto add_dependency = [&](GraphNode& node, ContentId id,
                                    SandboxDiagnosticDomain domain,
                                    SandboxDiagnosticField field) {
        if (empty_id(id)) return;
        const auto dependency = graph_index(id, domain);
        if (dependency < graph.size()) node.dependencies.emplace_back(dependency, field);
    };
    for (std::size_t index = 0; index < definition.waves.size(); ++index) {
        const auto& wave = definition.waves[index];
        add_dependency(graph[index], wave.predecessor_wave_id,
                       SandboxDiagnosticDomain::waves,
                       SandboxDiagnosticField::predecessor_wave_id);
        const auto domain = contracts::sandbox_trigger_reference_domain(wave.trigger.kind);
        if (domain == contracts::SandboxReferenceDomain::wave) {
            add_dependency(graph[index], wave.trigger.target_id,
                           SandboxDiagnosticDomain::waves,
                           SandboxDiagnosticField::trigger_target_id);
        } else if (domain == contracts::SandboxReferenceDomain::objective) {
            add_dependency(graph[index], wave.trigger.target_id,
                           SandboxDiagnosticDomain::objectives,
                           SandboxDiagnosticField::trigger_target_id);
        }
    }
    for (std::size_t index = 0; index < definition.objectives.size(); ++index) {
        auto& node = graph[definition.waves.size() + index];
        const auto& objective = definition.objectives[index];
        add_dependency(node, objective.predecessor_objective_id,
                       SandboxDiagnosticDomain::objectives,
                       SandboxDiagnosticField::predecessor_objective_id);
        if (contracts::sandbox_completion_reference_domain(objective.completion.kind) ==
            contracts::SandboxReferenceDomain::wave) {
            add_dependency(node, objective.completion.target_id,
                           SandboxDiagnosticDomain::waves,
                           SandboxDiagnosticField::completion_target_id);
        }
    }
    std::vector<std::uint8_t> visit(graph.size(), 0);
    const auto visit_node = [&](auto&& self, std::size_t index) -> void {
        if (visit[index] == 2) return;
        if (visit[index] == 1) return;
        visit[index] = 1;
        for (const auto [dependency, field] : graph[index].dependencies) {
            if (visit[dependency] == 1) {
                add_diagnostic(diagnostics, SandboxDiagnosticCode::dependency_cycle,
                               graph[index].domain, graph[index].record_index, field,
                               graph[index].key, graph[dependency].key);
            } else {
                self(self, dependency);
            }
        }
        visit[index] = 2;
    };
    for (std::size_t index = 0; index < graph.size(); ++index) visit_node(visit_node, index);
    std::vector<bool> reachable(graph.size(), false);
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t index = 0; index < graph.size(); ++index) {
            if (reachable[index] || !graph[index].external_root) continue;
            const auto ready = std::all_of(
                graph[index].dependencies.begin(), graph[index].dependencies.end(),
                [&reachable](const auto& dependency) { return reachable[dependency.first]; }
            );
            if (ready) { reachable[index] = true; changed = true; }
        }
        for (std::size_t index = 0; index < graph.size(); ++index) {
            if (reachable[index] || graph[index].external_root) continue;
            const auto ready = !graph[index].dependencies.empty() && std::all_of(
                graph[index].dependencies.begin(), graph[index].dependencies.end(),
                [&reachable](const auto& dependency) { return reachable[dependency.first]; }
            );
            if (ready) { reachable[index] = true; changed = true; }
        }
    }
    for (std::size_t index = 0; index < graph.size(); ++index) {
        if (!reachable[index]) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::unreachable_node,
                           graph[index].domain, graph[index].record_index,
                           SandboxDiagnosticField::none, graph[index].key);
        }
    }
    const auto completion_graph_index = graph_index(
        definition.completion_objective_id,
        SandboxDiagnosticDomain::objectives
    );
    if (completion_graph_index < graph.size()) {
        std::vector<bool> contributes(graph.size(), false);
        const auto mark_dependencies = [&](auto&& self, std::size_t index) -> void {
            if (contributes[index]) return;
            contributes[index] = true;
            for (const auto& dependency : graph[index].dependencies) {
                self(self, dependency.first);
            }
        };
        mark_dependencies(mark_dependencies, completion_graph_index);
        for (std::size_t index = 0; index < graph.size(); ++index) {
            if (!contributes[index]) {
                add_diagnostic(diagnostics, SandboxDiagnosticCode::unreachable_node,
                               graph[index].domain, graph[index].record_index,
                               SandboxDiagnosticField::none, graph[index].key,
                               definition.completion_objective_id.key);
            }
        }
        const auto has_successor = std::any_of(
            graph.begin(), graph.end(), [&](const auto& node) {
                return std::any_of(node.dependencies.begin(), node.dependencies.end(),
                                   [completion_graph_index](const auto& dependency) {
                                       return dependency.first == completion_graph_index;
                                   });
            }
        );
        if (has_successor) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::unreachable_node,
                           SandboxDiagnosticDomain::metadata, 0,
                           SandboxDiagnosticField::completion_objective_id,
                           definition.id.key, definition.completion_objective_id.key);
        }
    }
    if (find_id(definition.objectives, definition.completion_objective_id) == nullptr) {
        add_diagnostic(diagnostics, SandboxDiagnosticCode::missing_reference, SandboxDiagnosticDomain::metadata, 0, SandboxDiagnosticField::completion_objective_id, definition.id.key, definition.completion_objective_id.key);
    } else {
        const auto completion = graph_index(
            definition.completion_objective_id,
            SandboxDiagnosticDomain::objectives
        );
        if (completion >= reachable.size() || !reachable[completion]) {
            add_diagnostic(diagnostics, SandboxDiagnosticCode::unreachable_node,
                           SandboxDiagnosticDomain::metadata, 0,
                           SandboxDiagnosticField::completion_objective_id,
                           definition.id.key, definition.completion_objective_id.key);
        }
    }

    sort_diagnostics(diagnostics);
    return diagnostics;
}

}  // namespace

SandboxPackageValidation validate_sandbox_package(
    const contracts::SandboxDefinition& definition,
    const contracts::SandboxGameplayBindingDefinition& gameplay_binding
) noexcept {
    SandboxPackageValidation result{};
    try {
        result.diagnostics = validate_package_core(definition);
        if (!result.diagnostics.empty()) {
            result.error = SandboxPackageError::semantic_validation_failed;
            return result;
        }
        result.gameplay_binding_validation =
            contracts::validate_sandbox_gameplay_binding(definition, gameplay_binding);
        if (!contracts::sandbox_gameplay_binding_is_valid(
                result.gameplay_binding_validation
            )) {
            result.error = SandboxPackageError::gameplay_binding_validation_failed;
            return result;
        }
        result.error = SandboxPackageError::none;
        return result;
    } catch (const std::bad_alloc&) {
        result.error = SandboxPackageError::allocation_failed;
        result.diagnostics.clear();
        result.gameplay_binding_validation = {};
        return result;
    }
}

struct SandboxPackageDocument::Storage final {
    contracts::Sha256Digest fingerprint{};
    std::vector<std::string> strings{};
    contracts::SandboxDefinition definition{};
    contracts::SandboxGameplayBindingDefinition gameplay_binding{};
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

    void bind_views() noexcept {
        definition.regions = regions;
        definition.assets = assets;
        definition.actors = actors;
        definition.ground_blockers = ground_blockers;
        definition.safe_points = safe_points;
        definition.interactions = interactions;
        definition.mechanisms = mechanisms;
        definition.waves = waves;
        definition.wave_spawns = wave_spawns;
        definition.objectives = objectives;
        gameplay_binding.interaction_bindings = interaction_bindings;
        gameplay_binding.mechanism_bindings = mechanism_bindings;
    }
};

namespace {

template <typename Integer>
void append_le(std::vector<std::uint8_t>& bytes, Integer value) {
    static_assert(std::is_integral_v<Integer>);
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto bits = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        bytes.push_back(static_cast<std::uint8_t>(bits >> (index * 8U)));
    }
}

template <typename Integer>
void patch_le(std::vector<std::uint8_t>& bytes, std::size_t offset, Integer value) {
    static_assert(std::is_integral_v<Integer>);
    using Unsigned = std::make_unsigned_t<Integer>;
    const auto bits = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(bits >> (index * 8U));
    }
}

void append_zeroes(std::vector<std::uint8_t>& bytes, std::size_t count) {
    bytes.insert(bytes.end(), count, std::uint8_t{0});
}

void align_to_pack(std::vector<std::uint8_t>& bytes) {
    while (bytes.size() % contracts::sandbox_pack_alignment_bytes != 0) {
        bytes.push_back(0);
    }
}

struct EncodedSection final {
    SandboxPackSectionType type{};
    std::uint32_t count{};
    std::uint32_t width{};
    std::vector<std::uint8_t> bytes{};
    std::uint32_t offset{};
};

void collect_id(std::vector<std::string_view>& strings, ContentId id) {
    if (!empty_id(id)) {
        strings.push_back(id.name);
    }
}

[[nodiscard]] std::vector<std::string_view> collect_strings(
    const contracts::SandboxDefinition& definition,
    const contracts::SandboxGameplayBindingDefinition& binding
) {
    std::vector<std::string_view> strings;
    strings.reserve(
        8 + definition.regions.size() + definition.assets.size() +
        definition.actors.size() * 3 + definition.ground_blockers.size() * 3 +
        definition.safe_points.size() * 3 + definition.interactions.size() * 3 +
        definition.mechanisms.size() * 3 + definition.waves.size() * 4 +
        definition.wave_spawns.size() * 2 + definition.objectives.size() * 4 +
        binding.interaction_bindings.size() * 2 + binding.mechanism_bindings.size() * 2
    );
    collect_id(strings, definition.package_id);
    collect_id(strings, definition.id);
    collect_id(strings, definition.completion_objective_id);
    collect_id(strings, definition.player.id);
    collect_id(strings, definition.player.region_id);
    collect_id(strings, definition.player.asset_id);
    collect_id(strings, definition.player.initial_safe_point_id);
    for (const auto& value : definition.regions) {
        collect_id(strings, value.id);
    }
    for (const auto& value : definition.assets) {
        collect_id(strings, value.id);
    }
    const auto collect_placement = [&](const auto& value) {
        collect_id(strings, value.id);
        collect_id(strings, value.region_id);
        collect_id(strings, value.asset_id);
    };
    for (const auto& value : definition.actors) collect_placement(value);
    for (const auto& value : definition.ground_blockers) collect_placement(value);
    for (const auto& value : definition.safe_points) collect_placement(value);
    for (const auto& value : definition.interactions) collect_placement(value);
    for (const auto& value : definition.mechanisms) collect_placement(value);
    for (const auto& value : definition.waves) {
        collect_id(strings, value.id);
        collect_id(strings, value.region_id);
        collect_id(strings, value.predecessor_wave_id);
        collect_id(strings, value.trigger.target_id);
    }
    for (const auto& value : definition.wave_spawns) {
        collect_id(strings, value.wave_id);
        collect_id(strings, value.actor_id);
    }
    for (const auto& value : definition.objectives) {
        collect_id(strings, value.id);
        collect_id(strings, value.region_id);
        collect_id(strings, value.predecessor_objective_id);
        collect_id(strings, value.completion.target_id);
    }
    for (const auto& value : binding.interaction_bindings) {
        collect_id(strings, value.interaction_id);
        collect_id(strings, value.target_mechanism_id);
    }
    for (const auto& value : binding.mechanism_bindings) {
        collect_id(strings, value.mechanism_id);
        collect_id(strings, value.target_ground_blocker_id);
    }
    std::sort(strings.begin(), strings.end(), byte_less);
    strings.erase(std::unique(strings.begin(), strings.end()), strings.end());
    return strings;
}

[[nodiscard]] std::uint32_t string_index(
    std::span<const std::string_view> strings,
    std::string_view value
) {
    const auto found = std::lower_bound(strings.begin(), strings.end(), value, byte_less);
    return static_cast<std::uint32_t>(found - strings.begin());
}

void append_id(
    std::vector<std::uint8_t>& bytes,
    std::span<const std::string_view> strings,
    ContentId id
) {
    append_le(bytes, id.key);
    append_le(
        bytes,
        empty_id(id) ? contracts::sandbox_pack_no_string_index
                     : string_index(strings, id.name)
    );
}

void append_bounds(
    std::vector<std::uint8_t>& bytes,
    const contracts::SandboxBoundsMm& bounds
) {
    append_le(bytes, bounds.min_x);
    append_le(bytes, bounds.max_x);
    append_le(bytes, bounds.min_y);
    append_le(bytes, bounds.max_y);
    append_le(bytes, bounds.min_height);
    append_le(bytes, bounds.max_height);
    append_le(bytes, bounds.min_floor_layer);
    append_le(bytes, bounds.max_floor_layer);
}

void append_pose(std::vector<std::uint8_t>& bytes, contracts::GroundPoseMm pose) {
    append_le(bytes, pose.x);
    append_le(bytes, pose.y);
    append_le(bytes, pose.height);
    append_le(bytes, pose.floor_layer);
    append_le(bytes, std::uint16_t{0});
}

template <typename Definition>
[[nodiscard]] std::vector<Definition> sorted_by_id(std::span<const Definition> values) {
    std::vector<Definition> result(values.begin(), values.end());
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return std::tuple{left.id.key, left.id.name} < std::tuple{right.id.key, right.id.name};
    });
    return result;
}

template <typename Binding, typename IdGetter>
[[nodiscard]] std::vector<Binding> sorted_bindings(
    std::span<const Binding> values,
    IdGetter id
) {
    std::vector<Binding> result(values.begin(), values.end());
    std::sort(result.begin(), result.end(), [&](const auto& left, const auto& right) {
        const auto left_id = id(left);
        const auto right_id = id(right);
        return std::tuple{left_id.key, left_id.name} <
               std::tuple{right_id.key, right_id.name};
    });
    return result;
}

[[nodiscard]] std::vector<EncodedSection> encode_sections(
    const contracts::SandboxDefinition& definition,
    const contracts::SandboxGameplayBindingDefinition& binding,
    std::span<const std::string_view> strings
) {
    std::vector<EncodedSection> sections;
    sections.reserve(sandbox_known_section_count);

    EncodedSection string_section{SandboxPackSectionType::strings, static_cast<std::uint32_t>(strings.size()), 0};
    for (const auto value : strings) {
        append_le(string_section.bytes, static_cast<std::uint16_t>(value.size()));
        string_section.bytes.insert(string_section.bytes.end(), value.begin(), value.end());
    }
    sections.push_back(std::move(string_section));

    EncodedSection metadata{SandboxPackSectionType::metadata, 1, contracts::sandbox_pack_metadata_record_bytes};
    append_id(metadata.bytes, strings, definition.package_id);
    append_id(metadata.bytes, strings, definition.id);
    append_bounds(metadata.bytes, definition.bounds);
    append_id(metadata.bytes, strings, definition.completion_objective_id);
    sections.push_back(std::move(metadata));

    const auto regions = sorted_by_id(definition.regions);
    EncodedSection region_section{SandboxPackSectionType::regions, static_cast<std::uint32_t>(regions.size()), contracts::sandbox_pack_region_record_bytes};
    for (const auto& value : regions) {
        append_id(region_section.bytes, strings, value.id);
        append_bounds(region_section.bytes, value.bounds);
        append_zeroes(region_section.bytes, 8);
    }
    sections.push_back(std::move(region_section));

    const auto assets = sorted_by_id(definition.assets);
    EncodedSection asset_section{SandboxPackSectionType::assets, static_cast<std::uint32_t>(assets.size()), contracts::sandbox_pack_asset_record_bytes};
    for (const auto& value : assets) {
        append_id(asset_section.bytes, strings, value.id);
        append_le(asset_section.bytes, static_cast<std::uint8_t>(value.kind));
        append_zeroes(asset_section.bytes, 3);
    }
    sections.push_back(std::move(asset_section));

    EncodedSection player_section{SandboxPackSectionType::player, 1, contracts::sandbox_pack_player_record_bytes};
    append_id(player_section.bytes, strings, definition.player.id);
    append_id(player_section.bytes, strings, definition.player.region_id);
    append_id(player_section.bytes, strings, definition.player.asset_id);
    append_id(player_section.bytes, strings, definition.player.initial_safe_point_id);
    append_pose(player_section.bytes, definition.player.pose);
    append_le(player_section.bytes, definition.player.facing_millidegrees);
    append_zeroes(player_section.bytes, 4);
    sections.push_back(std::move(player_section));

    const auto append_placement = [&](EncodedSection& section, const auto& value) {
        append_id(section.bytes, strings, value.id);
        append_id(section.bytes, strings, value.region_id);
        append_id(section.bytes, strings, value.asset_id);
        append_pose(section.bytes, value.pose);
        append_le(section.bytes, value.facing_millidegrees);
        append_zeroes(section.bytes, 8);
    };

    const auto actors = sorted_by_id(definition.actors);
    EncodedSection actor_section{SandboxPackSectionType::actors, static_cast<std::uint32_t>(actors.size()), contracts::sandbox_pack_actor_record_bytes};
    for (const auto& value : actors) append_placement(actor_section, value);
    sections.push_back(std::move(actor_section));

    const auto blockers = sorted_by_id(definition.ground_blockers);
    EncodedSection blocker_section{SandboxPackSectionType::ground_blockers, static_cast<std::uint32_t>(blockers.size()), contracts::sandbox_pack_ground_blocker_record_bytes};
    for (const auto& value : blockers) {
        append_id(blocker_section.bytes, strings, value.id);
        append_id(blocker_section.bytes, strings, value.region_id);
        append_id(blocker_section.bytes, strings, value.asset_id);
        append_le(blocker_section.bytes, value.min_x);
        append_le(blocker_section.bytes, value.max_x);
        append_le(blocker_section.bytes, value.min_y);
        append_le(blocker_section.bytes, value.max_y);
        append_le(blocker_section.bytes, value.min_height);
        append_le(blocker_section.bytes, value.max_height);
        append_le(blocker_section.bytes, value.floor_layer);
        append_zeroes(blocker_section.bytes, 2);
    }
    sections.push_back(std::move(blocker_section));

    const auto safe_points = sorted_by_id(definition.safe_points);
    EncodedSection safe_section{SandboxPackSectionType::safe_points, static_cast<std::uint32_t>(safe_points.size()), contracts::sandbox_pack_safe_point_record_bytes};
    for (const auto& value : safe_points) append_placement(safe_section, value);
    sections.push_back(std::move(safe_section));

    const auto interactions = sorted_by_id(definition.interactions);
    EncodedSection interaction_section{SandboxPackSectionType::interactions, static_cast<std::uint32_t>(interactions.size()), contracts::sandbox_pack_interaction_record_bytes};
    for (const auto& value : interactions) append_placement(interaction_section, value);
    sections.push_back(std::move(interaction_section));

    const auto mechanisms = sorted_by_id(definition.mechanisms);
    EncodedSection mechanism_section{SandboxPackSectionType::mechanisms, static_cast<std::uint32_t>(mechanisms.size()), contracts::sandbox_pack_mechanism_record_bytes};
    for (const auto& value : mechanisms) append_placement(mechanism_section, value);
    sections.push_back(std::move(mechanism_section));

    const auto waves = sorted_by_id(definition.waves);
    EncodedSection wave_section{SandboxPackSectionType::waves, static_cast<std::uint32_t>(waves.size()), contracts::sandbox_pack_wave_record_bytes};
    for (const auto& value : waves) {
        append_id(wave_section.bytes, strings, value.id);
        append_id(wave_section.bytes, strings, value.region_id);
        append_id(wave_section.bytes, strings, value.predecessor_wave_id);
        append_le(wave_section.bytes, static_cast<std::uint8_t>(value.trigger.kind));
        append_zeroes(wave_section.bytes, 3);
        append_id(wave_section.bytes, strings, value.trigger.target_id);
        append_zeroes(wave_section.bytes, 12);
    }
    sections.push_back(std::move(wave_section));

    std::vector<contracts::SandboxWaveSpawnDefinition> wave_spawns(definition.wave_spawns.begin(), definition.wave_spawns.end());
    std::sort(wave_spawns.begin(), wave_spawns.end(), [](const auto& left, const auto& right) {
        return std::tuple{left.wave_id.key, left.wave_id.name, left.spawn_order, left.actor_id.key, left.actor_id.name, left.delay_ticks} <
               std::tuple{right.wave_id.key, right.wave_id.name, right.spawn_order, right.actor_id.key, right.actor_id.name, right.delay_ticks};
    });
    EncodedSection spawn_section{SandboxPackSectionType::wave_spawns, static_cast<std::uint32_t>(wave_spawns.size()), contracts::sandbox_pack_wave_spawn_record_bytes};
    for (const auto& value : wave_spawns) {
        append_id(spawn_section.bytes, strings, value.wave_id);
        append_id(spawn_section.bytes, strings, value.actor_id);
        append_le(spawn_section.bytes, value.delay_ticks);
        append_le(spawn_section.bytes, value.spawn_order);
        append_zeroes(spawn_section.bytes, 2);
    }
    sections.push_back(std::move(spawn_section));

    const auto objectives = sorted_by_id(definition.objectives);
    EncodedSection objective_section{SandboxPackSectionType::objectives, static_cast<std::uint32_t>(objectives.size()), contracts::sandbox_pack_objective_record_bytes};
    for (const auto& value : objectives) {
        append_id(objective_section.bytes, strings, value.id);
        append_id(objective_section.bytes, strings, value.region_id);
        append_id(objective_section.bytes, strings, value.predecessor_objective_id);
        append_le(objective_section.bytes, static_cast<std::uint8_t>(value.completion.kind));
        append_zeroes(objective_section.bytes, 3);
        append_id(objective_section.bytes, strings, value.completion.target_id);
        append_zeroes(objective_section.bytes, 12);
    }
    sections.push_back(std::move(objective_section));

    const auto interaction_bindings = sorted_bindings(binding.interaction_bindings, [](const auto& value) { return value.interaction_id; });
    EncodedSection interaction_binding_section{SandboxPackSectionType::interaction_gameplay_bindings, static_cast<std::uint32_t>(interaction_bindings.size()), contracts::sandbox_pack_interaction_gameplay_binding_record_bytes};
    for (const auto& value : interaction_bindings) {
        append_id(interaction_binding_section.bytes, strings, value.interaction_id);
        append_le(interaction_binding_section.bytes, static_cast<std::uint8_t>(value.operation));
        append_zeroes(interaction_binding_section.bytes, 3);
        append_le(interaction_binding_section.bytes, value.range_mm);
        append_id(interaction_binding_section.bytes, strings, value.target_mechanism_id);
    }
    sections.push_back(std::move(interaction_binding_section));

    const auto mechanism_bindings = sorted_bindings(binding.mechanism_bindings, [](const auto& value) { return value.mechanism_id; });
    EncodedSection mechanism_binding_section{SandboxPackSectionType::mechanism_gameplay_bindings, static_cast<std::uint32_t>(mechanism_bindings.size()), contracts::sandbox_pack_mechanism_gameplay_binding_record_bytes};
    for (const auto& value : mechanism_bindings) {
        append_id(mechanism_binding_section.bytes, strings, value.mechanism_id);
        append_le(mechanism_binding_section.bytes, static_cast<std::uint8_t>(value.activation));
        append_zeroes(mechanism_binding_section.bytes, 3);
        append_id(mechanism_binding_section.bytes, strings, value.target_ground_blocker_id);
        append_zeroes(mechanism_binding_section.bytes, 4);
    }
    sections.push_back(std::move(mechanism_binding_section));
    return sections;
}

}  // namespace

EncodeSandboxPackageResult encode_sandbox_package(
    const contracts::SandboxDefinition& definition,
    const contracts::SandboxGameplayBindingDefinition& gameplay_binding
) noexcept {
    EncodeSandboxPackageResult result{};
    result.validation = validate_sandbox_package(definition, gameplay_binding);
    if (!result.validation.valid()) {
        return result;
    }
    try {
        const auto strings = collect_strings(definition, gameplay_binding);
        if (strings.size() > contracts::sandbox_pack_max_strings) {
            result.validation.error = SandboxPackageError::invalid_string_table;
            return result;
        }
        std::size_t string_bytes = 0;
        for (const auto value : strings) {
            string_bytes += sizeof(std::uint16_t) + value.size();
        }
        if (string_bytes > contracts::sandbox_pack_max_string_bytes) {
            result.validation.error = SandboxPackageError::invalid_string_table;
            return result;
        }
        auto sections = encode_sections(definition, gameplay_binding, strings);
        const auto directory_bytes = sections.size() * contracts::sandbox_pack_directory_entry_bytes;
        result.bytes.assign(contracts::sandbox_pack_header_bytes + directory_bytes, 0);
        for (auto& section : sections) {
            align_to_pack(result.bytes);
            if (result.bytes.size() > std::numeric_limits<std::uint32_t>::max() ||
                section.bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
                result.validation.error = SandboxPackageError::pack_too_large;
                result.bytes.clear();
                return result;
            }
            section.offset = static_cast<std::uint32_t>(result.bytes.size());
            result.bytes.insert(result.bytes.end(), section.bytes.begin(), section.bytes.end());
        }
        if (result.bytes.size() > contracts::sandbox_pack_max_bytes) {
            result.validation.error = SandboxPackageError::pack_too_large;
            result.bytes.clear();
            return result;
        }

        std::copy(contracts::sandbox_pack_magic.begin(), contracts::sandbox_pack_magic.end(), result.bytes.begin());
        patch_le(result.bytes, 8, contracts::sandbox_pack_format_major);
        patch_le(result.bytes, 10, contracts::sandbox_pack_format_minor);
        patch_le(result.bytes, 12, static_cast<std::uint16_t>(contracts::sandbox_pack_header_bytes));
        patch_le(result.bytes, 14, static_cast<std::uint16_t>(contracts::sandbox_pack_directory_entry_bytes));
        patch_le(result.bytes, 16, std::uint32_t{0});
        patch_le(result.bytes, 20, contracts::sandbox_content_api_major);
        patch_le(result.bytes, 22, contracts::sandbox_content_api_minor);
        patch_le(result.bytes, 24, contracts::sandbox_authoring_schema_major);
        patch_le(result.bytes, 26, contracts::sandbox_authoring_schema_minor);
        patch_le(result.bytes, 28, contracts::sandbox_authoring_schema_patch);
        patch_le(result.bytes, 30, std::uint16_t{0});
        patch_le(result.bytes, 32, static_cast<std::uint32_t>(sections.size()));
        patch_le(result.bytes, 36, static_cast<std::uint32_t>(contracts::sandbox_pack_header_bytes));
        patch_le(result.bytes, 40, static_cast<std::uint32_t>(result.bytes.size()));
        patch_le(result.bytes, 44, std::uint32_t{0});
        patch_le(result.bytes, 48, definition.package_id.key);
        patch_le(result.bytes, 56, definition.id.key);

        for (std::size_t index = 0; index < sections.size(); ++index) {
            const auto entry = contracts::sandbox_pack_header_bytes +
                               index * contracts::sandbox_pack_directory_entry_bytes;
            const auto& section = sections[index];
            patch_le(result.bytes, entry, static_cast<std::uint16_t>(section.type));
            patch_le(result.bytes, entry + 2, std::uint16_t{1});
            patch_le(result.bytes, entry + 4, std::uint32_t{0});
            patch_le(result.bytes, entry + 8, section.count);
            patch_le(result.bytes, entry + 12, section.width);
            patch_le(result.bytes, entry + 16, section.offset);
            patch_le(result.bytes, entry + 20, static_cast<std::uint32_t>(section.bytes.size()));
        }
        result.fingerprint = contracts::sha256(result.bytes);
        std::copy(result.fingerprint.begin(), result.fingerprint.end(), result.bytes.begin() + static_cast<std::ptrdiff_t>(contracts::sandbox_pack_hash_offset));
        return result;
    } catch (const std::bad_alloc&) {
        result.validation.error = SandboxPackageError::allocation_failed;
        result.bytes.clear();
        result.fingerprint = {};
        return result;
    }
}

namespace {

class ByteReader final {
  public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) noexcept : bytes_(bytes) {}

    template <typename Integer>
    [[nodiscard]] bool read(Integer& value) noexcept {
        static_assert(std::is_integral_v<Integer>);
        if (remaining() < sizeof(Integer)) {
            return false;
        }
        using Unsigned = std::make_unsigned_t<Integer>;
        Unsigned bits = 0;
        for (std::size_t index = 0; index < sizeof(Integer); ++index) {
            bits = static_cast<Unsigned>(
                bits | static_cast<Unsigned>(
                           static_cast<std::uint64_t>(bytes_[offset_ + index]) <<
                           (index * 8U)
                       )
            );
        }
        value = static_cast<Integer>(bits);
        offset_ += sizeof(Integer);
        return true;
    }

    [[nodiscard]] bool read_zeroes(std::size_t count) noexcept {
        if (remaining() < count) {
            return false;
        }
        const auto values = bytes_.subspan(offset_, count);
        if (std::any_of(values.begin(), values.end(), [](std::uint8_t value) {
                return value != 0;
            })) {
            return false;
        }
        offset_ += count;
        return true;
    }

    [[nodiscard]] std::span<const std::uint8_t> take(std::size_t count) noexcept {
        if (remaining() < count) {
            return {};
        }
        const auto result = bytes_.subspan(offset_, count);
        offset_ += count;
        return result;
    }

    [[nodiscard]] std::size_t remaining() const noexcept {
        return bytes_.size() - offset_;
    }

  private:
    std::span<const std::uint8_t> bytes_{};
    std::size_t offset_{};
};

struct PackHeader final {
    std::uint16_t format_major{};
    std::uint16_t format_minor{};
    std::uint16_t header_bytes{};
    std::uint16_t directory_entry_bytes{};
    std::uint32_t flags{};
    std::uint16_t content_api_major{};
    std::uint16_t content_api_minor{};
    std::uint16_t schema_major{};
    std::uint16_t schema_minor{};
    std::uint16_t schema_patch{};
    std::uint16_t reserved0{};
    std::uint32_t section_count{};
    std::uint32_t directory_offset{};
    std::uint32_t total_bytes{};
    std::uint32_t reserved1{};
    contracts::StableContentKey package_key{};
    contracts::StableContentKey sandbox_key{};
    contracts::Sha256Digest fingerprint{};
};

struct DirectoryEntry final {
    std::uint16_t type{};
    std::uint16_t version{};
    std::uint32_t flags{};
    std::uint32_t count{};
    std::uint32_t width{};
    std::uint32_t offset{};
    std::uint32_t length{};
};

[[nodiscard]] bool bytes_are_zero(
    std::span<const std::uint8_t> bytes,
    std::size_t begin,
    std::size_t end
) noexcept {
    if (begin > end || end > bytes.size()) {
        return false;
    }
    return std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(begin),
                       bytes.begin() + static_cast<std::ptrdiff_t>(end),
                       [](std::uint8_t value) { return value == 0; });
}

[[nodiscard]] bool known_section(std::uint16_t type) noexcept {
    return type >= static_cast<std::uint16_t>(SandboxPackSectionType::strings) &&
           type <= static_cast<std::uint16_t>(
                       SandboxPackSectionType::mechanism_gameplay_bindings
                   );
}

[[nodiscard]] std::uint32_t expected_width(std::uint16_t raw_type) noexcept {
    switch (static_cast<SandboxPackSectionType>(raw_type)) {
        case SandboxPackSectionType::strings: return 0;
        case SandboxPackSectionType::metadata: return contracts::sandbox_pack_metadata_record_bytes;
        case SandboxPackSectionType::regions: return contracts::sandbox_pack_region_record_bytes;
        case SandboxPackSectionType::assets: return contracts::sandbox_pack_asset_record_bytes;
        case SandboxPackSectionType::player: return contracts::sandbox_pack_player_record_bytes;
        case SandboxPackSectionType::actors: return contracts::sandbox_pack_actor_record_bytes;
        case SandboxPackSectionType::ground_blockers: return contracts::sandbox_pack_ground_blocker_record_bytes;
        case SandboxPackSectionType::safe_points: return contracts::sandbox_pack_safe_point_record_bytes;
        case SandboxPackSectionType::interactions: return contracts::sandbox_pack_interaction_record_bytes;
        case SandboxPackSectionType::mechanisms: return contracts::sandbox_pack_mechanism_record_bytes;
        case SandboxPackSectionType::waves: return contracts::sandbox_pack_wave_record_bytes;
        case SandboxPackSectionType::wave_spawns: return contracts::sandbox_pack_wave_spawn_record_bytes;
        case SandboxPackSectionType::objectives: return contracts::sandbox_pack_objective_record_bytes;
        case SandboxPackSectionType::interaction_gameplay_bindings: return contracts::sandbox_pack_interaction_gameplay_binding_record_bytes;
        case SandboxPackSectionType::mechanism_gameplay_bindings: return contracts::sandbox_pack_mechanism_gameplay_binding_record_bytes;
    }
    return 0;
}

[[nodiscard]] std::size_t expected_capacity(std::uint16_t raw_type) noexcept {
    switch (static_cast<SandboxPackSectionType>(raw_type)) {
        case SandboxPackSectionType::strings: return contracts::sandbox_pack_max_strings;
        case SandboxPackSectionType::metadata:
        case SandboxPackSectionType::player: return 1;
        case SandboxPackSectionType::regions: return contracts::sandbox_region_capacity;
        case SandboxPackSectionType::assets: return contracts::sandbox_asset_capacity;
        case SandboxPackSectionType::actors: return contracts::sandbox_actor_capacity;
        case SandboxPackSectionType::ground_blockers: return contracts::sandbox_ground_blocker_capacity;
        case SandboxPackSectionType::safe_points: return contracts::sandbox_safe_point_capacity;
        case SandboxPackSectionType::interactions:
        case SandboxPackSectionType::interaction_gameplay_bindings: return contracts::sandbox_interaction_capacity;
        case SandboxPackSectionType::mechanisms:
        case SandboxPackSectionType::mechanism_gameplay_bindings: return contracts::sandbox_mechanism_capacity;
        case SandboxPackSectionType::waves: return contracts::sandbox_wave_capacity;
        case SandboxPackSectionType::wave_spawns: return contracts::sandbox_wave_spawn_capacity;
        case SandboxPackSectionType::objectives: return contracts::sandbox_objective_capacity;
    }
    return 0;
}

[[nodiscard]] SandboxPackageError parse_header(
    std::span<const std::uint8_t> bytes,
    PackHeader& header
) noexcept {
    if (bytes.size() > contracts::sandbox_pack_max_bytes) {
        return SandboxPackageError::pack_too_large;
    }
    if (bytes.size() < contracts::sandbox_pack_header_bytes) {
        return SandboxPackageError::truncated;
    }
    if (!std::equal(contracts::sandbox_pack_magic.begin(), contracts::sandbox_pack_magic.end(), bytes.begin())) {
        return SandboxPackageError::invalid_magic;
    }
    ByteReader reader(bytes.subspan(contracts::sandbox_pack_magic.size(), 56));
    if (!reader.read(header.format_major) || !reader.read(header.format_minor) ||
        !reader.read(header.header_bytes) || !reader.read(header.directory_entry_bytes) ||
        !reader.read(header.flags) || !reader.read(header.content_api_major) ||
        !reader.read(header.content_api_minor) || !reader.read(header.schema_major) ||
        !reader.read(header.schema_minor) || !reader.read(header.schema_patch) ||
        !reader.read(header.reserved0) || !reader.read(header.section_count) ||
        !reader.read(header.directory_offset) || !reader.read(header.total_bytes) ||
        !reader.read(header.reserved1) || !reader.read(header.package_key) ||
        !reader.read(header.sandbox_key) || reader.remaining() != 0) {
        return SandboxPackageError::truncated;
    }
    std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(contracts::sandbox_pack_hash_offset),
                contracts::sandbox_pack_hash_bytes,
                header.fingerprint.begin());
    if (header.format_major != contracts::sandbox_pack_format_major ||
        header.format_minor != contracts::sandbox_pack_format_minor ||
        header.content_api_major != contracts::sandbox_content_api_major ||
        header.content_api_minor != contracts::sandbox_content_api_minor ||
        header.schema_major != contracts::sandbox_authoring_schema_major ||
        header.schema_minor != contracts::sandbox_authoring_schema_minor ||
        header.schema_patch != contracts::sandbox_authoring_schema_patch) {
        return SandboxPackageError::unsupported_version;
    }
    if (header.header_bytes != contracts::sandbox_pack_header_bytes ||
        header.directory_entry_bytes != contracts::sandbox_pack_directory_entry_bytes ||
        header.directory_offset != contracts::sandbox_pack_header_bytes ||
        header.section_count < sandbox_known_section_count ||
        header.section_count > contracts::sandbox_pack_max_sections ||
        header.package_key == 0 || header.sandbox_key == 0) {
        return SandboxPackageError::invalid_header;
    }
    if (header.flags != 0 || header.reserved0 != 0 || header.reserved1 != 0) {
        return SandboxPackageError::invalid_reserved;
    }
    if (header.total_bytes < contracts::sandbox_pack_header_bytes) {
        return SandboxPackageError::invalid_header;
    }
    if (header.total_bytes > bytes.size()) {
        return SandboxPackageError::truncated;
    }
    if (header.total_bytes < bytes.size()) {
        return SandboxPackageError::trailing_bytes;
    }
    return SandboxPackageError::none;
}

[[nodiscard]] SandboxPackageError verify_hash(
    std::span<const std::uint8_t> bytes,
    const PackHeader& header
) {
    std::vector<std::uint8_t> hash_input(bytes.begin(), bytes.end());
    std::fill_n(hash_input.begin() + static_cast<std::ptrdiff_t>(contracts::sandbox_pack_hash_offset),
                contracts::sandbox_pack_hash_bytes,
                std::uint8_t{0});
    return contracts::sha256(hash_input) == header.fingerprint
               ? SandboxPackageError::none
               : SandboxPackageError::hash_mismatch;
}

[[nodiscard]] const DirectoryEntry* find_entry(
    std::span<const DirectoryEntry> entries,
    SandboxPackSectionType type
) noexcept {
    const auto raw = static_cast<std::uint16_t>(type);
    const auto found = std::find_if(entries.begin(), entries.end(), [raw](const auto& value) {
        return value.type == raw;
    });
    return found == entries.end() ? nullptr : &*found;
}

[[nodiscard]] SandboxPackageError parse_directory(
    std::span<const std::uint8_t> bytes,
    const PackHeader& header,
    std::vector<DirectoryEntry>& entries
) {
    const auto directory_bytes = static_cast<std::uint64_t>(header.section_count) *
                                 contracts::sandbox_pack_directory_entry_bytes;
    const auto directory_end = static_cast<std::uint64_t>(header.directory_offset) + directory_bytes;
    if (directory_end > bytes.size()) {
        return SandboxPackageError::truncated;
    }
    entries.reserve(header.section_count);
    std::array<bool, sandbox_known_section_count + 1> present{};
    ByteReader reader(bytes.subspan(header.directory_offset, static_cast<std::size_t>(directory_bytes)));
    std::uint16_t previous_type = 0;
    std::size_t previous_end = static_cast<std::size_t>(directory_end);
    for (std::uint32_t index = 0; index < header.section_count; ++index) {
        DirectoryEntry entry{};
        if (!reader.read(entry.type) || !reader.read(entry.version) || !reader.read(entry.flags) ||
            !reader.read(entry.count) || !reader.read(entry.width) || !reader.read(entry.offset) ||
            !reader.read(entry.length)) {
            return SandboxPackageError::truncated;
        }
        if (entry.type <= previous_type) {
            return entry.type == previous_type ? SandboxPackageError::duplicate_section
                                               : SandboxPackageError::invalid_directory;
        }
        previous_type = entry.type;
        if (known_section(entry.type)) {
            if (entry.version != 1) {
                return SandboxPackageError::unsupported_version;
            }
            if (entry.flags != 0 || entry.width != expected_width(entry.type) ||
                entry.count > expected_capacity(entry.type)) {
                return SandboxPackageError::invalid_section;
            }
            present[entry.type] = true;
            if (entry.type == static_cast<std::uint16_t>(SandboxPackSectionType::strings)) {
                if (entry.length > contracts::sandbox_pack_max_string_bytes) {
                    return SandboxPackageError::invalid_string_table;
                }
            } else {
                const auto expected_length = static_cast<std::uint64_t>(entry.count) * entry.width;
                if (expected_length != entry.length) {
                    return SandboxPackageError::invalid_section;
                }
            }
        } else if (entry.flags != contracts::sandbox_pack_section_optional) {
            return SandboxPackageError::unknown_required_section;
        }
        const auto section_end = static_cast<std::uint64_t>(entry.offset) + entry.length;
        if (entry.offset % contracts::sandbox_pack_alignment_bytes != 0 ||
            entry.offset < previous_end || section_end > bytes.size()) {
            return SandboxPackageError::invalid_directory;
        }
        if (!bytes_are_zero(bytes, previous_end, entry.offset)) {
            return SandboxPackageError::invalid_reserved;
        }
        previous_end = static_cast<std::size_t>(section_end);
        entries.push_back(entry);
    }
    if (reader.remaining() != 0) {
        return SandboxPackageError::invalid_directory;
    }
    if (previous_end != bytes.size()) {
        return SandboxPackageError::trailing_bytes;
    }
    for (std::uint16_t type = 1; type <= sandbox_known_section_count; ++type) {
        if (!present[type]) {
            return SandboxPackageError::missing_section;
        }
    }
    const auto* metadata = find_entry(entries, SandboxPackSectionType::metadata);
    const auto* player = find_entry(entries, SandboxPackSectionType::player);
    if (metadata == nullptr || player == nullptr || metadata->count != 1 || player->count != 1) {
        return SandboxPackageError::invalid_section;
    }
    return SandboxPackageError::none;
}

[[nodiscard]] SandboxPackageError parse_strings(
    std::span<const std::uint8_t> bytes,
    const DirectoryEntry& entry,
    std::vector<std::string>& strings
) {
    strings.reserve(entry.count);
    ByteReader reader(bytes.subspan(entry.offset, entry.length));
    for (std::uint32_t index = 0; index < entry.count; ++index) {
        std::uint16_t length = 0;
        if (!reader.read(length) || length == 0 ||
            length > contracts::sandbox_pack_max_id_bytes || reader.remaining() < length) {
            return SandboxPackageError::invalid_string_table;
        }
        const auto raw = reader.take(length);
        const std::string value(raw.begin(), raw.end());
        if (!valid_utf8(value) ||
            (!strings.empty() && !byte_less(strings.back(), value))) {
            return SandboxPackageError::invalid_string_table;
        }
        strings.push_back(value);
    }
    return reader.remaining() == 0 ? SandboxPackageError::none
                                   : SandboxPackageError::invalid_string_table;
}

[[nodiscard]] bool read_id(
    ByteReader& reader,
    std::span<const std::string> strings,
    ContentId& output
) noexcept {
    contracts::StableContentKey key = 0;
    std::uint32_t index = 0;
    if (!reader.read(key) || !reader.read(index)) {
        return false;
    }
    if (key == 0) {
        if (index != contracts::sandbox_pack_no_string_index) {
            return false;
        }
        output = {};
        return true;
    }
    if (index >= strings.size()) {
        return false;
    }
    output = {key, strings[index]};
    return contracts::stable_content_key(output.name) == output.key;
}

[[nodiscard]] bool read_bounds(ByteReader& reader, contracts::SandboxBoundsMm& output) noexcept {
    return reader.read(output.min_x) && reader.read(output.max_x) &&
           reader.read(output.min_y) && reader.read(output.max_y) &&
           reader.read(output.min_height) && reader.read(output.max_height) &&
           reader.read(output.min_floor_layer) && reader.read(output.max_floor_layer);
}

[[nodiscard]] bool read_pose(ByteReader& reader, contracts::GroundPoseMm& output) noexcept {
    return reader.read(output.x) && reader.read(output.y) && reader.read(output.height) &&
           reader.read(output.floor_layer) && reader.read_zeroes(2);
}

template <typename Definition>
[[nodiscard]] bool records_are_ordered(std::span<Definition> values) noexcept {
    for (std::size_t index = 1; index < values.size(); ++index) {
        const auto& previous = values[index - 1].id;
        const auto& current = values[index].id;
        if (std::tuple{current.key, current.name} < std::tuple{previous.key, previous.name}) {
            return false;
        }
    }
    return true;
}

template <typename Binding, typename IdGetter>
[[nodiscard]] bool bindings_are_ordered(
    std::span<Binding> values,
    IdGetter id
) noexcept {
    for (std::size_t index = 1; index < values.size(); ++index) {
        const auto previous = id(values[index - 1]);
        const auto current = id(values[index]);
        if (std::tuple{current.key, current.name} < std::tuple{previous.key, previous.name}) {
            return false;
        }
    }
    return true;
}

}  // namespace

SandboxPackageDocument::SandboxPackageDocument(std::unique_ptr<Storage> storage) noexcept
    : storage_(std::move(storage)) {}

SandboxPackageDocument::~SandboxPackageDocument() = default;

const contracts::SandboxDefinition& SandboxPackageDocument::definition() const noexcept {
    return storage_->definition;
}

const contracts::SandboxGameplayBindingDefinition&
SandboxPackageDocument::gameplay_binding() const noexcept {
    return storage_->gameplay_binding;
}

const contracts::Sha256Digest& SandboxPackageDocument::fingerprint() const noexcept {
    return storage_->fingerprint;
}

DecodeSandboxPackageResult decode_sandbox_package(std::span<const std::uint8_t> bytes) noexcept {
    DecodeSandboxPackageResult result{};
    const auto fail = [&](SandboxPackageError error) {
        result.validation = {};
        result.validation.error = error;
        result.document.reset();
    };
    try {
        PackHeader header{};
        auto error = parse_header(bytes, header);
        if (error != SandboxPackageError::none) { fail(error); return result; }
        error = verify_hash(bytes, header);
        if (error != SandboxPackageError::none) { fail(error); return result; }
        std::vector<DirectoryEntry> entries;
        error = parse_directory(bytes, header, entries);
        if (error != SandboxPackageError::none) { fail(error); return result; }

        auto storage = std::make_unique<SandboxPackageDocument::Storage>();
        storage->fingerprint = header.fingerprint;
        const auto* strings = find_entry(entries, SandboxPackSectionType::strings);
        if (strings == nullptr) { fail(SandboxPackageError::missing_section); return result; }
        error = parse_strings(bytes, *strings, storage->strings);
        if (error != SandboxPackageError::none) { fail(error); return result; }
        const auto entry = [&](SandboxPackSectionType type) {
            return find_entry(entries, type);
        };
        const auto reader_for = [&](SandboxPackSectionType type) {
            const auto* value = entry(type);
            return ByteReader(bytes.subspan(value->offset, value->length));
        };

        {
            auto reader = reader_for(SandboxPackSectionType::metadata);
            auto& value = storage->definition;
            if (!read_id(reader, storage->strings, value.package_id) ||
                !read_id(reader, storage->strings, value.id) ||
                !read_bounds(reader, value.bounds) ||
                !read_id(reader, storage->strings, value.completion_objective_id) ||
                reader.remaining() != 0 || value.package_id.key != header.package_key ||
                value.id.key != header.sandbox_key) {
                fail(SandboxPackageError::invalid_stable_id); return result;
            }
        }
        storage->regions.resize(entry(SandboxPackSectionType::regions)->count);
        {
            auto reader = reader_for(SandboxPackSectionType::regions);
            for (auto& value : storage->regions) {
                if (!read_id(reader, storage->strings, value.id) ||
                    !read_bounds(reader, value.bounds) || !reader.read_zeroes(8)) {
                    fail(SandboxPackageError::invalid_section); return result;
                }
            }
            if (reader.remaining() != 0 || !records_are_ordered(std::span{storage->regions})) {
                fail(SandboxPackageError::invalid_section); return result;
            }
        }
        storage->assets.resize(entry(SandboxPackSectionType::assets)->count);
        {
            auto reader = reader_for(SandboxPackSectionType::assets);
            for (auto& value : storage->assets) {
                std::uint8_t kind = 0;
                if (!read_id(reader, storage->strings, value.id) || !reader.read(kind) ||
                    !reader.read_zeroes(3)) {
                    fail(SandboxPackageError::invalid_section); return result;
                }
                value.kind = static_cast<contracts::SandboxAssetKind>(kind);
            }
            if (reader.remaining() != 0 || !records_are_ordered(std::span{storage->assets})) {
                fail(SandboxPackageError::invalid_section); return result;
            }
        }
        {
            auto reader = reader_for(SandboxPackSectionType::player);
            auto& value = storage->definition.player;
            if (!read_id(reader, storage->strings, value.id) ||
                !read_id(reader, storage->strings, value.region_id) ||
                !read_id(reader, storage->strings, value.asset_id) ||
                !read_id(reader, storage->strings, value.initial_safe_point_id) ||
                !read_pose(reader, value.pose) || !reader.read(value.facing_millidegrees) ||
                !reader.read_zeroes(4) || reader.remaining() != 0) {
                fail(SandboxPackageError::invalid_section); return result;
            }
        }
        const auto placements = [&](SandboxPackSectionType type, auto& values) {
            values.resize(entry(type)->count);
            auto reader = reader_for(type);
            for (auto& value : values) {
                if (!read_id(reader, storage->strings, value.id) ||
                    !read_id(reader, storage->strings, value.region_id) ||
                    !read_id(reader, storage->strings, value.asset_id) ||
                    !read_pose(reader, value.pose) || !reader.read(value.facing_millidegrees) ||
                    !reader.read_zeroes(8)) return false;
            }
            return reader.remaining() == 0 && records_are_ordered(std::span{values});
        };
        if (!placements(SandboxPackSectionType::actors, storage->actors) ||
            !placements(SandboxPackSectionType::safe_points, storage->safe_points) ||
            !placements(SandboxPackSectionType::interactions, storage->interactions) ||
            !placements(SandboxPackSectionType::mechanisms, storage->mechanisms)) {
            fail(SandboxPackageError::invalid_section); return result;
        }
        storage->ground_blockers.resize(entry(SandboxPackSectionType::ground_blockers)->count);
        {
            auto reader = reader_for(SandboxPackSectionType::ground_blockers);
            for (auto& value : storage->ground_blockers) {
                if (!read_id(reader, storage->strings, value.id) ||
                    !read_id(reader, storage->strings, value.region_id) ||
                    !read_id(reader, storage->strings, value.asset_id) ||
                    !reader.read(value.min_x) || !reader.read(value.max_x) ||
                    !reader.read(value.min_y) || !reader.read(value.max_y) ||
                    !reader.read(value.min_height) || !reader.read(value.max_height) ||
                    !reader.read(value.floor_layer) || !reader.read_zeroes(2)) {
                    fail(SandboxPackageError::invalid_section); return result;
                }
            }
            if (reader.remaining() != 0 ||
                !records_are_ordered(std::span{storage->ground_blockers})) {
                fail(SandboxPackageError::invalid_section); return result;
            }
        }
        storage->waves.resize(entry(SandboxPackSectionType::waves)->count);
        {
            auto reader = reader_for(SandboxPackSectionType::waves);
            for (auto& value : storage->waves) {
                std::uint8_t kind = 0;
                if (!read_id(reader, storage->strings, value.id) ||
                    !read_id(reader, storage->strings, value.region_id) ||
                    !read_id(reader, storage->strings, value.predecessor_wave_id) ||
                    !reader.read(kind) || !reader.read_zeroes(3) ||
                    !read_id(reader, storage->strings, value.trigger.target_id) ||
                    !reader.read_zeroes(12)) {
                    fail(SandboxPackageError::invalid_section); return result;
                }
                value.trigger.kind = static_cast<contracts::SandboxTriggerKind>(kind);
            }
            if (reader.remaining() != 0 || !records_are_ordered(std::span{storage->waves})) {
                fail(SandboxPackageError::invalid_section); return result;
            }
        }
        storage->wave_spawns.resize(entry(SandboxPackSectionType::wave_spawns)->count);
        {
            auto reader = reader_for(SandboxPackSectionType::wave_spawns);
            for (auto& value : storage->wave_spawns) {
                if (!read_id(reader, storage->strings, value.wave_id) ||
                    !read_id(reader, storage->strings, value.actor_id) ||
                    !reader.read(value.delay_ticks) || !reader.read(value.spawn_order) ||
                    !reader.read_zeroes(2)) {
                    fail(SandboxPackageError::invalid_section); return result;
                }
            }
            const auto less = [](const auto& left, const auto& right) {
                return std::tuple{left.wave_id.key, left.wave_id.name, left.spawn_order,
                                  left.actor_id.key, left.actor_id.name, left.delay_ticks} <
                       std::tuple{right.wave_id.key, right.wave_id.name, right.spawn_order,
                                  right.actor_id.key, right.actor_id.name, right.delay_ticks};
            };
            if (reader.remaining() != 0 ||
                !std::is_sorted(storage->wave_spawns.begin(), storage->wave_spawns.end(), less)) {
                fail(SandboxPackageError::invalid_section); return result;
            }
        }
        storage->objectives.resize(entry(SandboxPackSectionType::objectives)->count);
        {
            auto reader = reader_for(SandboxPackSectionType::objectives);
            for (auto& value : storage->objectives) {
                std::uint8_t kind = 0;
                if (!read_id(reader, storage->strings, value.id) ||
                    !read_id(reader, storage->strings, value.region_id) ||
                    !read_id(reader, storage->strings, value.predecessor_objective_id) ||
                    !reader.read(kind) || !reader.read_zeroes(3) ||
                    !read_id(reader, storage->strings, value.completion.target_id) ||
                    !reader.read_zeroes(12)) {
                    fail(SandboxPackageError::invalid_section); return result;
                }
                value.completion.kind =
                    static_cast<contracts::SandboxObjectiveCompletionKind>(kind);
            }
            if (reader.remaining() != 0 || !records_are_ordered(std::span{storage->objectives})) {
                fail(SandboxPackageError::invalid_section); return result;
            }
        }
        storage->interaction_bindings.resize(
            entry(SandboxPackSectionType::interaction_gameplay_bindings)->count
        );
        {
            auto reader = reader_for(SandboxPackSectionType::interaction_gameplay_bindings);
            for (auto& value : storage->interaction_bindings) {
                std::uint8_t operation = 0;
                if (!read_id(reader, storage->strings, value.interaction_id) ||
                    !reader.read(operation) || !reader.read_zeroes(3) ||
                    !reader.read(value.range_mm) ||
                    !read_id(reader, storage->strings, value.target_mechanism_id)) {
                    fail(SandboxPackageError::invalid_section); return result;
                }
                value.operation =
                    static_cast<contracts::SandboxInteractionOperation>(operation);
            }
            if (reader.remaining() != 0 ||
                !bindings_are_ordered(std::span{storage->interaction_bindings},
                                      [](const auto& value) { return value.interaction_id; })) {
                fail(SandboxPackageError::invalid_section); return result;
            }
        }
        storage->mechanism_bindings.resize(
            entry(SandboxPackSectionType::mechanism_gameplay_bindings)->count
        );
        {
            auto reader = reader_for(SandboxPackSectionType::mechanism_gameplay_bindings);
            for (auto& value : storage->mechanism_bindings) {
                std::uint8_t activation = 0;
                if (!read_id(reader, storage->strings, value.mechanism_id) ||
                    !reader.read(activation) || !reader.read_zeroes(3) ||
                    !read_id(reader, storage->strings, value.target_ground_blocker_id) ||
                    !reader.read_zeroes(4)) {
                    fail(SandboxPackageError::invalid_section); return result;
                }
                value.activation =
                    static_cast<contracts::SandboxMechanismActivation>(activation);
            }
            if (reader.remaining() != 0 ||
                !bindings_are_ordered(std::span{storage->mechanism_bindings},
                                      [](const auto& value) { return value.mechanism_id; })) {
                fail(SandboxPackageError::invalid_section); return result;
            }
        }
        storage->bind_views();
        result.validation = validate_sandbox_package(storage->definition, storage->gameplay_binding);
        if (!result.validation.valid()) return result;
        result.document = std::unique_ptr<SandboxPackageDocument>(
            new SandboxPackageDocument(std::move(storage))
        );
        return result;
    } catch (const std::bad_alloc&) {
        fail(SandboxPackageError::allocation_failed);
        return result;
    }
}

}  // namespace tgd::content
