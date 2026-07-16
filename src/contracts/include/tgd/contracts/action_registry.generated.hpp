// Generated from content/design/action-registry.json. Do not edit by hand.
#pragma once

#include <tgd/contracts/input_action.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tgd::contracts {

struct InputContextDescriptor final {
    InputContextId id{};
    std::string_view name{};
    std::int32_t priority{};
    bool captures_text{};
};

struct ActionDescriptor final {
    ActionId id{};
    std::string_view name{};
    ActionValueType value_type{ActionValueType::digital};
    std::string_view edge_policy{};
    bool clear_on_blur{};
    std::array<InputContextId, 3> contexts{};
    std::uint8_t context_count{};
};

inline constexpr std::array<InputContextDescriptor, 5> input_context_descriptors{{
    {InputContextId{0xad730ef5U}, "gameplay", 10, false},
    {InputContextId{0xae3b2953U}, "dialogue", 20, false},
    {InputContextId{0x99e4dd3aU}, "menu", 30, false},
    {InputContextId{0xe851ca4dU}, "text_entry", 40, true},
    {InputContextId{0x491e0a9cU}, "system", 0, false},
}};

inline constexpr std::array<ActionDescriptor, 26> action_descriptors{{
    {ActionId{0x2f7d674bU}, "move_x", ActionValueType::axis1d,
     "continuous", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x2e7d65b8U}, "move_y", ActionValueType::axis1d,
     "continuous", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0xa73f5c0dU}, "jump", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x3f5ebd6cU}, "light_attack", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0xee0cd235U}, "heavy_attack", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x2bd1616eU}, "guard", ActionValueType::digital,
     "hold_or_toggle", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x3cf27f66U}, "evade", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x6841c9abU}, "weapon_skill", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0xa86c592fU}, "lantern", ActionValueType::digital,
     "hold_or_toggle", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x116b05cfU}, "interact", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x8f4e21c3U}, "stance_previous", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0xce7fcc03U}, "stance_next", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x57148a4bU}, "stance_direct_1", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x58148bdeU}, "stance_direct_2", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x59148d71U}, "stance_direct_3", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x57ede3eaU}, "switch_weapon", ActionValueType::digital,
     "press_or_hold", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x6610b9b2U}, "quick_item", ActionValueType::digital,
     "press_or_hold", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0xbdaf044cU}, "open_map", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x13a390c5U}, "open_journal", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xad730ef5U}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0xca6c5c17U}, "ui_navigate", ActionValueType::vector2,
     "repeat_with_ui_delay", true,
     {InputContextId{0xae3b2953U}, InputContextId{0x99e4dd3aU}, InputContextId{}}, 2},
    {ActionId{0xbb7352f4U}, "ui_confirm", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xae3b2953U}, InputContextId{0x99e4dd3aU}, InputContextId{}}, 2},
    {ActionId{0xd6192d4eU}, "ui_cancel", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0xae3b2953U}, InputContextId{0x99e4dd3aU}, InputContextId{0xe851ca4dU}}, 3},
    {ActionId{0x329aa1d1U}, "ui_tab_previous", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0x99e4dd3aU}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x1d7e8535U}, "ui_tab_next", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0x99e4dd3aU}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x7084d38dU}, "pause", ActionValueType::digital,
     "press_release", true,
     {InputContextId{0x491e0a9cU}, InputContextId{}, InputContextId{}}, 1},
    {ActionId{0x0fd5ead1U}, "text_input", ActionValueType::text,
     "text_commit", true,
     {InputContextId{0xe851ca4dU}, InputContextId{}, InputContextId{}}, 1},
}};

[[nodiscard]] constexpr const ActionDescriptor* find_action_descriptor(ActionId id) noexcept {
    for (const auto& descriptor : action_descriptors) {
        if (descriptor.id == id) {
            return &descriptor;
        }
    }
    return nullptr;
}

[[nodiscard]] constexpr bool action_supports_context(
    const ActionDescriptor& action,
    InputContextId context
) noexcept {
    for (std::size_t index = 0; index < action.context_count; ++index) {
        if (action.contexts[index] == context) {
            return true;
        }
    }
    return false;
}

}  // namespace tgd::contracts
