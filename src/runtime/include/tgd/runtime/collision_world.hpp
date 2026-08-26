#pragma once

#include <tgd/contracts/session_types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tgd::runtime {

struct GroundMovementBounds final {
    std::int32_t min_x{};
    std::int32_t max_x{};
    std::int32_t min_y{};
    std::int32_t max_y{};
    std::int32_t min_height{};
    std::int32_t max_height{};
    std::int16_t min_floor_layer{};
    std::int16_t max_floor_layer{};
};

struct GroundBlocker final {
    contracts::CollisionShapeId shape_id{};
    std::int32_t min_x{};
    std::int32_t max_x{};
    std::int32_t min_y{};
    std::int32_t max_y{};
    std::int32_t min_height{};
    std::int32_t max_height{};
    std::int16_t floor_layer{};
    bool enabled{true};
};

enum class CollisionWorldError : std::uint8_t {
    none,
    too_many_blockers,
    invalid_blocker,
    duplicate_shape_id,
    invalid_bounds,
};

struct GroundMoveResolution final {
    contracts::GroundPoseMm pose{};
    bool blocked_x{};
    bool blocked_y{};
};

class ICollisionWorld {
  public:
    virtual ~ICollisionWorld() = default;

    [[nodiscard]] virtual GroundMoveResolution resolve_ground_move(
        const contracts::GroundPoseMm& pose,
        std::int32_t delta_x,
        std::int32_t delta_y,
        std::int32_t actor_radius,
        std::int32_t actor_height
    ) const noexcept = 0;
};

class StaticCollisionWorld final : public ICollisionWorld {
  public:
    static constexpr std::size_t max_blockers = 64;

    [[nodiscard]] CollisionWorldError configure(std::span<const GroundBlocker> blockers) noexcept;
    [[nodiscard]] CollisionWorldError configure_bounds(const GroundMovementBounds& bounds) noexcept;
    [[nodiscard]] bool set_blocker_enabled(
        contracts::CollisionShapeId shape_id,
        bool enabled
    ) noexcept;
    [[nodiscard]] std::optional<bool> blocker_enabled(
        contracts::CollisionShapeId shape_id
    ) const noexcept;
    [[nodiscard]] std::size_t blocker_count() const noexcept;

    [[nodiscard]] GroundMoveResolution resolve_ground_move(
        const contracts::GroundPoseMm& pose,
        std::int32_t delta_x,
        std::int32_t delta_y,
        std::int32_t actor_radius,
        std::int32_t actor_height
    ) const noexcept override;

  private:
    std::array<GroundBlocker, max_blockers> blockers_{};
    std::size_t blocker_count_{};
    GroundMovementBounds bounds_{};
    bool has_bounds_{};
};

}  // namespace tgd::runtime
