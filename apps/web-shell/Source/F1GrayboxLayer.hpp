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

class F1GrayboxLayer final :
    public ax::Layer,
    public tgd::gameplay::ICombatEventSink,
    public tgd::gameplay::IQuestEventSink {
  public:
    CREATE_FUNC(F1GrayboxLayer);

    [[nodiscard]] bool init() override;
    void update(float delta_seconds) override;

    void clearInput(tgd::contracts::InputClearReason reason) noexcept;
    void shutdown() noexcept;
    void publish(std::span<const tgd::contracts::CombatEvent> events) noexcept override;
    void publish(std::span<const tgd::contracts::QuestEvent> events) noexcept override;
    [[nodiscard]] std::int32_t qaPlayerHealth() const noexcept;
    [[nodiscard]] bool qaPlayerActive() const noexcept;
    [[nodiscard]] std::uint32_t qaActiveHostiles() const noexcept;
    [[nodiscard]] std::uint32_t qaRetryCount() const noexcept;
    [[nodiscard]] std::uint32_t qaQuestBeatIndex() const noexcept;
    [[nodiscard]] std::uint32_t qaQuestCompletedObjectives() const noexcept;
    [[nodiscard]] std::uint32_t qaQuestRequiredObjectives() const noexcept;
    [[nodiscard]] std::uint32_t qaQuestSelectedChoices() const noexcept;
    [[nodiscard]] bool qaQuestResolved() const noexcept;
    [[nodiscard]] bool qaResolutionRewardReady() const noexcept;
    [[nodiscard]] std::int32_t qaSafePointPoseX() const noexcept;
    [[nodiscard]] std::int32_t qaSafePointPoseY() const noexcept;
    [[nodiscard]] std::int32_t qaPlayerPoseX() const noexcept;
    [[nodiscard]] std::int32_t qaPlayerPoseY() const noexcept;
    [[nodiscard]] std::uint32_t qaEligiblePlayTicks() const noexcept;
    [[nodiscard]] std::uint32_t qaIdleTicks() const noexcept;
    [[nodiscard]] std::uint32_t qaFailureRetryTicks() const noexcept;
    [[nodiscard]] std::uint32_t qaBeatTargetsMet() const noexcept;
    [[nodiscard]] bool qaPlayableTargetMet() const noexcept;
    [[nodiscard]] std::uint32_t qaIncomingAttackTicks() const noexcept;
    [[nodiscard]] bool qaPlayerBusy() const noexcept;

  private:
    struct PendingCombatIntent final {
        tgd::contracts::CombatCommandType type{tgd::contracts::CombatCommandType::light_attack};
        tgd::contracts::StableContentKey stance{};
    };

    static constexpr std::size_t hostile_capacity = 4;
    static constexpr std::size_t combat_intent_capacity = 8;
    static constexpr std::size_t quest_marker_capacity =
        tgd::gameplay::DeterministicQuestInteractionResolver::interaction_capacity;

    void createBackdrop();
    void createWorld();
    void createActors();
    void createHud();
    void createKeyboardInput();
    void simulateTick() noexcept;
    void updatePlayerPresentation() noexcept;
    void updateCombatPresentation() noexcept;
    void updateQuestPresentation() noexcept;
    void updateDirectionalKey(ax::EventKeyboard::KeyCode key, bool pressed) noexcept;
    [[nodiscard]] bool updateCombatKey(
        ax::EventKeyboard::KeyCode key,
        bool pressed
    ) noexcept;
    void updateJumpKey(bool pressed) noexcept;
    void updateInteractKey(bool pressed) noexcept;
    void submitQuestCombatSignal(const tgd::contracts::CombatEvent& event) noexcept;
    void submitQuestCombatOutcome(const tgd::contracts::CombatEvent& event) noexcept;
    void submitQuestBossPhase() noexcept;
    void syncBossStanceForQuest() noexcept;
    void submitAxisState() noexcept;
    void clearHeldInput(
        tgd::contracts::InputClearReason reason,
        bool release_guard
    ) noexcept;
    void queueCombatIntent(
        tgd::contracts::CombatCommandType type,
        tgd::contracts::StableContentKey stance = 0
    ) noexcept;
    [[nodiscard]] bool submitCombatTick(tgd::contracts::TickIndex tick) noexcept;
    [[nodiscard]] bool activateEncounterForBeat(
        tgd::contracts::StableContentKey beat
    ) noexcept;
    [[nodiscard]] bool retryEncounter() noexcept;
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
    tgd::gameplay::DeterministicQuestInteractionResolver quest_interactions_{};
    tgd::gameplay::DeterministicQuestCombatTriggerResolver quest_combat_triggers_{};
    tgd::gameplay::DeterministicQuestCombatOutcomeResolver quest_combat_outcomes_{};
    tgd::gameplay::DeterministicQuestBossPhaseResolver quest_boss_phases_{};
    tgd::gameplay::DeterministicQuestResolutionRewardResolver quest_resolution_rewards_{};
    tgd::gameplay::SessionInputState input_{};
    tgd::contracts::PlatformSequence platform_sequence_{};
    tgd::contracts::CommandSequence command_sequence_{1};
    tgd::contracts::CommandSequence combat_command_sequence_{1};
    tgd::contracts::CommandSequence encounter_command_sequence_{1};
    tgd::contracts::CommandSequence retry_command_sequence_{1};
    std::array<bool, 8> directional_keys_{};
    std::array<bool, 7> combat_keys_{};
    std::array<PendingCombatIntent, combat_intent_capacity> combat_intents_{};
    std::size_t combat_intent_count_{};
    std::int32_t submitted_move_x_{};
    std::int32_t submitted_move_y_{};
    bool jump_pressed_{};
    bool interact_pressed_{};
    bool player_defeated_{};
    bool retry_requested_{};
    std::uint32_t retry_count_{};
    tgd::contracts::TickIndex incoming_attack_tick_{};
    tgd::contracts::StableActorKey incoming_attack_source_{};
    tgd::contracts::StableContentKey pending_boss_stance_{};
    tgd::contracts::StableContentKey resolution_reward_{};
    tgd::contracts::StableContentKey resolution_reward_dedup_key_{};
    float tick_accumulator_{};

    ax::Node* world_layer_{};
    ax::Node* player_node_{};
    ax::DrawNode* boss_phase_aura_{};
    std::array<ax::Node*, hostile_capacity> hostile_nodes_{};
    std::array<tgd::contracts::StableActorKey, hostile_capacity> hostile_actor_keys_{};
    std::array<ax::Node*, quest_marker_capacity> quest_marker_nodes_{};
    std::array<tgd::contracts::StableContentKey, quest_marker_capacity>
        quest_marker_objectives_{};
    std::size_t quest_marker_count_{};
    ax::DrawNode* combat_fx_{};
    ax::DrawNode* foreground_awning_{};
    ax::Label* combat_resources_label_{};
    ax::Label* playtime_audit_label_{};
    ax::Label* quest_state_label_{};
    ax::Label* interaction_prompt_label_{};
    ax::Label* combat_event_label_{};
    std::array<std::uint8_t, hostile_capacity> hostile_flash_ticks_{};
    std::uint8_t player_action_ticks_{};
    std::uint8_t player_hit_ticks_{};
    std::uint8_t attack_fx_ticks_{};
};

[[nodiscard]] ax::Scene* createF1GrayboxScene(F1GrayboxLayer** layer_out);
