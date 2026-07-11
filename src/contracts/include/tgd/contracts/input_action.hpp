#pragma once

#include <cstdint>
#include <string_view>

namespace tgd::contracts {

using PlatformSequence = std::uint64_t;

[[nodiscard]] constexpr std::uint32_t stable_input_id(std::string_view name) noexcept {
    std::uint32_t hash = 2'166'136'261U;
    for (const auto character : name) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 16'777'619U;
    }
    return hash;
}

struct ActionId final {
    std::uint32_t value{};

    [[nodiscard]] friend constexpr bool operator==(const ActionId&, const ActionId&) noexcept =
        default;
};

struct InputContextId final {
    std::uint32_t value{};

    [[nodiscard]] friend constexpr bool operator==(
        const InputContextId&,
        const InputContextId&
    ) noexcept = default;
};

[[nodiscard]] constexpr ActionId action_id(std::string_view name) noexcept {
    return {stable_input_id(name)};
}

[[nodiscard]] constexpr InputContextId input_context_id(std::string_view name) noexcept {
    return {stable_input_id(name)};
}

enum class ActionValueType : std::uint8_t {
    digital,
    axis1d,
    vector2,
    text,
};

enum class ActionSampleEdge : std::uint8_t {
    value_changed,
    pressed,
    released,
};

enum class InputClearReason : std::uint8_t {
    blur,
    visibility_hidden,
    device_disconnected,
    context_changed,
    mapping_changed,
    pause,
};

struct ScalarActionSample final {
    PlatformSequence platform_sequence{};
    ActionId action{};
    std::int32_t value_q15{};
    ActionSampleEdge edge{ActionSampleEdge::value_changed};
    bool repeated{};
};

}  // namespace tgd::contracts
