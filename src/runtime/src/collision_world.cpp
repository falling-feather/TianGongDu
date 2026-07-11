#include <tgd/runtime/collision_world.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>

namespace tgd::runtime {
namespace {

[[nodiscard]] std::int32_t saturating_add(std::int32_t value, std::int32_t delta) noexcept {
    const auto sum = static_cast<std::int64_t>(value) + static_cast<std::int64_t>(delta);
    return static_cast<std::int32_t>(std::clamp(
        sum,
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
        static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
    ));
}

[[nodiscard]] bool overlaps(
    const contracts::GroundPoseMm& pose,
    std::int32_t actor_radius,
    std::int32_t actor_height,
    const GroundBlocker& blocker
) noexcept {
    if (pose.floor_layer != blocker.floor_layer) {
        return false;
    }

    const auto actor_min_x = static_cast<std::int64_t>(pose.x) - actor_radius;
    const auto actor_max_x = static_cast<std::int64_t>(pose.x) + actor_radius;
    const auto actor_min_y = static_cast<std::int64_t>(pose.y) - actor_radius;
    const auto actor_max_y = static_cast<std::int64_t>(pose.y) + actor_radius;
    const auto actor_min_height = static_cast<std::int64_t>(pose.height);
    const auto actor_max_height = actor_min_height + actor_height;

    return actor_max_x > blocker.min_x && actor_min_x < blocker.max_x &&
           actor_max_y > blocker.min_y && actor_min_y < blocker.max_y &&
           actor_max_height > blocker.min_height && actor_min_height < blocker.max_height;
}

}  // namespace

CollisionWorldError StaticCollisionWorld::configure(
    std::span<const GroundBlocker> blockers
) noexcept {
    if (blockers.size() > max_blockers) {
        return CollisionWorldError::too_many_blockers;
    }

    for (std::size_t index = 0; index < blockers.size(); ++index) {
        const auto& blocker = blockers[index];
        if (blocker.shape_id == 0 || blocker.min_x >= blocker.max_x ||
            blocker.min_y >= blocker.max_y || blocker.min_height >= blocker.max_height) {
            return CollisionWorldError::invalid_blocker;
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (blockers[previous].shape_id == blocker.shape_id) {
                return CollisionWorldError::duplicate_shape_id;
            }
        }
    }

    blocker_count_ = blockers.size();
    std::copy(blockers.begin(), blockers.end(), blockers_.begin());
    std::sort(
        blockers_.begin(),
        blockers_.begin() + static_cast<std::ptrdiff_t>(blocker_count_),
        [](const GroundBlocker& left, const GroundBlocker& right) {
            return left.shape_id < right.shape_id;
        }
    );
    return CollisionWorldError::none;
}

std::size_t StaticCollisionWorld::blocker_count() const noexcept {
    return blocker_count_;
}

GroundMoveResolution StaticCollisionWorld::resolve_ground_move(
    const contracts::GroundPoseMm& pose,
    std::int32_t delta_x,
    std::int32_t delta_y,
    std::int32_t actor_radius,
    std::int32_t actor_height
) const noexcept {
    GroundMoveResolution result{pose, false, false};

    result.pose.x = saturating_add(result.pose.x, delta_x);
    for (std::size_t index = 0; index < blocker_count_; ++index) {
        if (overlaps(result.pose, actor_radius, actor_height, blockers_[index])) {
            result.pose.x = pose.x;
            result.blocked_x = true;
            break;
        }
    }

    result.pose.y = saturating_add(result.pose.y, delta_y);
    for (std::size_t index = 0; index < blocker_count_; ++index) {
        if (overlaps(result.pose, actor_radius, actor_height, blockers_[index])) {
            result.pose.y = pose.y;
            result.blocked_y = true;
            break;
        }
    }

    return result;
}

}  // namespace tgd::runtime
