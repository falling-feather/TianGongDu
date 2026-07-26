#include "SystemDemoLayer.hpp"

#include <tgd/content/sandbox_package.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/sandbox_gameplay_binding.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define TGD_SYSTEM_DEMO_KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
#define TGD_SYSTEM_DEMO_KEEPALIVE
#endif

namespace {

using tgd::contracts::GroundPoseMm;
using tgd::contracts::SandboxAssetKind;
using tgd::contracts::StableActorKey;
using tgd::contracts::StableContentKey;
using tgd::integration::SandboxRuntimeCommandDisposition;

constexpr std::string_view package_path = "/system-demo.tgdsbx";
constexpr std::string_view workbench_preview_package_path =
    "/workbench-preview.tgdsbx";
constexpr StableActorKey player_actor = 0x706c617965720001ULL;
constexpr float design_width = 1280.0F;
constexpr float design_height = 720.0F;
constexpr float fixed_step_seconds = 1.0F / 60.0F;
constexpr std::int32_t cardinal_move_mm = 60;
constexpr std::int32_t diagonal_move_mm = 42;
constexpr std::int32_t craft_interaction_range_mm = 1'200;

SystemDemoLayer *active_layer{};

[[nodiscard]] ax::Color4F color(const std::uint8_t red,
                                const std::uint8_t green,
                                const std::uint8_t blue,
                                const std::uint8_t alpha = 255) noexcept {
  constexpr float component_scale = 1.0F / 255.0F;
  return {
      static_cast<float>(red) * component_scale,
      static_cast<float>(green) * component_scale,
      static_cast<float>(blue) * component_scale,
      static_cast<float>(alpha) * component_scale,
  };
}

void solidPolygon(ax::DrawNode *draw,
                  const std::initializer_list<ax::Vec2> points,
                  const ax::Color4F &fill,
                  const ax::Color4F &border = ax::Color4F::TRANSPARENT,
                  const float border_width = 0.0F) {
  draw->drawSolidPoly(points.begin(), static_cast<unsigned int>(points.size()),
                      fill, border_width, border);
}

[[nodiscard]] ax::Label *makeLabel(const std::string_view text,
                                   const float size, const ax::Color4B tint,
                                   const ax::Vec2 anchor = {0.0F, 1.0F}) {
  auto *label = ax::Label::createWithSystemFont(text, "Arial", size);
  if (label != nullptr) {
    label->setTextColor(tint);
    label->setAnchorPoint(anchor);
  }
  return label;
}

[[nodiscard]] std::vector<std::uint8_t> readBytes(const std::string_view path) {
  const std::string owned_path{path};
  std::ifstream input{owned_path, std::ios::binary | std::ios::ate};
  if (!input.is_open()) {
    return {};
  }
  const auto end = input.tellg();
  if (end <= 0) {
    return {};
  }
  const auto size = static_cast<std::size_t>(end);
  std::vector<std::uint8_t> bytes(size);
  input.seekg(0, std::ios::beg);
  input.read(reinterpret_cast<char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  if (!input.good()) {
    return {};
  }
  return bytes;
}

[[nodiscard]] std::string kindLabel(const SandboxAssetKind kind) {
  switch (kind) {
  case SandboxAssetKind::player:
    return "PLAYER";
  case SandboxAssetKind::actor:
    return "ACTOR";
  case SandboxAssetKind::obstacle:
    return "OBSTACLE";
  case SandboxAssetKind::interaction:
    return "INTERACT";
  case SandboxAssetKind::mechanism:
    return "MECHANISM";
  case SandboxAssetKind::safe_point:
    return "SAFE POINT";
  case SandboxAssetKind::effect:
    return "EFFECT";
  }
  return "INVALID";
}

[[nodiscard]] std::string dutyLabel(
    const tgd::contracts::EncounterTacticalDuty duty) {
  using Duty = tgd::contracts::EncounterTacticalDuty;
  switch (duty) {
  case Duty::pressure:
    return "PRESSURE";
  case Duty::flanker:
    return "FLANKER";
  case Duty::harrier:
    return "HARRIER";
  case Duty::controller:
    return "CONTROLLER";
  }
  return "INVALID";
}

[[nodiscard]] std::string_view
craftStageLabel(const tgd::gameplay::CraftSessionStage stage) noexcept {
  using Stage = tgd::gameplay::CraftSessionStage;
  switch (stage) {
  case Stage::awaiting_material:
    return "SELECT MATERIAL";
  case Stage::performing_operations:
    return "CANOPY OPERATIONS";
  case Stage::trial_ready:
    return "RAIN TRIAL READY";
  case Stage::rework_required:
    return "LEAK FOUND / REWORK REQUIRED";
  case Stage::completed:
    return "TRIAL PASSED / CANOPY TUNED";
  case Stage::invalid:
    break;
  }
  return "INVALID";
}

} // namespace

SystemDemoLayer::~SystemDemoLayer() {
  if (active_layer == this) {
    active_layer = nullptr;
  }
}

bool SystemDemoLayer::init() {
  if (!ax::Layer::init()) {
    return false;
  }
  active_layer = this;
  createBackdrop();

  auto *title = makeLabel("TIANGONGDU / SYSTEM DEMO 0.8.4", 24.0F,
                          ax::Color4B(239, 219, 173, 255));
  auto *badge =
      makeLabel("INTERNAL BLOCKOUT", 13.0F, ax::Color4B(114, 224, 200, 255));
  if (title != nullptr) {
    title->setPosition({316.0F, 696.0F});
    addChild(title, 20'000);
  }
  if (badge != nullptr) {
    badge->setPosition({317.0F, 662.0F});
    addChild(badge, 20'000);
  }

  if (!loadRuntimeAndAssets()) {
    auto *failure = makeLabel(message_, 18.0F, ax::Color4B(255, 135, 120, 255),
                              {0.5F, 0.5F});
    if (failure != nullptr) {
      failure->setDimensions(760.0F, 160.0F);
      failure->setAlignment(ax::TextHAlignment::CENTER,
                            ax::TextVAlignment::CENTER);
      failure->setPosition({780.0F, 360.0F});
      addChild(failure, 20'000);
    }
    return true;
  }

  createWorldPresentation();
  createRegistryPanel();
  createHud();
  createCraftPanel();
  createKeyboardInput();
  refreshPresentation();
  ready_ = true;
  message_ = "READY / TUNE THE CANOPY, OPEN THE GATE, CLEAR TWO WAVES";
  scheduleUpdate();
  return true;
}

void SystemDemoLayer::update(const float delta_seconds) {
  if (!ready_) {
    return;
  }
  if (craft_mode_) {
    directions_.fill(false);
    fixed_step_accumulator_ = 0.0F;
    refreshPresentation();
    return;
  }
  fixed_step_accumulator_ += std::clamp(delta_seconds, 0.0F, 0.1F);
  std::uint32_t steps{};
  while (fixed_step_accumulator_ >= fixed_step_seconds && steps < 6U) {
    applyMovementStep();
    advanceEncounterStep();
    fixed_step_accumulator_ -= fixed_step_seconds;
    ++steps;
  }
  if (steps == 6U && fixed_step_accumulator_ >= fixed_step_seconds) {
    fixed_step_accumulator_ = 0.0F;
  }
  refreshPresentation();
}

SystemDemoLayer *SystemDemoLayer::active() noexcept { return active_layer; }

bool SystemDemoLayer::qaReady() const noexcept { return ready_; }

std::int32_t SystemDemoLayer::qaPlayerX() const noexcept {
  return coordinator_.snapshot().session.player_pose.x;
}

std::int32_t SystemDemoLayer::qaPlayerY() const noexcept {
  return coordinator_.snapshot().session.player_pose.y;
}

bool SystemDemoLayer::qaGateOpen() const noexcept { return gate_open_; }

std::uint32_t SystemDemoLayer::qaRetryCount() const noexcept {
  return retry_count_;
}

std::uint32_t SystemDemoLayer::qaBlockedMoveCount() const noexcept {
  return blocked_move_count_;
}

std::uint32_t SystemDemoLayer::qaPackageByteCount() const noexcept {
  return package_byte_count_;
}

std::uint32_t SystemDemoLayer::qaAssetCount() const noexcept {
  return asset_count_;
}

std::int32_t SystemDemoLayer::qaPlayerHealth() const noexcept {
  return encounter_.snapshot().player_health;
}

std::uint32_t SystemDemoLayer::qaActiveHostileCount() const noexcept {
  return encounter_.snapshot().active_hostile_count;
}

std::uint32_t SystemDemoLayer::qaDefeatedHostileCount() const noexcept {
  return encounter_.snapshot().defeated_hostile_count;
}

std::uint32_t SystemDemoLayer::qaCompletedWaveCount() const noexcept {
  return encounter_.snapshot().completed_wave_count;
}

std::uint32_t SystemDemoLayer::qaCompletedObjectiveCount() const noexcept {
  return encounter_.snapshot().completed_objective_count;
}

bool SystemDemoLayer::qaTerminalCompleted() const noexcept {
  return encounter_.snapshot().terminal_completed;
}

std::uint32_t SystemDemoLayer::qaAcceptedAttackCount() const noexcept {
  return encounter_.snapshot().accepted_attack_count;
}

std::uint32_t SystemDemoLayer::qaRepeatedTriggerCount() const noexcept {
  return encounter_.snapshot().repeated_trigger_count;
}

bool SystemDemoLayer::qaCraftMode() const noexcept { return craft_mode_; }

bool SystemDemoLayer::qaCraftInRange() const noexcept {
  return playerInCraftRange();
}

std::uint32_t SystemDemoLayer::qaCraftStage() const noexcept {
  return static_cast<std::uint32_t>(craft_session_.snapshot().stage);
}

std::uint32_t SystemDemoLayer::qaCraftSelectedMaterial() const noexcept {
  const auto selected = craft_session_.snapshot().selected_material;
  if (selected == craft_material_keys_[0]) {
    return 1U;
  }
  if (selected == craft_material_keys_[1]) {
    return 2U;
  }
  return 0U;
}

std::uint32_t
SystemDemoLayer::qaCraftCompletedOperationCount() const noexcept {
  return craft_session_.snapshot().completed_operation_count;
}

std::uint32_t SystemDemoLayer::qaCraftTrialCount() const noexcept {
  return craft_session_.snapshot().trial_count;
}

std::uint32_t SystemDemoLayer::qaCraftMistakeCount() const noexcept {
  return craft_session_.snapshot().mistake_count;
}

std::uint32_t SystemDemoLayer::qaCraftReworkCount() const noexcept {
  return craft_session_.snapshot().rework_count;
}

bool SystemDemoLayer::qaCraftCompleted() const noexcept {
  return craft_session_.snapshot().completed;
}

void SystemDemoLayer::qaOperate() noexcept { submitOperate(); }

void SystemDemoLayer::qaAttackLight() noexcept {
  submitAttack(tgd::gameplay::SandboxEncounterAttack::light);
}

void SystemDemoLayer::qaAttackHeavy() noexcept {
  submitAttack(tgd::gameplay::SandboxEncounterAttack::heavy);
}

void SystemDemoLayer::qaRetry() noexcept { retryLocal(); }

bool SystemDemoLayer::loadRuntimeAndAssets() noexcept {
  auto bytes = readBytes(workbench_preview_package_path);
  if (bytes.empty()) {
    bytes = readBytes(package_path);
  }
  if (bytes.empty() ||
      bytes.size() >
          static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    message_ = "BOOT FAILED / UNIQUE system-demo.tgdsbx WAS NOT READ";
    return false;
  }
  package_byte_count_ = static_cast<std::uint32_t>(bytes.size());

  const auto decoded = tgd::content::decode_sandbox_package(bytes);
  if (!decoded.validation.valid() || decoded.document == nullptr) {
    message_ = "BOOT FAILED / CANONICAL SYSTEM DEMO PACKAGE DID NOT DECODE";
    return false;
  }
  const auto &definition = decoded.document->definition();
  if (definition.assets.size() !=
      tgd::presentation::sandbox_runtime_asset_slot_count) {
    message_ = "BOOT FAILED / SYSTEM DEMO STABLE ASSET SET IS INCOMPLETE";
    return false;
  }

  std::array<tgd::presentation::SandboxPresentationAssetRequirement,
             tgd::presentation::sandbox_runtime_asset_slot_count>
      requirements{};
  for (std::size_t index = 0; index < definition.assets.size(); ++index) {
    const auto &source = definition.assets[index];
    if (source.id.name.size() > requirements[index].id_bytes.size()) {
      message_ = "BOOT FAILED / STABLE ASSET ID EXCEEDED RUNTIME CAPACITY";
      return false;
    }
    requirements[index].key = source.id.key;
    requirements[index].id_byte_count =
        static_cast<std::uint16_t>(source.id.name.size());
    std::copy(source.id.name.begin(), source.id.name.end(),
              requirements[index].id_bytes.begin());
    requirements[index].kind = source.kind;
  }

  const tgd::content::SandboxPackagePublicationIdentity package_identity{
      1U,
      decoded.document->fingerprint(),
  };
  const tgd::presentation::SandboxAssetSourceIdentity asset_source{
      package_identity.generation(),
      package_identity.checksum(),
      1U,
  };
  auto prepared = asset_resolver_.prepare(
      tgd::presentation::system_sandbox_asset_registry(), requirements,
      tgd::presentation::SandboxAssetQuality::standard, asset_source);
  if (prepared.error != tgd::presentation::SandboxAssetResolveError::none) {
    message_ = "BOOT FAILED / STABLE ASSET REGISTRY DID NOT PREPARE";
    return false;
  }

  const tgd::gameplay::SandboxPlayerRuntimeBinding player_binding{
      definition.player.id,
      player_actor,
  };
  constexpr tgd::integration::SandboxThinRuntimePlayerConfig player_config{
      500,
      100,
      1'800,
  };
  const auto published = coordinator_.publish(
      {package_identity, std::move(bytes)}, player_binding, player_config);
  if (published.disposition !=
      tgd::integration::SandboxRuntimePublishDisposition::published) {
    message_ = "BOOT FAILED / SANDBOX RUNTIME AGGREGATE DID NOT PUBLISH";
    return false;
  }
  const auto committed = asset_resolver_.commit(std::move(prepared.prepared));
  if (committed.disposition !=
      tgd::presentation::SandboxAssetCommitDisposition::committed) {
    message_ = "BOOT FAILED / STABLE ASSET SET DID NOT COMMIT";
    return false;
  }

  const auto *live_document = coordinator_.document();
  const auto *live_assets = asset_resolver_.live_set();
  if (live_document == nullptr || live_assets == nullptr ||
      live_assets->size() !=
          tgd::presentation::sandbox_runtime_asset_slot_count ||
      live_document->definition().actors.empty() ||
      live_document->definition().waves.empty() ||
      live_document->definition().wave_spawns.empty() ||
      live_document->definition().objectives.empty() ||
      live_document->definition().interactions.empty() ||
      live_document->definition().ground_blockers.empty() ||
      live_document->gameplay_binding().interaction_bindings.empty() ||
      live_document->gameplay_binding().mechanism_bindings.empty() ||
      live_document->gameplay_binding().actor_bindings.empty() ||
      live_document->craft_definition().materials.size() != 2U ||
      live_document->craft_definition().workstations.empty() ||
      live_document->craft_definition().processes.size() != 1U ||
      live_document->craft_definition().material_choices.size() != 2U ||
      live_document->craft_definition().steps.size() != 4U) {
    message_ = "BOOT FAILED / PUBLISHED SYSTEM DEMO SHAPE DRIFTED";
    return false;
  }

  const auto &live_definition = live_document->definition();
  const auto &gameplay_binding = live_document->gameplay_binding();
  const auto &interaction_binding =
      gameplay_binding.interaction_bindings.front();
  const auto interaction = std::find_if(
      live_definition.interactions.begin(), live_definition.interactions.end(),
      [&interaction_binding](const auto &candidate) {
        return candidate.id.key == interaction_binding.interaction_id.key;
      });
  const auto mechanism_binding =
      std::find_if(gameplay_binding.mechanism_bindings.begin(),
                   gameplay_binding.mechanism_bindings.end(),
                   [&interaction_binding](const auto &candidate) {
                     return candidate.mechanism_id.key ==
                            interaction_binding.target_mechanism_id.key;
                   });
  if (interaction == live_definition.interactions.end() ||
      mechanism_binding == gameplay_binding.mechanism_bindings.end()) {
    message_ = "BOOT FAILED / PRIMARY INTERACTION CHAIN DID NOT RESOLVE";
    return false;
  }
  const auto blocker =
      std::find_if(live_definition.ground_blockers.begin(),
                   live_definition.ground_blockers.end(),
                   [&mechanism_binding](const auto &candidate) {
                     return candidate.id.key ==
                            mechanism_binding->target_ground_blocker_id.key;
                   });
  if (blocker == live_definition.ground_blockers.end()) {
    message_ = "BOOT FAILED / PRIMARY GATE BLOCKER DID NOT RESOLVE";
    return false;
  }

  gate_blocker_index_ = static_cast<std::size_t>(
      blocker - live_definition.ground_blockers.begin());
  interaction_key_ = interaction->id.key;
  target_mechanism_key_ = interaction_binding.target_mechanism_id.key;
  interaction_pose_ = interaction->pose;
  interaction_range_mm_ = interaction_binding.range_mm;
  asset_count_ = static_cast<std::uint32_t>(live_assets->size());
  const auto runtime_snapshot = coordinator_.snapshot();
  if (encounter_.initialize(live_definition, gameplay_binding, player_actor,
                            runtime_snapshot.session.player_pose) !=
      tgd::gameplay::SandboxEncounterBuildError::none) {
    message_ = "BOOT FAILED / AUTHORED ENCOUNTER DID NOT INITIALIZE";
    return false;
  }

  const auto &craft = live_document->craft_definition();
  const auto &craft_process = craft.processes.front();
  const auto workstation = std::find_if(
      craft.workstations.begin(), craft.workstations.end(),
      [&craft_process](const auto &candidate) {
        return candidate.id.key == craft_process.workstation_id.key;
      });
  const auto pass_material = std::find_if(
      craft.material_choices.begin(), craft.material_choices.end(),
      [&craft_process](const auto &candidate) {
        return candidate.process_id.key == craft_process.id.key &&
               candidate.outcome ==
                   tgd::contracts::CraftMaterialOutcome::passes_trial;
      });
  const auto rework_material = std::find_if(
      craft.material_choices.begin(), craft.material_choices.end(),
      [&craft_process](const auto &candidate) {
        return candidate.process_id.key == craft_process.id.key &&
               candidate.outcome ==
                   tgd::contracts::CraftMaterialOutcome::requires_rework;
      });
  if (workstation == craft.workstations.end() ||
      pass_material == craft.material_choices.end() ||
      rework_material == craft.material_choices.end()) {
    message_ = "BOOT FAILED / CRAFT PROCESS REFERENCES DID NOT RESOLVE";
    return false;
  }

  auto predecessor = tgd::contracts::StableContentKey{};
  for (std::size_t index = 0; index < craft_operation_keys_.size(); ++index) {
    const auto operation = std::find_if(
        craft.steps.begin(), craft.steps.end(),
        [&craft_process, predecessor](const auto &candidate) {
          return candidate.process_id.key == craft_process.id.key &&
                 candidate.kind == tgd::contracts::CraftStepKind::operation &&
                 candidate.predecessor_step_id.key == predecessor;
        });
    if (operation == craft.steps.end()) {
      message_ = "BOOT FAILED / CRAFT OPERATION CHAIN DID NOT RESOLVE";
      return false;
    }
    craft_operation_keys_[index] = operation->id.key;
    predecessor = operation->id.key;
  }
  const auto extra_operation = std::find_if(
      craft.steps.begin(), craft.steps.end(),
      [&craft_process, predecessor](const auto &candidate) {
        return candidate.process_id.key == craft_process.id.key &&
               candidate.kind == tgd::contracts::CraftStepKind::operation &&
               candidate.predecessor_step_id.key == predecessor;
      });
  if (extra_operation != craft.steps.end()) {
    message_ = "BOOT FAILED / HOST CRAFT SLICE REQUIRES TWO OPERATIONS";
    return false;
  }

  if (craft_session_.initialize(craft, craft_process.id.key) !=
      tgd::gameplay::CraftSessionBuildError::none) {
    message_ = "BOOT FAILED / AUTHORED CRAFT SESSION DID NOT INITIALIZE";
    return false;
  }
  craft_material_keys_[0] = pass_material->material_id.key;
  craft_material_keys_[1] = rework_material->material_id.key;
  craft_workstation_asset_key_ = workstation->asset_id.key;
  craft_workstation_asset_id_ = workstation->asset_id.name;
  craft_workstation_pose_ = workstation->pose;
  gate_open_ = currentGateOpen();
  return true;
}

void SystemDemoLayer::createBackdrop() {
  addChild(ax::LayerColor::create(ax::Color4B(7, 18, 24, 255), design_width,
                                  design_height),
           -20'000);
  auto *far = ax::DrawNode::create();
  if (far == nullptr) {
    return;
  }
  solidPolygon(
      far,
      {{286.0F, 0.0F}, {1280.0F, 0.0F}, {1280.0F, 720.0F}, {286.0F, 720.0F}},
      color(12, 31, 38));
  solidPolygon(far, {{286.0F, 350.0F}, {470.0F, 520.0F}, {650.0F, 368.0F}},
               color(19, 52, 58));
  solidPolygon(far, {{560.0F, 350.0F}, {820.0F, 566.0F}, {1075.0F, 348.0F}},
               color(15, 45, 53));
  solidPolygon(far, {{920.0F, 350.0F}, {1120.0F, 510.0F}, {1280.0F, 375.0F}},
               color(18, 48, 53));
  for (std::uint32_t index = 0; index < 62U; ++index) {
    const float x = 300.0F + static_cast<float>((index * 149U + 37U) % 1010U);
    const float y = 20.0F + static_cast<float>((index * 83U + 17U) % 690U);
    far->drawLine({x, y}, {x - 8.0F, y - 22.0F}, color(109, 180, 191, 62),
                  1.0F);
  }
  addChild(far, -19'000);
}

void SystemDemoLayer::createWorldPresentation() {
  const auto *document = coordinator_.document();
  if (document == nullptr) {
    return;
  }
  const auto &definition = document->definition();
  world_layer_ = ax::Node::create();
  addChild(world_layer_, 0);

  const auto &bounds = definition.bounds;
  const std::array ground_points{
      project({bounds.min_x, bounds.min_y, 0, bounds.min_floor_layer}),
      project({bounds.max_x, bounds.min_y, 0, bounds.min_floor_layer}),
      project({bounds.max_x, bounds.max_y, 0, bounds.min_floor_layer}),
      project({bounds.min_x, bounds.max_y, 0, bounds.min_floor_layer}),
  };
  auto *ground = ax::DrawNode::create();
  ground->drawSolidPoly(ground_points.data(),
                        static_cast<unsigned int>(ground_points.size()),
                        color(51, 72, 65), 2.0F, color(121, 139, 112));
  for (std::int32_t y = bounds.min_y + 1000; y < bounds.max_y; y += 2000) {
    const auto start = project({bounds.min_x, y, 0, bounds.min_floor_layer});
    const auto end = project({bounds.max_x, y, 0, bounds.min_floor_layer});
    ground->drawLine(start, end, color(136, 145, 111, 52), 1.0F);
  }
  for (std::int32_t x = bounds.min_x + 1000; x < bounds.max_x; x += 1000) {
    const auto start = project({x, bounds.min_y, 0, bounds.min_floor_layer});
    const auto end = project({x, bounds.max_y, 0, bounds.min_floor_layer});
    ground->drawLine(start, end, color(99, 129, 117, 45), 1.0F);
  }
  world_layer_->addChild(ground, -1000);

  auto *water = ax::DrawNode::create();
  const std::array canal{
      project({bounds.min_x, 4'800, 0, 0}),
      project({bounds.max_x, 4'800, 0, 0}),
      project({bounds.max_x, 5'700, 0, 0}),
      project({bounds.min_x, 5'700, 0, 0}),
  };
  water->drawSolidPoly(canal.data(), static_cast<unsigned int>(canal.size()),
                       color(22, 73, 80, 185), 1.0F, color(85, 147, 145, 130));
  world_layer_->addChild(water, -900);

  for (std::size_t index = 0; index < definition.ground_blockers.size();
       ++index) {
    const auto &blocker = definition.ground_blockers[index];
    const std::array threshold{
        project({blocker.min_x, blocker.min_y, 0, blocker.floor_layer}),
        project({blocker.max_x, blocker.min_y, 0, blocker.floor_layer}),
        project({blocker.max_x, blocker.max_y, 0, blocker.floor_layer}),
        project({blocker.min_x, blocker.max_y, 0, blocker.floor_layer}),
    };
    auto *threshold_draw = ax::DrawNode::create();
    threshold_draw->drawSolidPoly(
        threshold.data(), static_cast<unsigned int>(threshold.size()),
        color(138, 93, 58, 125), 2.0F, color(224, 175, 101, 205));
    world_layer_->addChild(threshold_draw, -300);

    auto *blocker_node = createAssetSprite(
        blocker.asset_id.name, blocker.asset_id.key, SandboxAssetKind::obstacle,
        sceneAssetExtent(SandboxAssetKind::obstacle));
    if (blocker_node == nullptr) {
      continue;
    }
    const GroundPoseMm blocker_pose{
        blocker.min_x + ((blocker.max_x - blocker.min_x) / 2),
        blocker.min_y + ((blocker.max_y - blocker.min_y) / 2),
        blocker.min_height,
        blocker.floor_layer,
    };
    const auto position = project(blocker_pose);
    blocker_node->setPosition(position);
    world_layer_->addChild(blocker_node, depthOrder(position.y) + 4);
    if (index == gate_blocker_index_) {
      gate_node_ = blocker_node;
    } else {
      blocker_node->setOpacity(190);
    }
  }

  for (const auto &safe_point : definition.safe_points) {
    auto *safe_node =
        createAssetSprite(safe_point.asset_id.name, safe_point.asset_id.key,
                          SandboxAssetKind::safe_point,
                          sceneAssetExtent(SandboxAssetKind::safe_point));
    if (safe_node != nullptr) {
      const auto position = project(safe_point.pose);
      safe_node->setPosition(position);
      world_layer_->addChild(safe_node, depthOrder(position.y));
    }
  }

  for (const auto &interaction : definition.interactions) {
    auto *interaction_node =
        createAssetSprite(interaction.asset_id.name, interaction.asset_id.key,
                          SandboxAssetKind::interaction,
                          sceneAssetExtent(SandboxAssetKind::interaction));
    if (interaction_node != nullptr) {
      const auto position = project(interaction.pose);
      interaction_node->setPosition(position);
      interaction_node->setOpacity(
          interaction.id.key == interaction_key_ ? 255 : 170);
      world_layer_->addChild(interaction_node, depthOrder(position.y) + 2);
    }
  }

  craft_workstation_node_ = createAssetSprite(
      craft_workstation_asset_id_, craft_workstation_asset_key_,
      SandboxAssetKind::interaction,
      sceneAssetExtent(SandboxAssetKind::interaction) * 1.18F);
  if (craft_workstation_node_ != nullptr) {
    const auto position = project(craft_workstation_pose_);
    craft_workstation_node_->setPosition(position);
    craft_workstation_node_->setColor(ax::Color3B(102, 226, 194));
    world_layer_->addChild(craft_workstation_node_, depthOrder(position.y) + 3);
    craft_workstation_status_label_ =
        makeLabel("UMBRELLA WORKSTATION / C", 10.0F,
                  ax::Color4B(123, 231, 204, 255), {0.5F, 0.0F});
    if (craft_workstation_status_label_ != nullptr) {
      craft_workstation_status_label_->setPosition(
          position + ax::Vec2(0.0F, -22.0F));
      world_layer_->addChild(craft_workstation_status_label_,
                             depthOrder(position.y) + 4);
    }
  }

  for (const auto &mechanism : definition.mechanisms) {
    auto *mechanism_node =
        createAssetSprite(mechanism.asset_id.name, mechanism.asset_id.key,
                          SandboxAssetKind::mechanism,
                          sceneAssetExtent(SandboxAssetKind::mechanism));
    if (mechanism_node != nullptr) {
      const auto position = project(mechanism.pose);
      mechanism_node->setPosition(position);
      world_layer_->addChild(mechanism_node, depthOrder(position.y) + 3);
    }
  }

  actor_presentation_count_ = 0;
  for (const auto &actor : definition.actors) {
    auto *actor_node = createAssetSprite(
        actor.asset_id.name, actor.asset_id.key, SandboxAssetKind::actor,
        sceneAssetExtent(SandboxAssetKind::actor));
    if (actor_node == nullptr) {
      continue;
    }
    const auto position = project(actor.pose);
    actor_node->setPosition(position);
    actor_node->setOpacity(46);
    world_layer_->addChild(actor_node, depthOrder(position.y));
    const auto *binding = encounter_.actor_binding(actor.id.key);
    const auto slot_text =
        binding == nullptr
            ? std::string{"PLACEMENT ONLY"}
            : dutyLabel(binding->duty) + " / WAITING";
    auto *slot = makeLabel(slot_text, 9.0F,
                           ax::Color4B(142, 173, 168, 180), {0.5F, 0.0F});
    if (slot != nullptr) {
      slot->setPosition(position + ax::Vec2(0.0F, -16.0F));
      world_layer_->addChild(slot, depthOrder(position.y) + 1);
    }
    if (actor_presentation_count_ < actor_presentations_.size()) {
      actor_presentations_[actor_presentation_count_++] = {
          actor.id.key,
          actor_node,
          slot,
      };
    }
  }

  player_node_ = createAssetSprite(
      definition.player.asset_id.name, definition.player.asset_id.key,
      SandboxAssetKind::player, sceneAssetExtent(SandboxAssetKind::player));
  if (player_node_ != nullptr) {
    world_layer_->addChild(player_node_, 1000);
  }
}

void SystemDemoLayer::createRegistryPanel() {
  auto *panel = ax::DrawNode::create();
  solidPolygon(panel,
               {{0.0F, 0.0F}, {286.0F, 0.0F}, {286.0F, 720.0F}, {0.0F, 720.0F}},
               color(4, 14, 20, 245), color(65, 112, 116, 210), 1.0F);
  addChild(panel, 15'000);

  auto *heading = makeLabel("PACKAGE-BOUND ASSET SLOTS", 14.0F,
                            ax::Color4B(231, 205, 158, 255));
  auto *source = makeLabel(
      "system-demo.tgdsbx\n12 Stable IDs / STANDARD\nresolver bytes -> GPU",
      11.0F, ax::Color4B(141, 188, 180, 255));
  if (heading != nullptr) {
    heading->setPosition({20.0F, 690.0F});
    addChild(heading, 15'010);
  }
  if (source != nullptr) {
    source->setPosition({20.0F, 662.0F});
    addChild(source, 15'010);
  }

  const auto *live_assets = asset_resolver_.live_set();
  if (live_assets == nullptr) {
    return;
  }
  for (std::size_t index = 0; index < live_assets->size(); ++index) {
    const auto *asset = live_assets->asset_at(index);
    if (asset == nullptr) {
      continue;
    }
    const auto column = static_cast<float>(index % 3U);
    const auto row = static_cast<float>(index / 3U);
    const ax::Vec2 position{
        52.0F + (column * 88.0F),
        530.0F - (row * 118.0F),
    };
    auto *sprite = createAssetSprite(asset->stable_id(), asset->stable_key(),
                                     asset->kind(), 58.0F);
    if (sprite != nullptr) {
      sprite->setPosition(position);
      sprite->setOpacity(225);
      addChild(sprite, 15'020);
    }
    const auto caption =
        std::to_string(index + 1U) + " / " + kindLabel(asset->kind());
    auto *label =
        makeLabel(caption, 9.0F, ax::Color4B(182, 198, 179, 230), {0.5F, 1.0F});
    if (label != nullptr) {
      label->setPosition(position + ax::Vec2(0.0F, -40.0F));
      addChild(label, 15'020);
    }
  }

  auto *note = makeLabel("BLOCKOUT ONLY\nNo asset name owns gameplay.", 10.0F,
                         ax::Color4B(219, 158, 112, 255));
  if (note != nullptr) {
    note->setPosition({20.0F, 72.0F});
    addChild(note, 15'020);
  }
}

void SystemDemoLayer::createHud() {
  auto *controls = makeLabel(
      "MOVE  WASD/ARROWS   GATE  F   ATTACK  J/K   CRAFT  C · 1/2 · 3/4 · T · G   RETRY  R",
      11.0F, ax::Color4B(224, 228, 199, 255));
  if (controls != nullptr) {
    controls->setPosition({316.0F, 634.0F});
    addChild(controls, 20'000);
  }

  gate_status_label_ = makeLabel("GATE / CLOSED / BLOCKER ENABLED", 15.0F,
                                 ax::Color4B(239, 169, 100, 255));
  player_status_label_ =
      makeLabel("PLAYER", 12.0F, ax::Color4B(170, 209, 200, 255));
  wave_status_label_ =
      makeLabel("WAVE / WAITING", 12.0F, ax::Color4B(170, 209, 200, 255));
  objective_status_label_ =
      makeLabel("OBJECTIVES / 0", 12.0F, ax::Color4B(170, 209, 200, 255));
  prompt_label_ = makeLabel("MOVE TO THE GATE CONSOLE", 18.0F,
                            ax::Color4B(239, 219, 173, 255), {0.5F, 0.5F});
  message_label_ = makeLabel(message_, 11.0F, ax::Color4B(144, 186, 178, 255));
  if (gate_status_label_ != nullptr) {
    gate_status_label_->setPosition({1010.0F, 690.0F});
    addChild(gate_status_label_, 20'000);
  }
  if (player_status_label_ != nullptr) {
    player_status_label_->setPosition({1010.0F, 664.0F});
    addChild(player_status_label_, 20'000);
  }
  if (wave_status_label_ != nullptr) {
    wave_status_label_->setPosition({1010.0F, 638.0F});
    addChild(wave_status_label_, 20'000);
  }
  if (objective_status_label_ != nullptr) {
    objective_status_label_->setPosition({1010.0F, 612.0F});
    addChild(objective_status_label_, 20'000);
  }
  if (prompt_label_ != nullptr) {
    prompt_label_->setPosition({780.0F, 42.0F});
    addChild(prompt_label_, 20'000);
  }
  if (message_label_ != nullptr) {
    message_label_->setPosition({316.0F, 612.0F});
    addChild(message_label_, 20'000);
  }
}

void SystemDemoLayer::createCraftPanel() {
  craft_panel_ = ax::Node::create();
  if (craft_panel_ == nullptr) {
    return;
  }
  addChild(craft_panel_, 30'000);

  auto *panel = ax::DrawNode::create();
  if (panel != nullptr) {
    solidPolygon(panel,
                 {{350.0F, 110.0F},
                  {1210.0F, 110.0F},
                  {1210.0F, 565.0F},
                  {350.0F, 565.0F}},
                 color(5, 19, 25, 247), color(106, 209, 188, 235), 2.0F);
    solidPolygon(panel,
                 {{350.0F, 492.0F},
                  {1210.0F, 492.0F},
                  {1210.0F, 565.0F},
                  {350.0F, 565.0F}},
                 color(15, 49, 54, 248));
    panel->drawLine({380.0F, 395.0F}, {1180.0F, 395.0F},
                    color(108, 173, 157, 150), 1.0F);
    craft_panel_->addChild(panel);
  }

  auto *title =
      makeLabel("JIANGNAN UMBRELLA / CANOPY TUNING", 25.0F,
                ax::Color4B(241, 218, 167, 255));
  auto *subtitle = makeLabel(
      "NEED: EVEN CANOPY TENSION  ·  MAKE A CHOICE  ·  OPERATE IN ORDER  ·  TEST IN RAIN",
      11.0F, ax::Color4B(142, 207, 192, 255));
  auto *close =
      makeLabel("C / LEAVE WORKSTATION", 11.0F,
                ax::Color4B(149, 188, 181, 255), {1.0F, 1.0F});
  craft_stage_label_ =
      makeLabel("STAGE / SELECT MATERIAL", 18.0F,
                ax::Color4B(122, 226, 198, 255));
  craft_material_label_ =
      makeLabel("MATERIAL / NONE", 13.0F,
                ax::Color4B(210, 199, 165, 255));
  craft_steps_label_ =
      makeLabel("", 15.0F, ax::Color4B(222, 226, 205, 255));
  craft_hint_label_ =
      makeLabel("", 13.0F, ax::Color4B(239, 174, 112, 255));

  if (title != nullptr) {
    title->setPosition({385.0F, 550.0F});
    craft_panel_->addChild(title, 2);
  }
  if (subtitle != nullptr) {
    subtitle->setPosition({385.0F, 511.0F});
    craft_panel_->addChild(subtitle, 2);
  }
  if (close != nullptr) {
    close->setPosition({1175.0F, 548.0F});
    craft_panel_->addChild(close, 2);
  }
  if (craft_stage_label_ != nullptr) {
    craft_stage_label_->setPosition({385.0F, 470.0F});
    craft_panel_->addChild(craft_stage_label_, 2);
  }
  if (craft_material_label_ != nullptr) {
    craft_material_label_->setPosition({385.0F, 432.0F});
    craft_panel_->addChild(craft_material_label_, 2);
  }
  if (craft_steps_label_ != nullptr) {
    craft_steps_label_->setDimensions(760.0F, 210.0F);
    craft_steps_label_->setAlignment(ax::TextHAlignment::LEFT,
                                     ax::TextVAlignment::TOP);
    craft_steps_label_->setPosition({385.0F, 377.0F});
    craft_panel_->addChild(craft_steps_label_, 2);
  }
  if (craft_hint_label_ != nullptr) {
    craft_hint_label_->setDimensions(760.0F, 72.0F);
    craft_hint_label_->setAlignment(ax::TextHAlignment::LEFT,
                                    ax::TextVAlignment::CENTER);
    craft_hint_label_->setPosition({385.0F, 173.0F});
    craft_panel_->addChild(craft_hint_label_, 2);
  }
  craft_panel_->setVisible(false);
}

void SystemDemoLayer::createKeyboardInput() {
  auto *listener = ax::EventListenerKeyboard::create();
  listener->onKeyPressed = [this](const ax::EventKeyboard::KeyCode key,
                                  ax::Event *) {
    if (key == ax::EventKeyboard::KeyCode::KEY_C) {
      toggleCraftMode();
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_1) {
      selectCraftMaterial(0U);
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_2) {
      selectCraftMaterial(1U);
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_3) {
      performCraftOperation(0U);
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_4) {
      performCraftOperation(1U);
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_T) {
      runCraftTrial();
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_G) {
      performCraftRework();
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_F) {
      submitOperate();
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_R) {
      retryLocal();
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_J ||
        key == ax::EventKeyboard::KeyCode::KEY_SPACE) {
      submitAttack(tgd::gameplay::SandboxEncounterAttack::light);
      return;
    }
    if (key == ax::EventKeyboard::KeyCode::KEY_K) {
      submitAttack(tgd::gameplay::SandboxEncounterAttack::heavy);
      return;
    }
    setDirection(key, true);
  };
  listener->onKeyReleased = [this](const ax::EventKeyboard::KeyCode key,
                                   ax::Event *) { setDirection(key, false); };
  _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
}

void SystemDemoLayer::applyMovementStep() noexcept {
  if (encounter_.snapshot().player_defeated) {
    directions_.fill(false);
    return;
  }
  const auto horizontal =
      static_cast<std::int32_t>(
          directions_[static_cast<std::size_t>(Direction::right)]) -
      static_cast<std::int32_t>(
          directions_[static_cast<std::size_t>(Direction::left)]);
  const auto vertical =
      static_cast<std::int32_t>(
          directions_[static_cast<std::size_t>(Direction::forward)]) -
      static_cast<std::int32_t>(
          directions_[static_cast<std::size_t>(Direction::back)]);
  if (horizontal == 0 && vertical == 0) {
    return;
  }
  const bool diagonal = horizontal != 0 && vertical != 0;
  const auto distance = diagonal ? diagonal_move_mm : cardinal_move_mm;
  const auto before = coordinator_.snapshot();
  const auto moved = coordinator_.advance_player({
      before.runtime_generation,
      before.movement_sequence + 1U,
      before.authoritative_tick + 1U,
      before.session.player_actor,
      before.session.player_pose.floor_layer,
      horizontal * distance,
      vertical * distance,
  });
  if (moved.disposition ==
      SandboxRuntimeCommandDisposition::collision_blocked) {
    ++blocked_move_count_;
    message_ = "BLOCKED / CLOSED GATE COLLISION IS AUTHORITATIVE";
  } else if (moved.disposition != SandboxRuntimeCommandDisposition::applied) {
    message_ = "MOVE REJECTED / RUNTIME STATE PRESERVED";
  }
}

void SystemDemoLayer::advanceEncounterStep() noexcept {
  const auto before = encounter_.snapshot();
  if (!before.initialized || before.player_defeated ||
      before.terminal_completed) {
    return;
  }
  const auto pose = coordinator_.snapshot().session.player_pose;
  const auto advanced = encounter_.advance_one_tick(pose);
  using Disposition = tgd::gameplay::SandboxEncounterStepDisposition;
  if (advanced.disposition == Disposition::player_defeated) {
    directions_.fill(false);
    message_ = "PLAYER DOWN / R RESTARTS THE LOCAL GATE AND TWO-WAVE ROUTE";
  } else if (advanced.disposition == Disposition::terminal_completed) {
    directions_.fill(false);
    message_ = "TERMINAL OBJECTIVE COMPLETE / TWO AUTHORED WAVES CLEARED";
  } else if (advanced.disposition != Disposition::advanced) {
    directions_.fill(false);
    message_ = "ENCOUNTER STEP REJECTED / DETERMINISTIC STATE PRESERVED";
  }
}

void SystemDemoLayer::submitOperate() noexcept {
  if (craft_mode_) {
    message_ = "GATE INPUT PAUSED / C TO LEAVE THE WORKSTATION";
    refreshPresentation();
    return;
  }
  const auto before = coordinator_.snapshot();
  auto encounter_candidate = encounter_;
  const auto triggered =
      encounter_candidate.notify_mechanism_activated(target_mechanism_key_);
  const bool trigger_ready =
      triggered == tgd::gameplay::SandboxEncounterEventDisposition::applied ||
      triggered == tgd::gameplay::SandboxEncounterEventDisposition::repeated;
  if (!trigger_ready) {
    message_ = "OPERATE REJECTED / ENCOUNTER PREFLIGHT PRESERVED BOTH STATES";
    refreshPresentation();
    return;
  }
  const auto operated = coordinator_.submit_operate({
      before.runtime_generation,
      before.session.last_command_sequence + 1U,
      before.session.player_actor,
      interaction_key_,
  });
  if (operated.disposition == SandboxRuntimeCommandDisposition::applied) {
    if (triggered ==
        tgd::gameplay::SandboxEncounterEventDisposition::applied) {
      encounter_ = std::move(encounter_candidate);
      message_ = "GATE OPEN / OBJECTIVE COMPLETE / WAVE 1 ACTIVE";
    } else {
      message_ = "OPERATE REJECTED / RUNTIME AND ENCOUNTER STATE DRIFT";
    }
  } else if (operated.disposition ==
             SandboxRuntimeCommandDisposition::session_rejected) {
    message_ = "OPERATE REJECTED / MOVE WITHIN 1200 MM OF THE CONSOLE";
  } else if (operated.disposition ==
             SandboxRuntimeCommandDisposition::repeated) {
    if (triggered ==
        tgd::gameplay::SandboxEncounterEventDisposition::repeated) {
      encounter_ = std::move(encounter_candidate);
      message_ = "OPERATE REPEATED / NO DUPLICATE WAVE SPAWN";
    } else {
      message_ = "OPERATE REPEATED / ENCOUNTER STATE DRIFT PRESERVED";
    }
  } else {
    message_ = "OPERATE REJECTED / RUNTIME STATE PRESERVED";
  }
  refreshPresentation();
}

void SystemDemoLayer::submitAttack(
    const tgd::gameplay::SandboxEncounterAttack attack) noexcept {
  if (craft_mode_) {
    message_ = "COMBAT INPUT PAUSED / C TO LEAVE THE WORKSTATION";
    refreshPresentation();
    return;
  }
  using Disposition = tgd::gameplay::SandboxEncounterAttackDisposition;
  const auto queued = encounter_.queue_player_attack(attack);
  switch (queued) {
  case Disposition::queued:
    message_ = attack == tgd::gameplay::SandboxEncounterAttack::heavy
                   ? "HEAVY ATTACK QUEUED / NEAREST ACTIVE HOSTILE"
                   : "LIGHT ATTACK QUEUED / NEAREST ACTIVE HOSTILE";
    break;
  case Disposition::already_queued:
    message_ = "ATTACK ALREADY QUEUED / WAIT FOR THE NEXT FIXED TICK";
    break;
  case Disposition::player_defeated:
    message_ = "PLAYER DOWN / R TO RESTART THE LOCAL ROUTE";
    break;
  case Disposition::no_active_target:
    message_ = gate_open_ ? "NO ACTIVE TARGET / ADVANCE THE AUTHORED ROUTE"
                          : "NO ACTIVE TARGET / OPEN THE GATE FIRST";
    break;
  case Disposition::invalid_state:
  case Disposition::invalid:
    message_ = "ATTACK REJECTED / ENCOUNTER STATE PRESERVED";
    break;
  }
  refreshPresentation();
}

void SystemDemoLayer::toggleCraftMode() noexcept {
  if (craft_mode_) {
    craft_mode_ = false;
    message_ = craft_session_.snapshot().completed
                   ? "WORKSTATION CLOSED / TUNED CANOPY STATE PRESERVED"
                   : "WORKSTATION CLOSED / CRAFT STATE PRESERVED";
    refreshPresentation();
    return;
  }
  if (!playerInCraftRange()) {
    message_ =
        "WORKSTATION OUT OF RANGE / MOVE WITHIN 1200 MM OF THE CRAFT BENCH";
    refreshPresentation();
    return;
  }
  if (encounter_.snapshot().active_hostile_count > 0U) {
    message_ = "WORKSTATION LOCKED / CLEAR THE ACTIVE WAVE FIRST";
    refreshPresentation();
    return;
  }
  directions_.fill(false);
  craft_mode_ = true;
  message_ = craft_session_.snapshot().completed
                 ? "WORKSTATION OPEN / COMPLETED CANOPY REVIEW"
                 : "WORKSTATION OPEN / READ THE NEED, THEN CHOOSE MATERIAL";
  refreshPresentation();
}

void SystemDemoLayer::selectCraftMaterial(
    const std::size_t choice_index) noexcept {
  if (!craft_mode_ || choice_index >= craft_material_keys_.size()) {
    message_ = "CRAFT INPUT REJECTED / C TO ENTER THE WORKSTATION";
    refreshPresentation();
    return;
  }
  const auto selected =
      craft_session_.select_material(craft_material_keys_[choice_index]);
  using Disposition = tgd::gameplay::CraftActionDisposition;
  switch (selected.disposition) {
  case Disposition::applied:
    message_ =
        choice_index == 0U
            ? "MATERIAL SET / FLEXIBLE BAST-PAPER PATCH / BEGIN ALIGNMENT"
            : "MATERIAL SET / STIFF SALVAGE PAPER / TEST WILL REVEAL FITNESS";
    break;
  case Disposition::wrong_stage:
    message_ = "MATERIAL LOCKED / R RESTARTS THE LOCAL CRAFT ROUTE";
    break;
  case Disposition::unknown_target:
  case Disposition::wrong_order:
  case Disposition::invalid_state:
  case Disposition::invalid:
    message_ = "MATERIAL REJECTED / CRAFT STATE PRESERVED";
    break;
  }
  refreshPresentation();
}

void SystemDemoLayer::performCraftOperation(
    const std::size_t operation_index) noexcept {
  if (!craft_mode_ || operation_index >= craft_operation_keys_.size()) {
    message_ = "CRAFT INPUT REJECTED / C TO ENTER THE WORKSTATION";
    refreshPresentation();
    return;
  }
  const auto performed =
      craft_session_.perform_operation(craft_operation_keys_[operation_index]);
  using Disposition = tgd::gameplay::CraftActionDisposition;
  switch (performed.disposition) {
  case Disposition::applied:
    message_ = operation_index == 0U
                   ? "OPERATION APPLIED / RIBS ALIGNED / NEXT: PASTE PATCH"
                   : "OPERATION APPLIED / PATCH PASTED / RAIN TRIAL READY";
    break;
  case Disposition::wrong_order:
    message_ = operation_index == 0U
                   ? "ORDER REJECTED / ALIGNMENT IS ALREADY COMPLETE"
                   : "ORDER REJECTED / ALIGN THE RIBS BEFORE PASTING";
    break;
  case Disposition::wrong_stage:
    message_ = "OPERATION REJECTED / FOLLOW THE CURRENT CRAFT STAGE";
    break;
  case Disposition::unknown_target:
  case Disposition::invalid_state:
  case Disposition::invalid:
    message_ = "OPERATION REJECTED / CRAFT STATE PRESERVED";
    break;
  }
  refreshPresentation();
}

void SystemDemoLayer::runCraftTrial() noexcept {
  if (!craft_mode_) {
    message_ = "CRAFT INPUT REJECTED / C TO ENTER THE WORKSTATION";
    refreshPresentation();
    return;
  }
  const auto trial = craft_session_.run_trial();
  using Disposition = tgd::gameplay::CraftActionDisposition;
  if (trial.disposition == Disposition::applied) {
    message_ =
        trial.snapshot.stage == tgd::gameplay::CraftSessionStage::rework_required
            ? "RAIN TRIAL FAILED / CANOPY LEAKS / G TO RETENSION"
            : "RAIN TRIAL PASSED / TUNED CANOPY OUTPUT READY";
  } else if (trial.disposition == Disposition::wrong_stage) {
    message_ = "TRIAL REJECTED / COMPLETE THE CURRENT CRAFT STEP FIRST";
  } else {
    message_ = "TRIAL REJECTED / CRAFT STATE PRESERVED";
  }
  refreshPresentation();
}

void SystemDemoLayer::performCraftRework() noexcept {
  if (!craft_mode_) {
    message_ = "CRAFT INPUT REJECTED / C TO ENTER THE WORKSTATION";
    refreshPresentation();
    return;
  }
  const auto reworked = craft_session_.perform_rework();
  using Disposition = tgd::gameplay::CraftActionDisposition;
  if (reworked.disposition == Disposition::applied) {
    message_ = "REWORK APPLIED / CANOPY RETENSIONED / T TO TEST AGAIN";
  } else if (reworked.disposition == Disposition::wrong_stage) {
    message_ = "REWORK REJECTED / RAIN TRIAL HAS NOT REQUESTED IT";
  } else {
    message_ = "REWORK REJECTED / CRAFT STATE PRESERVED";
  }
  refreshPresentation();
}

void SystemDemoLayer::retryLocal() noexcept {
  const auto before = coordinator_.snapshot();
  const auto retried = coordinator_.retry_standalone({
      before.runtime_generation,
      before.session.last_command_sequence + 1U,
  });
  if (retried.disposition == SandboxRuntimeCommandDisposition::applied) {
    const auto restarted =
        encounter_.restart(retried.snapshot.session.player_pose);
    const auto craft_restarted = craft_session_.restart();
    if (restarted == tgd::gameplay::SandboxEncounterBuildError::none &&
        craft_restarted == tgd::gameplay::CraftSessionBuildError::none) {
      ++retry_count_;
      directions_.fill(false);
      fixed_step_accumulator_ = 0.0F;
      craft_mode_ = false;
      message_ = "LOCAL RETRY / CRAFT, GATE, WAVES, OBJECTIVES RESTORED";
    } else {
      message_ = "LOCAL RETRY / GAMEPLAY RESTART REJECTED";
    }
  } else {
    message_ = "LOCAL RETRY REJECTED / RUNTIME STATE PRESERVED";
  }
  refreshPresentation();
}

void SystemDemoLayer::refreshPresentation() noexcept {
  const auto snapshot = coordinator_.snapshot();
  if (!snapshot.initialized) {
    return;
  }
  const auto encounter_snapshot = encounter_.snapshot();
  const auto craft_snapshot = craft_session_.snapshot();
  if (player_node_ != nullptr && world_layer_ != nullptr) {
    const auto position = project(snapshot.session.player_pose);
    player_node_->setPosition(position);
    player_node_->setOpacity(encounter_snapshot.player_defeated ? 105 : 255);
    player_node_->setColor(encounter_snapshot.player_defeated
                               ? ax::Color3B(139, 139, 139)
                               : ax::Color3B::WHITE);
    world_layer_->reorderChild(player_node_, depthOrder(position.y) + 10);
  }

  const auto combat_actors = encounter_.combat_actors();
  for (std::size_t index = 0; index < actor_presentation_count_; ++index) {
    auto &presentation = actor_presentations_[index];
    const auto combat_actor =
        std::find_if(combat_actors.begin(), combat_actors.end(),
                     [&presentation](const auto &candidate) {
                       return candidate.actor == presentation.actor;
                     });
    if (combat_actor == combat_actors.end() || presentation.sprite == nullptr) {
      continue;
    }
    const auto position = project(combat_actor->pose);
    presentation.sprite->setPosition(position);
    presentation.sprite->setOpacity(combat_actor->defeated
                                        ? 72
                                        : (combat_actor->active ? 255 : 46));
    presentation.sprite->setColor(combat_actor->defeated
                                      ? ax::Color3B(116, 116, 116)
                                      : ax::Color3B::WHITE);
    if (world_layer_ != nullptr) {
      world_layer_->reorderChild(presentation.sprite, depthOrder(position.y));
    }
    if (presentation.status != nullptr) {
      const auto *binding = encounter_.actor_binding(presentation.actor);
      const auto duty =
          binding == nullptr ? std::string{"ACTOR"} : dutyLabel(binding->duty);
      if (combat_actor->defeated) {
        presentation.status->setString(duty + " / DEFEATED");
        presentation.status->setTextColor(
            ax::Color4B(153, 153, 153, 205));
      } else if (combat_actor->active) {
        presentation.status->setString(
            duty + " / HP " +
            std::to_string(combat_actor->resources.health) + "/" +
            std::to_string(combat_actor->resources.health_max));
        presentation.status->setTextColor(
            ax::Color4B(245, 189, 117, 255));
      } else {
        presentation.status->setString(duty + " / WAITING");
        presentation.status->setTextColor(
            ax::Color4B(142, 173, 168, 170));
      }
      presentation.status->setPosition(position + ax::Vec2(0.0F, -16.0F));
      if (world_layer_ != nullptr) {
        world_layer_->reorderChild(presentation.status,
                                   depthOrder(position.y) + 1);
      }
    }
  }

  gate_open_ = currentGateOpen();
  if (gate_node_ != nullptr) {
    gate_node_->setOpacity(gate_open_ ? 42 : 255);
  }
  if (gate_status_label_ != nullptr) {
    gate_status_label_->setString(gate_open_
                                      ? "GATE / OPEN / BLOCKER DISABLED"
                                      : "GATE / CLOSED / BLOCKER ENABLED");
    gate_status_label_->setTextColor(gate_open_
                                         ? ax::Color4B(112, 224, 190, 255)
                                         : ax::Color4B(239, 169, 100, 255));
  }
  if (player_status_label_ != nullptr) {
    player_status_label_->setString(
        "PLAYER / HP " + std::to_string(encounter_snapshot.player_health) +
        "/" + std::to_string(encounter_snapshot.player_health_max) + " / X " +
        std::to_string(snapshot.session.player_pose.x) + " / Y " +
        std::to_string(snapshot.session.player_pose.y));
    player_status_label_->setTextColor(
        encounter_snapshot.player_defeated
            ? ax::Color4B(255, 128, 118, 255)
            : ax::Color4B(170, 209, 200, 255));
  }
  if (wave_status_label_ != nullptr) {
    wave_status_label_->setString(
        "WAVES / " +
        std::to_string(encounter_snapshot.completed_wave_count) + "/2 / " +
        activeWaveName() + " / HOSTILES " +
        std::to_string(encounter_snapshot.active_hostile_count));
  }
  if (objective_status_label_ != nullptr) {
    objective_status_label_->setString(
        encounter_snapshot.terminal_completed
            ? "OBJECTIVES / 2/2 / TERMINAL COMPLETE"
            : "OBJECTIVES / " +
                  std::to_string(
                      encounter_snapshot.completed_objective_count) +
                  "/2 / ROUTE ACTIVE");
    objective_status_label_->setTextColor(
        encounter_snapshot.terminal_completed
            ? ax::Color4B(112, 224, 190, 255)
            : ax::Color4B(170, 209, 200, 255));
  }
  if (craft_workstation_node_ != nullptr) {
    craft_workstation_node_->setOpacity(
        craft_snapshot.completed ? 255 : (playerInCraftRange() ? 255 : 190));
    craft_workstation_node_->setColor(
        craft_snapshot.completed ? ax::Color3B(239, 201, 115)
                                 : ax::Color3B(102, 226, 194));
  }
  if (craft_workstation_status_label_ != nullptr) {
    craft_workstation_status_label_->setString(
        craft_snapshot.completed
            ? "UMBRELLA WORKSTATION / TRIAL PASSED"
            : (playerInCraftRange() ? "UMBRELLA WORKSTATION / C TO USE"
                                    : "UMBRELLA WORKSTATION"));
    craft_workstation_status_label_->setTextColor(
        craft_snapshot.completed ? ax::Color4B(240, 205, 126, 255)
                                 : ax::Color4B(123, 231, 204, 255));
  }
  if (craft_panel_ != nullptr) {
    craft_panel_->setVisible(craft_mode_);
  }
  if (craft_stage_label_ != nullptr) {
    craft_stage_label_->setString(
        "STAGE / " + std::string{craftStageLabel(craft_snapshot.stage)});
    craft_stage_label_->setTextColor(
        craft_snapshot.stage ==
                tgd::gameplay::CraftSessionStage::rework_required
            ? ax::Color4B(255, 146, 111, 255)
            : (craft_snapshot.completed
                   ? ax::Color4B(240, 205, 126, 255)
                   : ax::Color4B(122, 226, 198, 255)));
  }
  if (craft_material_label_ != nullptr) {
    if (craft_snapshot.selected_material == craft_material_keys_[0]) {
      craft_material_label_->setString(
          craft_snapshot.completed
              ? "MATERIAL / FLEXIBLE BAST-PAPER PATCH / PASSED WITHOUT REWORK"
              : "MATERIAL / FLEXIBLE BAST-PAPER PATCH / TRIAL RESPONSE UNKNOWN");
    } else if (craft_snapshot.selected_material == craft_material_keys_[1]) {
      craft_material_label_->setString(
          craft_snapshot.completed
              ? "MATERIAL / STIFF SALVAGE PAPER / REWORKED / RETRIAL PASSED"
              : (craft_snapshot.stage ==
                         tgd::gameplay::CraftSessionStage::rework_required
                     ? "MATERIAL / STIFF SALVAGE PAPER / FIRST TRIAL LEAKED"
                     : "MATERIAL / STIFF SALVAGE PAPER / TRIAL RESPONSE UNKNOWN"));
    } else {
      craft_material_label_->setString("MATERIAL / NONE SELECTED");
    }
  }
  if (craft_steps_label_ != nullptr) {
    const auto material_done = craft_snapshot.selected_material != 0;
    const auto first_done = craft_snapshot.completed_operation_count >= 1U;
    const auto second_done = craft_snapshot.completed_operation_count >= 2U;
    const auto trial_done = craft_snapshot.trial_count >= 1U;
    const auto rework_done = craft_snapshot.rework_count >= 1U;
    const auto second_trial_done = craft_snapshot.trial_count >= 2U;
    craft_steps_label_->setString(
        std::string{material_done ? "[DONE] " : "[NEXT] "} +
        "1 / 2  CHOOSE A PAPER PATCH\n" +
        (first_done ? "[DONE] " : "[    ] ") +
        "3      ALIGN THE UMBRELLA RIBS\n" +
        (second_done ? "[DONE] " : "[    ] ") +
        "4      PASTE AND SMOOTH THE PATCH\n" +
        (trial_done ? "[DONE] " : "[    ] ") +
        "T      RUN THE RAIN TRIAL\n" +
        (rework_done ? "[DONE] " : "[    ] ") +
        "G      RETENSION IF THE TRIAL FINDS A LEAK\n" +
        (second_trial_done
             ? "[DONE] "
             : (craft_snapshot.completed && craft_snapshot.rework_count == 0U
                    ? "[ N/A] "
                    : "[    ] ")) +
        "T      REPEAT THE RAIN TRIAL AFTER REWORK");
  }
  if (craft_hint_label_ != nullptr) {
    using Stage = tgd::gameplay::CraftSessionStage;
    switch (craft_snapshot.stage) {
    case Stage::awaiting_material:
      craft_hint_label_->setString(
          "CHOOSE:  1 FLEXIBLE BAST-PAPER PATCH    2 STIFF SALVAGE PAPER\n"
          "The need is fixed; the rain trial reveals whether the choice needs rework.");
      break;
    case Stage::performing_operations:
      craft_hint_label_->setString(
          craft_snapshot.completed_operation_count == 0U
              ? "NEXT INPUT: 3 / ALIGN RIBS.  Pressing 4 first is rejected without state drift."
              : "NEXT INPUT: 4 / PASTE PATCH.  The rain trial unlocks only after both operations.");
      break;
    case Stage::trial_ready:
      craft_hint_label_->setString(
          craft_snapshot.rework_count == 0U
              ? "NEXT INPUT: T / RAIN TRIAL.  The result follows the authored material response."
              : "NEXT INPUT: T / REPEAT RAIN TRIAL after retensioning.");
      break;
    case Stage::rework_required:
      craft_hint_label_->setString(
          "THE TRIAL FOUND A LEAK.  NEXT INPUT: G / RETENSION, then T / TEST AGAIN.");
      break;
    case Stage::completed:
      craft_hint_label_->setString(
          "OUTPUT READY: TUNED CANOPY.  Trial and rework counts remain visible; C returns to the scene.");
      break;
    case Stage::invalid:
      craft_hint_label_->setString("CRAFT SESSION INVALID");
      break;
    }
  }
  if (prompt_label_ != nullptr) {
    if (craft_mode_) {
      prompt_label_->setString(
          "CRAFT SESSION ACTIVE / FOLLOW THE EXPLICIT INPUTS / C TO LEAVE");
    } else if (encounter_snapshot.player_defeated) {
      prompt_label_->setString("PLAYER DOWN / R TO RESTART THE LOCAL ROUTE");
    } else if (encounter_snapshot.terminal_completed) {
      prompt_label_->setString(
          "TWO WAVES COMPLETE / TERMINAL OBJECTIVE PASSED / R TO REPLAY");
    } else if (encounter_snapshot.active_hostile_count > 0) {
      prompt_label_->setString(
          "J OR SPACE / LIGHT    K / HEAVY    CLEAR THE ACTIVE WAVE");
    } else if (gate_open_) {
      prompt_label_->setString("GATE OPEN / AUTHORED WAVE TRANSITION");
    } else if (playerInCraftRange()) {
      prompt_label_->setString(
          craft_snapshot.completed
              ? "C / REVIEW THE PASSED CANOPY AT THE WORKSTATION"
              : "C / USE THE UMBRELLA WORKSTATION");
    } else if (playerInOperateRange()) {
      prompt_label_->setString("F / OPERATE THE GATE CONSOLE");
    } else {
      prompt_label_->setString(
          "CLOSED GATE BLOCKS THE ROUTE / FIND THE CONSOLE");
    }
  }
  if (message_label_ != nullptr) {
    message_label_->setString(message_);
  }
}

std::string SystemDemoLayer::activeWaveName() const {
  const auto active_wave = encounter_.snapshot().active_wave;
  const auto *document = coordinator_.document();
  if (active_wave == 0 || document == nullptr) {
    return "WAITING";
  }
  const auto &waves = document->definition().waves;
  const auto found =
      std::find_if(waves.begin(), waves.end(), [active_wave](const auto &wave) {
        return wave.id.key == active_wave;
      });
  if (found == waves.end()) {
    return "UNKNOWN";
  }
  const std::string name{found->id.name};
  const auto segment = name.find_last_of('.');
  return segment == std::string::npos ? name : name.substr(segment + 1);
}

void SystemDemoLayer::setDirection(const ax::EventKeyboard::KeyCode key,
                                   const bool down) noexcept {
  using KeyCode = ax::EventKeyboard::KeyCode;
  switch (key) {
  case KeyCode::KEY_A:
  case KeyCode::KEY_LEFT_ARROW:
    directions_[static_cast<std::size_t>(Direction::left)] = down;
    break;
  case KeyCode::KEY_D:
  case KeyCode::KEY_RIGHT_ARROW:
    directions_[static_cast<std::size_t>(Direction::right)] = down;
    break;
  case KeyCode::KEY_W:
  case KeyCode::KEY_UP_ARROW:
    directions_[static_cast<std::size_t>(Direction::forward)] = down;
    break;
  case KeyCode::KEY_S:
  case KeyCode::KEY_DOWN_ARROW:
    directions_[static_cast<std::size_t>(Direction::back)] = down;
    break;
  default:
    break;
  }
}

ax::Vec2 SystemDemoLayer::project(const GroundPoseMm &pose) const noexcept {
  const auto *document = coordinator_.document();
  if (document == nullptr) {
    return {};
  }
  const auto &bounds = document->definition().bounds;
  const auto y_span = static_cast<float>(bounds.max_y - bounds.min_y);
  const auto y_scale = y_span > 0.0F ? 520.0F / y_span : 0.0F;
  const auto mid_y =
      (static_cast<float>(bounds.min_y) + static_cast<float>(bounds.max_y)) *
      0.5F;
  const auto relative_y = static_cast<float>(pose.y) - mid_y;
  return {
      750.0F + (static_cast<float>(pose.x) * 0.09F) - (relative_y * 0.014F),
      92.0F + (static_cast<float>(pose.y - bounds.min_y) * y_scale) +
          (static_cast<float>(pose.height) * 0.024F),
  };
}

int SystemDemoLayer::depthOrder(const float screen_y) const noexcept {
  return 10'000 - static_cast<int>(screen_y * 10.0F);
}

ax::Sprite *SystemDemoLayer::createAssetSprite(
    const std::string_view stable_id, const StableContentKey stable_key,
    const SandboxAssetKind kind, const float target_extent) const {
  const auto *set = asset_resolver_.live_set();
  if (set == nullptr) {
    return nullptr;
  }
  const auto *asset = set->find(stable_id, stable_key, kind);
  if (asset == nullptr || asset->canonical_bytes().empty()) {
    return nullptr;
  }

  ax::Data data;
  const auto bytes = asset->canonical_bytes();
  if (data.copy(bytes.data(), static_cast<ssize_t>(bytes.size())) !=
      static_cast<ssize_t>(bytes.size())) {
    return nullptr;
  }
  const auto cache_key =
      std::string{"system-demo/"} + std::string{stable_id} + "/standard";
  auto *sprite = ax::Sprite::create(data, cache_key);
  if (sprite == nullptr) {
    return nullptr;
  }
  if (asset->width() > 0U && asset->height() > 0U) {
    sprite->setAnchorPoint({
        static_cast<float>(asset->root_anchor_x()) /
            static_cast<float>(asset->width()),
        static_cast<float>(asset->root_anchor_y()) /
            static_cast<float>(asset->height()),
    });
  }
  const auto size = sprite->getContentSize();
  const bool use_width = kind == SandboxAssetKind::obstacle;
  const float current_extent = use_width ? size.x : size.y;
  if (current_extent > 0.0F) {
    sprite->setScale(target_extent / current_extent);
  }
  return sprite;
}

float SystemDemoLayer::sceneAssetExtent(
    const SandboxAssetKind kind) const noexcept {
  switch (kind) {
  case SandboxAssetKind::player:
    return 92.0F;
  case SandboxAssetKind::actor:
    return 80.0F;
  case SandboxAssetKind::obstacle:
    return 455.0F;
  case SandboxAssetKind::interaction:
    return 66.0F;
  case SandboxAssetKind::mechanism:
    return 84.0F;
  case SandboxAssetKind::safe_point:
    return 88.0F;
  case SandboxAssetKind::effect:
    return 70.0F;
  }
  return 64.0F;
}

bool SystemDemoLayer::playerInOperateRange() const noexcept {
  const auto pose = coordinator_.snapshot().session.player_pose;
  if (pose.floor_layer != interaction_pose_.floor_layer ||
      interaction_range_mm_ <= 0) {
    return false;
  }
  const auto delta_x = static_cast<std::int64_t>(pose.x) - interaction_pose_.x;
  const auto delta_y = static_cast<std::int64_t>(pose.y) - interaction_pose_.y;
  const auto range = static_cast<std::int64_t>(interaction_range_mm_);
  return (delta_x * delta_x) + (delta_y * delta_y) <= range * range;
}

bool SystemDemoLayer::playerInCraftRange() const noexcept {
  const auto pose = coordinator_.snapshot().session.player_pose;
  if (pose.floor_layer != craft_workstation_pose_.floor_layer) {
    return false;
  }
  const auto delta_x =
      static_cast<std::int64_t>(pose.x) - craft_workstation_pose_.x;
  const auto delta_y =
      static_cast<std::int64_t>(pose.y) - craft_workstation_pose_.y;
  const auto range = static_cast<std::int64_t>(craft_interaction_range_mm);
  return (delta_x * delta_x) + (delta_y * delta_y) <= range * range;
}

bool SystemDemoLayer::currentGateOpen() const noexcept {
  const auto blocker = coordinator_.collision_at(gate_blocker_index_);
  return blocker.has_value() && !blocker->enabled;
}

ax::Scene *createSystemDemoScene() {
  auto *scene = ax::Scene::create();
  auto *layer = SystemDemoLayer::create();
  if (scene == nullptr || layer == nullptr) {
    return nullptr;
  }
  scene->addChild(layer);
  return scene;
}

extern "C" {

TGD_SYSTEM_DEMO_KEEPALIVE int tgd_system_demo_qa_ready() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr && layer->qaReady() ? 1 : 0;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::int32_t tgd_system_demo_qa_player_x() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaPlayerX() : 0;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::int32_t tgd_system_demo_qa_player_y() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaPlayerY() : 0;
}

TGD_SYSTEM_DEMO_KEEPALIVE int tgd_system_demo_qa_gate_open() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr && layer->qaGateOpen() ? 1 : 0;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t tgd_system_demo_qa_retry_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaRetryCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_blocked_move_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaBlockedMoveCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_package_byte_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaPackageByteCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t tgd_system_demo_qa_asset_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaAssetCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::int32_t
tgd_system_demo_qa_player_health() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaPlayerHealth() : 0;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_active_hostile_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaActiveHostileCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_defeated_hostile_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaDefeatedHostileCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_completed_wave_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaCompletedWaveCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_completed_objective_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaCompletedObjectiveCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE int tgd_system_demo_qa_terminal_completed() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr && layer->qaTerminalCompleted() ? 1 : 0;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_accepted_attack_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaAcceptedAttackCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_repeated_trigger_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaRepeatedTriggerCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE int tgd_system_demo_qa_craft_mode() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr && layer->qaCraftMode() ? 1 : 0;
}

TGD_SYSTEM_DEMO_KEEPALIVE int tgd_system_demo_qa_craft_in_range() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr && layer->qaCraftInRange() ? 1 : 0;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_craft_stage() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaCraftStage() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_craft_selected_material() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaCraftSelectedMaterial() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_craft_completed_operations() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaCraftCompletedOperationCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_craft_trial_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaCraftTrialCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_craft_mistake_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaCraftMistakeCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE std::uint32_t
tgd_system_demo_qa_craft_rework_count() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr ? layer->qaCraftReworkCount() : 0U;
}

TGD_SYSTEM_DEMO_KEEPALIVE int tgd_system_demo_qa_craft_completed() {
  const auto *layer = SystemDemoLayer::active();
  return layer != nullptr && layer->qaCraftCompleted() ? 1 : 0;
}

TGD_SYSTEM_DEMO_KEEPALIVE void tgd_system_demo_qa_operate() {
  auto *layer = SystemDemoLayer::active();
  if (layer != nullptr) {
    layer->qaOperate();
  }
}

TGD_SYSTEM_DEMO_KEEPALIVE void tgd_system_demo_qa_attack_light() {
  auto *layer = SystemDemoLayer::active();
  if (layer != nullptr) {
    layer->qaAttackLight();
  }
}

TGD_SYSTEM_DEMO_KEEPALIVE void tgd_system_demo_qa_attack_heavy() {
  auto *layer = SystemDemoLayer::active();
  if (layer != nullptr) {
    layer->qaAttackHeavy();
  }
}

TGD_SYSTEM_DEMO_KEEPALIVE void tgd_system_demo_qa_retry() {
  auto *layer = SystemDemoLayer::active();
  if (layer != nullptr) {
    layer->qaRetry();
  }
}

} // extern "C"
