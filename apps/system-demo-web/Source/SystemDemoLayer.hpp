#pragma once

#include <axmol.h>

#include <tgd/contracts/sandbox_definition.hpp>
#include <tgd/gameplay/craft_session.hpp>
#include <tgd/gameplay/sandbox_encounter_session.hpp>
#include <tgd/integration/sandbox_runtime_coordinator.hpp>
#include <tgd/presentation/sandbox_asset_resolver.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

class SystemDemoLayer final : public ax::Layer {
public:
  CREATE_FUNC(SystemDemoLayer);

  ~SystemDemoLayer() override;

  [[nodiscard]] bool init() override;
  void update(float delta_seconds) override;

  [[nodiscard]] static SystemDemoLayer *active() noexcept;
  [[nodiscard]] bool qaReady() const noexcept;
  [[nodiscard]] std::int32_t qaPlayerX() const noexcept;
  [[nodiscard]] std::int32_t qaPlayerY() const noexcept;
  [[nodiscard]] bool qaGateOpen() const noexcept;
  [[nodiscard]] std::uint32_t qaRetryCount() const noexcept;
  [[nodiscard]] std::uint32_t qaBlockedMoveCount() const noexcept;
  [[nodiscard]] std::uint32_t qaPackageByteCount() const noexcept;
  [[nodiscard]] std::uint32_t qaAssetCount() const noexcept;
  [[nodiscard]] std::int32_t qaPlayerHealth() const noexcept;
  [[nodiscard]] std::uint32_t qaActiveHostileCount() const noexcept;
  [[nodiscard]] std::uint32_t qaDefeatedHostileCount() const noexcept;
  [[nodiscard]] std::uint32_t qaCompletedWaveCount() const noexcept;
  [[nodiscard]] std::uint32_t qaCompletedObjectiveCount() const noexcept;
  [[nodiscard]] bool qaTerminalCompleted() const noexcept;
  [[nodiscard]] std::uint32_t qaAcceptedAttackCount() const noexcept;
  [[nodiscard]] std::uint32_t qaRepeatedTriggerCount() const noexcept;
  [[nodiscard]] bool qaCraftMode() const noexcept;
  [[nodiscard]] bool qaCraftInRange() const noexcept;
  [[nodiscard]] std::uint32_t qaCraftStage() const noexcept;
  [[nodiscard]] std::uint32_t qaCraftSelectedMaterial() const noexcept;
  [[nodiscard]] std::uint32_t qaCraftCompletedOperationCount() const noexcept;
  [[nodiscard]] std::uint32_t qaCraftTrialCount() const noexcept;
  [[nodiscard]] std::uint32_t qaCraftMistakeCount() const noexcept;
  [[nodiscard]] std::uint32_t qaCraftReworkCount() const noexcept;
  [[nodiscard]] bool qaCraftCompleted() const noexcept;
  void qaOperate() noexcept;
  void qaAttackLight() noexcept;
  void qaAttackHeavy() noexcept;
  void qaRetry() noexcept;

private:
  struct ActorPresentation final {
    tgd::contracts::StableActorKey actor{};
    ax::Sprite *sprite{};
    ax::Label *status{};
  };

  enum class Direction : std::size_t {
    left = 0,
    right = 1,
    forward = 2,
    back = 3,
  };

  [[nodiscard]] bool loadRuntimeAndAssets() noexcept;
  void createBackdrop();
  void createWorldPresentation();
  void createRegistryPanel();
  void createHud();
  void createCraftPanel();
  void createKeyboardInput();
  void applyMovementStep() noexcept;
  void advanceEncounterStep() noexcept;
  void submitOperate() noexcept;
  void submitAttack(tgd::gameplay::SandboxEncounterAttack attack) noexcept;
  void toggleCraftMode() noexcept;
  void selectCraftMaterial(std::size_t choice_index) noexcept;
  void performCraftOperation(std::size_t operation_index) noexcept;
  void runCraftTrial() noexcept;
  void performCraftRework() noexcept;
  void retryLocal() noexcept;
  void refreshPresentation() noexcept;
  void setDirection(ax::EventKeyboard::KeyCode key, bool down) noexcept;

  [[nodiscard]] ax::Vec2
  project(const tgd::contracts::GroundPoseMm &pose) const noexcept;
  [[nodiscard]] int depthOrder(float screen_y) const noexcept;
  [[nodiscard]] ax::Sprite *createAssetSprite(
      std::string_view stable_id, tgd::contracts::StableContentKey stable_key,
      tgd::contracts::SandboxAssetKind kind, float target_extent) const;
  [[nodiscard]] float
  sceneAssetExtent(tgd::contracts::SandboxAssetKind kind) const noexcept;
  [[nodiscard]] bool playerInOperateRange() const noexcept;
  [[nodiscard]] bool playerInCraftRange() const noexcept;
  [[nodiscard]] bool currentGateOpen() const noexcept;
  [[nodiscard]] std::string activeWaveName() const;

  tgd::integration::SandboxRuntimeCoordinator coordinator_{};
  tgd::gameplay::SandboxEncounterSession encounter_{};
  tgd::gameplay::CraftSession craft_session_{};
  tgd::presentation::SandboxAssetResolver asset_resolver_{};
  std::array<ActorPresentation,
             tgd::gameplay::SandboxEncounterSession::hostile_capacity>
      actor_presentations_{};
  std::size_t actor_presentation_count_{};
  std::array<bool, 4> directions_{};
  float fixed_step_accumulator_{};
  std::uint32_t retry_count_{};
  std::uint32_t blocked_move_count_{};
  std::uint32_t package_byte_count_{};
  std::uint32_t asset_count_{};
  bool ready_{};
  bool gate_open_{};
  bool craft_mode_{};

  tgd::contracts::StableContentKey interaction_key_{};
  tgd::contracts::StableContentKey target_mechanism_key_{};
  tgd::contracts::GroundPoseMm interaction_pose_{};
  std::int32_t interaction_range_mm_{};
  std::size_t gate_blocker_index_{};
  std::array<tgd::contracts::StableContentKey, 2> craft_material_keys_{};
  std::array<tgd::contracts::StableContentKey, 2> craft_operation_keys_{};
  tgd::contracts::StableContentKey craft_workstation_asset_key_{};
  tgd::contracts::GroundPoseMm craft_workstation_pose_{};
  std::string craft_workstation_asset_id_{};

  ax::Node *world_layer_{};
  ax::Sprite *player_node_{};
  ax::Sprite *gate_node_{};
  ax::Label *gate_status_label_{};
  ax::Label *player_status_label_{};
  ax::Label *wave_status_label_{};
  ax::Label *objective_status_label_{};
  ax::Label *prompt_label_{};
  ax::Label *message_label_{};
  ax::Sprite *craft_workstation_node_{};
  ax::Label *craft_workstation_status_label_{};
  ax::Node *craft_panel_{};
  ax::Label *craft_stage_label_{};
  ax::Label *craft_material_label_{};
  ax::Label *craft_steps_label_{};
  ax::Label *craft_hint_label_{};
  std::string message_{"BOOTING UNIQUE SYSTEM DEMO PACKAGE"};
};

[[nodiscard]] ax::Scene *createSystemDemoScene();
