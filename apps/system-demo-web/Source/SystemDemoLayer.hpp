#pragma once

#include <axmol.h>

#include <tgd/contracts/sandbox_definition.hpp>
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

private:
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
  void createKeyboardInput();
  void applyMovementStep() noexcept;
  void submitOperate() noexcept;
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
  [[nodiscard]] bool currentGateOpen() const noexcept;

  tgd::integration::SandboxRuntimeCoordinator coordinator_{};
  tgd::presentation::SandboxAssetResolver asset_resolver_{};
  std::array<bool, 4> directions_{};
  float fixed_step_accumulator_{};
  std::uint32_t retry_count_{};
  std::uint32_t blocked_move_count_{};
  std::uint32_t package_byte_count_{};
  std::uint32_t asset_count_{};
  bool ready_{};
  bool gate_open_{};

  tgd::contracts::StableContentKey interaction_key_{};
  tgd::contracts::GroundPoseMm interaction_pose_{};
  std::int32_t interaction_range_mm_{};

  ax::Node *world_layer_{};
  ax::Sprite *player_node_{};
  ax::Sprite *gate_node_{};
  ax::Label *gate_status_label_{};
  ax::Label *player_status_label_{};
  ax::Label *prompt_label_{};
  ax::Label *message_label_{};
  std::string message_{"BOOTING UNIQUE SYSTEM DEMO PACKAGE"};
};

[[nodiscard]] ax::Scene *createSystemDemoScene();
