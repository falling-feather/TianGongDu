#pragma once

#include <tgd/contracts/action_registry.generated.hpp>
#include <tgd/contracts/session_types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tgd::gameplay {

enum class SessionInputError : std::uint8_t {
    none,
    out_of_order_platform_sequence,
    unknown_action,
    invalid_sample,
    unsupported_gameplay_action,
    invalid_camera_basis,
    stale_camera_basis,
};

struct SessionCommandBatch final {
    std::array<contracts::SessionCommand, 2> commands{};
    std::size_t size{};
    bool interact_pressed{};

    [[nodiscard]] std::span<const contracts::SessionCommand> view() const noexcept {
        return std::span{commands}.first(size);
    }
};

class SessionInputState final {
  public:
    [[nodiscard]] SessionInputError submit(
        std::span<const contracts::ScalarActionSample> samples
    ) noexcept;
    [[nodiscard]] SessionInputError clear(
        contracts::PlatformSequence sequence,
        contracts::InputClearReason reason
    ) noexcept;
    [[nodiscard]] SessionInputError set_gameplay_enabled(
        bool enabled,
        contracts::PlatformSequence sequence
    ) noexcept;
    [[nodiscard]] SessionInputError set_camera_basis(
        const contracts::CameraBasisQ15& basis
    ) noexcept;
    [[nodiscard]] SessionCommandBatch commands_for_tick(
        contracts::TickIndex tick,
        contracts::StableActorKey actor,
        contracts::CommandSequence first_sequence
    ) noexcept;

    [[nodiscard]] contracts::PlatformSequence last_platform_sequence() const noexcept;
    [[nodiscard]] contracts::InputClearReason last_clear_reason() const noexcept;
    [[nodiscard]] const contracts::CameraBasisQ15& camera_basis() const noexcept;
    [[nodiscard]] bool gameplay_enabled() const noexcept;

  private:
    [[nodiscard]] SessionInputError validate_sample(
        const contracts::ScalarActionSample& sample
    ) const noexcept;
    void apply_sample(const contracts::ScalarActionSample& sample) noexcept;
    void clear_values(contracts::InputClearReason reason) noexcept;

    contracts::PlatformSequence last_platform_sequence_{};
    contracts::InputClearReason last_clear_reason_{contracts::InputClearReason::context_changed};
    contracts::CameraBasisQ15 camera_basis_{};
    std::int32_t move_x_{};
    std::int32_t move_y_{};
    bool jump_pressed_{};
    bool interact_pressed_{};
    bool gameplay_enabled_{true};
};

}  // namespace tgd::gameplay
