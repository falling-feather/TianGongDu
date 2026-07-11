#pragma once

#include <axmol.h>

#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/input_action.hpp>
#include <tgd/gameplay/session_input_state.hpp>
#include <tgd/gameplay/vertical_slice_session.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

class F1GrayboxLayer final : public ax::Layer {
  public:
    CREATE_FUNC(F1GrayboxLayer);

    [[nodiscard]] bool init() override;
    void update(float delta_seconds) override;

    void clearInput(tgd::contracts::InputClearReason reason) noexcept;
    void shutdown() noexcept;

  private:
    void createBackdrop();
    void createWorld();
    void createActors();
    void createHud();
    void createKeyboardInput();
    void simulateTick() noexcept;
    void updatePlayerPresentation() noexcept;
    void updateDirectionalKey(ax::EventKeyboard::KeyCode key, bool pressed) noexcept;
    void updateJumpKey(bool pressed) noexcept;
    void submitAxisState() noexcept;

    [[nodiscard]] ax::Vec2 project(const tgd::contracts::GroundPoseMm& pose) const noexcept;
    [[nodiscard]] int directionalKeyIndex(ax::EventKeyboard::KeyCode key) const noexcept;
    [[nodiscard]] int depthOrder(float screen_y) const noexcept;

    tgd::content::BuiltInF1ContentDefinitionProvider content_{};
    const tgd::contracts::VerticalSliceDefinition* definition_{};
    tgd::gameplay::VerticalSliceSession session_{};
    tgd::gameplay::SessionInputState input_{};
    tgd::contracts::PlatformSequence platform_sequence_{};
    tgd::contracts::CommandSequence command_sequence_{1};
    std::array<bool, 8> directional_keys_{};
    std::int32_t submitted_move_x_{};
    std::int32_t submitted_move_y_{};
    bool jump_pressed_{};
    float tick_accumulator_{};

    ax::Node* world_layer_{};
    ax::Node* player_node_{};
    ax::DrawNode* foreground_awning_{};
};

[[nodiscard]] ax::Scene* createF1GrayboxScene(F1GrayboxLayer** layer_out);
