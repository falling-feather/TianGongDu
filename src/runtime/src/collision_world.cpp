#include <tgd/runtime/collision_world.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace tgd::runtime {
namespace {

[[nodiscard]] constexpr std::int32_t saturated_add(
    const std::int32_t value,
    const std::int32_t delta
) noexcept {
    const auto sum = static_cast<std::int64_t>(value) + static_cast<std::int64_t>(delta);
    return static_cast<std::int32_t>(std::clamp(
        sum,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
    ));
}

[[nodiscard]] constexpr bool intervals_overlap(
    const std::int32_t lhs_min,
    const std::int32_t lhs_max,
    const std::int32_t rhs_min,
    const std::int32_t rhs_max
) noexcept {
    return lhs_min < rhs_max && lhs_max > rhs_min;
}

[[nodiscard]] constexpr bool valid_blocker(const GroundBlocker& blocker) noexcept {
    return blocker.shape_id != 0U
        && blocker.min_x < blocker.max_x
        && blocker.min_y < blocker.max_y
        && blocker.min_height < blocker.max_height;
}

[[nodiscard]] constexpr bool valid_bounds(const GroundMovementBounds& bounds) noexcept {
    return bounds.min_x < bounds.max_x
        && bounds.min_y < bounds.max_y
        && bounds.min_height < bounds.max_height
        && bounds.min_floor_layer <= bounds.max_floor_layer;
}

[[nodiscard]] constexpr bool actor_vertical_overlap(
    const contracts::GroundPoseMm& pose,
    const std::int32_t actor_height,
    const GroundBlocker& blocker
) noexcept {
    const auto actor_top = saturated_add(pose.height, actor_height);
    return intervals_overlap(pose.height, actor_top, blocker.min_height, blocker.max_height);
}

[[nodiscard]] constexpr bool inside_axis_bounds(
    const std::int32_t center,
    const std::int32_t radius,
    const std::int32_t minimum,
    const std::int32_t maximum
) noexcept {
    const auto low = static_cast<std::int64_t>(center) - radius;
    const auto high = static_cast<std::int64_t>(center) + radius;
    return low >= minimum && high <= maximum;
}

}  // namespace

CollisionWorldError StaticCollisionWorld::configure(
    const std::span<const GroundBlocker> blockers
) noexcept {
    if (blockers.size() > max_blockers) {
        return CollisionWorldError::too_many_blockers;
    }

    std::array<GroundBlocker, max_blockers> candidate{};
    for (std::size_t index = 0; index < blockers.size(); ++index) {
        if (!valid_blocker(blockers[index])) {
            return CollisionWorldError::invalid_blocker;
        }
        candidate[index] = blockers[index];
    }
    std::sort(
        candidate.begin(),
        candidate.begin() + static_cast<std::ptrdiff_t>(blockers.size()),
        [](const GroundBlocker& left, const GroundBlocker& right) noexcept {
            return left.shape_id < right.shape_id;
        }
    );
    for (std::size_t index = 1; index < blockers.size(); ++index) {
        if (candidate[index - 1].shape_id == candidate[index].shape_id) {
            return CollisionWorldError::duplicate_shape_id;
        }
    }

    blockers_ = candidate;
    blocker_count_ = blockers.size();
    return CollisionWorldError::none;
}

CollisionWorldError StaticCollisionWorld::configure_bounds(
    const GroundMovementBounds& bounds
) noexcept {
    if (!valid_bounds(bounds)) {
        return CollisionWorldError::invalid_bounds;
    }
    bounds_ = bounds;
    has_bounds_ = true;
    return CollisionWorldError::none;
}

bool StaticCollisionWorld::set_blocker_enabled(
    const contracts::CollisionShapeId shape_id,
    const bool enabled
) noexcept {
    for (std::size_t index = 0; index < blocker_count_; ++index) {
        if (blockers_[index].shape_id == shape_id) {
            blockers_[index].enabled = enabled;
            return true;
        }
    }
    return false;
}

std::optional<bool> StaticCollisionWorld::blocker_enabled(
    const contracts::CollisionShapeId shape_id
) const noexcept {
    for (std::size_t index = 0; index < blocker_count_; ++index) {
        if (blockers_[index].shape_id == shape_id) {
            return blockers_[index].enabled;
        }
    }
    return std::nullopt;
}

std::size_t StaticCollisionWorld::blocker_count() const noexcept {
    return blocker_count_;
}

GroundMoveResolution StaticCollisionWorld::resolve_ground_move(
    const contracts::GroundPoseMm& pose,
    const std::int32_t delta_x,
    const std::int32_t delta_y,
    const std::int32_t actor_radius,
    const std::int32_t actor_height
) const noexcept {
    GroundMoveResolution result{pose, false, false};
    if (actor_radius < 0 || actor_height <= 0) {
        result.blocked_x = delta_x != 0;
        result.blocked_y = delta_y != 0;
        return result;
    }

    if (has_bounds_) {
        const auto top = static_cast<std::int64_t>(pose.height) + actor_height;
        if (pose.floor_layer < bounds_.min_floor_layer
            || pose.floor_layer > bounds_.max_floor_layer
            || pose.height < bounds_.min_height
            || top > bounds_.max_height) {
            result.blocked_x = delta_x != 0;
            result.blocked_y = delta_y != 0;
            return result;
        }
    }

    const auto target_x = saturated_add(pose.x, delta_x);
    bool blocked_x = has_bounds_
        && !inside_axis_bounds(target_x, actor_radius, bounds_.min_x, bounds_.max_x);
    if (!blocked_x) {
        const auto swept_min_x = saturated_add(std::min(pose.x, target_x), -actor_radius);
        const auto swept_max_x = saturated_add(std::max(pose.x, target_x), actor_radius);
        const auto actor_min_y = saturated_add(pose.y, -actor_radius);
        const auto actor_max_y = saturated_add(pose.y, actor_radius);
        for (std::size_t index = 0; index < blocker_count_; ++index) {
            const auto& blocker = blockers_[index];
            if (!blocker.enabled || blocker.floor_layer != pose.floor_layer
                || !actor_vertical_overlap(pose, actor_height, blocker)) {
                continue;
            }
            if (intervals_overlap(swept_min_x, swept_max_x, blocker.min_x, blocker.max_x)
                && intervals_overlap(actor_min_y, actor_max_y, blocker.min_y, blocker.max_y)) {
                blocked_x = true;
                break;
            }
        }
    }
    if (!blocked_x) {
        result.pose.x = target_x;
    }
    result.blocked_x = blocked_x;

    const auto target_y = saturated_add(pose.y, delta_y);
    bool blocked_y = has_bounds_
        && !inside_axis_bounds(target_y, actor_radius, bounds_.min_y, bounds_.max_y);
    if (!blocked_y) {
        const auto actor_min_x = saturated_add(result.pose.x, -actor_radius);
        const auto actor_max_x = saturated_add(result.pose.x, actor_radius);
        const auto swept_min_y = saturated_add(std::min(pose.y, target_y), -actor_radius);
        const auto swept_max_y = saturated_add(std::max(pose.y, target_y), actor_radius);
        for (std::size_t index = 0; index < blocker_count_; ++index) {
            const auto& blocker = blockers_[index];
            if (!blocker.enabled || blocker.floor_layer != pose.floor_layer
                || !actor_vertical_overlap(pose, actor_height, blocker)) {
                continue;
            }
            if (intervals_overlap(actor_min_x, actor_max_x, blocker.min_x, blocker.max_x)
                && intervals_overlap(swept_min_y, swept_max_y, blocker.min_y, blocker.max_y)) {
                blocked_y = true;
                break;
            }
        }
    }
    if (!blocked_y) {
        result.pose.y = target_y;
    }
    result.blocked_y = blocked_y;
    return result;
}

}  // namespace tgd::runtime
