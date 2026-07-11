#pragma once

#include <axmol.h>

#include <tgd/content/content_definition_provider.hpp>
#include <tgd/contracts/input_action.hpp>
#include <tgd/gameplay/combat_resolver.hpp>
#include <tgd/gameplay/encounter_director.hpp>
#include <tgd/gameplay/session_input_state.hpp>
#include <tgd/gameplay/vertical_slice_session.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

class F1GrayboxLayer final : public ax::Layer, public tgd::gameplay::ICombatEventSink {
  public:
    CREATE_FUNC(F1GrayboxLayer);

    [[nodiscard]] bool init() override;
    void update(float delta_seconds) override;

    void clearInput(tgd::contracts::InputClearReason reason) noexcept;
    void shutdown() noexcept;
    void publish(std::span<const tgd::contracts::CombatEvent> events) noexcept override;

  private:
    struct PendingCombatIntent final {
        tgd::contracts::CombatCommandType type{tgd::contracts::CombatCommandType::light_attack};
        tgd::contracts::StableContentKey stance{};
    };

    static constexpr std::size_t hostile_capacity = 3;
    static constexpr std::size_t combat_intent_capacity = 8;

    void createBackdrop();
    void createWorld();
    void createActors();
    void createHud();
    void createKeyboardInput();
    void simulateTick() noexcept;
    void updatePlayerPresentation() noexcept;
    void updateCombatPresentation() noexcept;
    void updateDirectionalKey(ax::EventKeyboard::KeyCode key, bool pressed) noexcept;
    [[nodiscard]] bool updateCombatKey(
        ax::EventKeyboard::KeyCode key,
        bool pressed
    ) noexcept;
    void updateJumpKey(bool pressed) noexcept;
    void submitAxisState() noexcept;
    void queueCombatIntent(
        tgd::contracts::CombatCommandType type,
        tgd::contracts::StableContentKey stance = 0
    ) noexcept;
    [[nodiscard]] bool submitCombatTick(tgd::contracts::TickIndex tick) noexcept;
    void refreshCombatHud() noexcept;

    [[nodiscard]] ax::Vec2 project(const tgd::contracts::GroundPoseMm& pose) const noexcept;
    [[nodiscard]] int directionalKeyIndex(ax::EventKeyboard::KeyCode key) const noexcept;
    [[nodiscard]] int combatKeyIndex(ax::EventKeyboard::KeyCode key) const noexcept;
    [[nodiscard]] int depthOrder(float screen_y) const noexcept;
    [[nodiscard]] tgd::contracts::StableActorKey nearestActiveHostile() const noexcept;

    tgd::content::BuiltInF1ContentDefinitionProvider content_{};
    const tgd::contracts::VerticalSliceDefinition* definition_{};
    const tgd::contracts::CombatEncounterDefinition* combat_definition_{};
    tgd::gameplay::VerticalSliceSession session_{};
    tgd::gameplay::DeterministicCombatResolver combat_{};
    tgd::gameplay::DeterministicEncounterDirector encounter_{};
    tgd::gameplay::SessionInputState input_{};
    tgd::contracts::PlatformSequence platform_sequence_{};
    tgd::contracts::CommandSequence command_sequence_{1};
    tgd::contracts::CommandSequence combat_command_sequence_{1};
    tgd::contracts::CommandSequence encounter_command_sequence_{1};
    std::array<bool, 8> directional_keys_{};
    std::array<bool, 7> combat_keys_{};
    std::array<PendingCombatIntent, combat_intent_capacity> combat_intents_{};
    std::size_t combat_intent_count_{};
    std::int32_t submitted_move_x_{};
    std::int32_t submitted_move_y_{};
    bool jump_pressed_{};
    float tick_accumulator_{};

    ax::Node* world_layer_{};
    ax::Node* player_node_{};
    std::array<ax::Node*, hostile_capacity> hostile_nodes_{};
    std::array<tgd::contracts::StableActorKey, hostile_capacity> hostile_actor_keys_{};
    ax::DrawNode* combat_fx_{};
    ax::DrawNode* foreground_awning_{};
    ax::Label* combat_resources_label_{};
    ax::Label* combat_event_label_{};
    std::array<std::uint8_t, hostile_capacity> hostile_flash_ticks_{};
    std::uint8_t player_action_ticks_{};
    std::uint8_t player_hit_ticks_{};
    std::uint8_t attack_fx_ticks_{};
};

[[nodiscard]] ax::Scene* createF1GrayboxScene(F1GrayboxLayer** layer_out);
