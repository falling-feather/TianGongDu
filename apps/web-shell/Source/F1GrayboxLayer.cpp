#include "F1GrayboxLayer.hpp"

#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/session_types.hpp>
#include <tgd/runtime/collision_world.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <string_view>

namespace {

constexpr float design_width = 1280.0F;
constexpr float design_height = 720.0F;
constexpr float fixed_tick_seconds = 1.0F / 60.0F;
constexpr std::uint32_t max_catch_up_ticks = 4;

[[nodiscard]] ax::Color4F color(
    std::uint8_t red,
    std::uint8_t green,
    std::uint8_t blue,
    std::uint8_t alpha = 255
) noexcept {
    constexpr float scale = 1.0F / 255.0F;
    return {
        static_cast<float>(red) * scale,
        static_cast<float>(green) * scale,
        static_cast<float>(blue) * scale,
        static_cast<float>(alpha) * scale,
    };
}

void solidPolygon(
    ax::DrawNode* draw,
    std::initializer_list<ax::Vec2> points,
    const ax::Color4F& fill,
    const ax::Color4F& border = ax::Color4F::TRANSPARENT,
    float border_width = 0.0F
) {
    draw->drawSolidPoly(
        points.begin(),
        static_cast<unsigned int>(points.size()),
        fill,
        border_width,
        border
    );
}

[[nodiscard]] ax::Label* label(
    std::string_view text,
    float size,
    const ax::Color4B& tint
) {
    auto* value = ax::Label::createWithSystemFont(text, "Arial", size);
    value->setTextColor(tint);
    value->setAnchorPoint(ax::Vec2(0.0F, 1.0F));
    return value;
}

[[nodiscard]] ax::Node* playerVisual() {
    auto* node = ax::Node::create();
    auto* draw = ax::DrawNode::create();
    draw->drawSolidCircle({0.0F, -2.0F}, 20.0F, 0.0F, 24, 1.45F, 0.38F, color(3, 9, 13, 120));
    solidPolygon(
        draw,
        {{-15.0F, 2.0F}, {15.0F, 2.0F}, {18.0F, 38.0F}, {0.0F, 55.0F}, {-18.0F, 38.0F}},
        color(42, 107, 117),
        color(151, 213, 202),
        1.5F
    );
    draw->drawSolidCircle({0.0F, 63.0F}, 9.0F, 0.0F, 20, color(205, 178, 136));
    draw->drawLine({0.0F, 55.0F}, {0.0F, 91.0F}, color(226, 183, 103), 2.0F);
    solidPolygon(
        draw,
        {{-31.0F, 78.0F}, {-16.0F, 91.0F}, {0.0F, 96.0F}, {17.0F, 91.0F}, {32.0F, 78.0F}, {0.0F, 74.0F}},
        color(190, 147, 82),
        color(246, 214, 146),
        1.5F
    );
    node->addChild(draw);
    return node;
}

[[nodiscard]] ax::Node* shenYanVisual() {
    auto* node = ax::Node::create();
    auto* draw = ax::DrawNode::create();
    draw->drawSolidCircle({0.0F, -2.0F}, 18.0F, 0.0F, 20, 1.35F, 0.35F, color(3, 9, 13, 100));
    solidPolygon(
        draw,
        {{-13.0F, 0.0F}, {13.0F, 0.0F}, {17.0F, 45.0F}, {0.0F, 59.0F}, {-17.0F, 45.0F}},
        color(126, 87, 64),
        color(222, 183, 124),
        1.0F
    );
    draw->drawSolidCircle({0.0F, 67.0F}, 9.0F, 0.0F, 20, color(198, 166, 126));
    draw->drawLine({-16.0F, 48.0F}, {-30.0F, 18.0F}, color(218, 169, 92), 3.0F);
    node->addChild(draw);
    return node;
}

[[nodiscard]] ax::Node* umbrellaDollVisual(bool alternate) {
    auto* node = ax::Node::create();
    auto* draw = ax::DrawNode::create();
    draw->drawSolidCircle({0.0F, -2.0F}, 17.0F, 0.0F, 20, 1.4F, 0.35F, color(3, 8, 12, 110));
    solidPolygon(
        draw,
        {{-13.0F, 0.0F}, {13.0F, 0.0F}, {10.0F, 35.0F}, {0.0F, 46.0F}, {-10.0F, 35.0F}},
        alternate ? color(113, 64, 58) : color(91, 54, 51),
        color(216, 121, 86),
        1.0F
    );
    draw->drawSolidCircle({0.0F, 51.0F}, 7.0F, 0.0F, 18, color(176, 153, 115));
    solidPolygon(
        draw,
        {{-27.0F, 63.0F}, {-12.0F, 75.0F}, {0.0F, 78.0F}, {14.0F, 73.0F}, {28.0F, 60.0F}, {0.0F, 58.0F}},
        alternate ? color(151, 69, 52) : color(122, 62, 55),
        color(230, 134, 88),
        1.0F
    );
    draw->drawLine({18.0F, 64.0F}, {23.0F, 49.0F}, color(112, 183, 178), 2.0F);
    node->addChild(draw);
    return node;
}

[[nodiscard]] ax::Node* paperEgretVisual() {
    auto* node = ax::Node::create();
    auto* draw = ax::DrawNode::create();
    draw->drawSolidCircle({0.0F, -7.0F}, 16.0F, 0.0F, 20, 1.5F, 0.3F, color(3, 8, 12, 90));
    solidPolygon(
        draw,
        {{-42.0F, 26.0F}, {-9.0F, 39.0F}, {0.0F, 30.0F}, {-8.0F, 17.0F}},
        color(178, 190, 176),
        color(230, 220, 181),
        1.0F
    );
    solidPolygon(
        draw,
        {{42.0F, 26.0F}, {9.0F, 39.0F}, {0.0F, 30.0F}, {8.0F, 17.0F}},
        color(163, 181, 171),
        color(230, 220, 181),
        1.0F
    );
    draw->drawSolidCircle({0.0F, 33.0F}, 7.0F, 0.0F, 18, color(217, 210, 175));
    draw->drawLine({5.0F, 35.0F}, {18.0F, 39.0F}, color(210, 139, 70), 2.0F);
    node->addChild(draw);
    return node;
}

[[nodiscard]] ax::Node* bossVisual() {
    auto* node = ax::Node::create();
    auto* draw = ax::DrawNode::create();
    draw->drawSolidCircle({0.0F, -5.0F}, 38.0F, 0.0F, 24, 1.6F, 0.35F, color(2, 7, 10, 130));
    solidPolygon(
        draw,
        {{-34.0F, 0.0F}, {34.0F, 0.0F}, {27.0F, 79.0F}, {0.0F, 102.0F}, {-27.0F, 79.0F}},
        color(57, 52, 63),
        color(177, 126, 91),
        2.0F
    );
    draw->drawSolidCircle({0.0F, 112.0F}, 14.0F, 0.0F, 24, color(190, 167, 127));
    solidPolygon(
        draw,
        {{-76.0F, 126.0F}, {-45.0F, 151.0F}, {0.0F, 165.0F}, {47.0F, 149.0F}, {78.0F, 123.0F}, {0.0F, 111.0F}},
        color(79, 66, 68),
        color(222, 158, 100),
        2.0F
    );
    draw->drawLine({0.0F, 112.0F}, {-45.0F, 151.0F}, color(115, 184, 151), 2.0F);
    draw->drawLine({0.0F, 112.0F}, {47.0F, 149.0F}, color(193, 135, 76), 2.0F);
    draw->drawLine({0.0F, 112.0F}, {-63.0F, 131.0F}, color(166, 159, 102), 2.0F);
    draw->drawLine({0.0F, 112.0F}, {64.0F, 129.0F}, color(168, 197, 214), 2.0F);
    node->addChild(draw);
    return node;
}

}  // namespace

bool F1GrayboxLayer::init() {
    if (!Layer::init()) {
        return false;
    }

    definition_ = content_.find_vertical_slice(
        tgd::contracts::stable_content_key("f1_rainy_umbrella_trial")
    );
    if (definition_ == nullptr) {
        return false;
    }

    auto collision_world = std::make_unique<tgd::runtime::StaticCollisionWorld>();
    const auto& start = definition_->player.initial_pose;
    const std::array blockers{
        tgd::runtime::GroundBlocker{1, start.x - 5'000, start.x - 2'800, -7'500, 7'500, 0, 4'000, 0},
        tgd::runtime::GroundBlocker{2, start.x + 20'500, start.x + 23'000, -7'500, 7'500, 0, 4'000, 0},
        tgd::runtime::GroundBlocker{3, start.x - 5'000, start.x + 23'000, -7'500, -6'700, 0, 4'000, 0},
        tgd::runtime::GroundBlocker{4, start.x - 5'000, start.x + 23'000, 6'700, 7'500, 0, 4'000, 0},
        tgd::runtime::GroundBlocker{5, start.x + 8'200, start.x + 10'400, 1'600, 3'700, 0, 3'000, 0},
        tgd::runtime::GroundBlocker{6, start.x + 13'700, start.x + 15'400, -4'300, -2'500, 0, 3'000, 0},
    };
    if (collision_world->configure(blockers) != tgd::runtime::CollisionWorldError::none ||
        input_.set_camera_basis(definition_->player.camera_basis) !=
            tgd::gameplay::SessionInputError::none ||
        session_.initialize(*definition_, std::move(collision_world)) !=
            tgd::gameplay::VerticalSliceError::none ||
        session_.start() != tgd::gameplay::VerticalSliceError::none) {
        return false;
    }

    createBackdrop();
    createWorld();
    createActors();
    createHud();
    createKeyboardInput();
    updatePlayerPresentation();
    scheduleUpdate();
    return true;
}

void F1GrayboxLayer::update(float delta_seconds) {
    if (delta_seconds <= 0.0F || delta_seconds > 0.25F) {
        tick_accumulator_ = 0.0F;
        return;
    }
    tick_accumulator_ += delta_seconds;
    std::uint32_t executed = 0;
    while (tick_accumulator_ >= fixed_tick_seconds && executed < max_catch_up_ticks) {
        simulateTick();
        tick_accumulator_ -= fixed_tick_seconds;
        ++executed;
    }
    if (executed == max_catch_up_ticks && tick_accumulator_ >= fixed_tick_seconds) {
        tick_accumulator_ = 0.0F;
    }
    updatePlayerPresentation();
}

void F1GrayboxLayer::clearInput(tgd::contracts::InputClearReason reason) noexcept {
    directional_keys_.fill(false);
    jump_pressed_ = false;
    submitted_move_x_ = 0;
    submitted_move_y_ = 0;
    static_cast<void>(input_.clear(++platform_sequence_, reason));
    tick_accumulator_ = 0.0F;
}

void F1GrayboxLayer::shutdown() noexcept {
    clearInput(tgd::contracts::InputClearReason::pause);
    const auto lifecycle = session_.lifecycle();
    if (lifecycle != tgd::gameplay::VerticalSliceLifecycle::uninitialized &&
        lifecycle != tgd::gameplay::VerticalSliceLifecycle::destroyed) {
        static_cast<void>(session_.destroy());
    }
    unscheduleUpdate();
}

void F1GrayboxLayer::createBackdrop() {
    addChild(ax::LayerColor::create(ax::Color4B(7, 17, 24, 255), design_width, design_height), -1000);

    auto* far = ax::DrawNode::create();
    far->drawSolidCircle({1050.0F, 578.0F}, 54.0F, 0.0F, 48, color(220, 211, 170, 220));
    solidPolygon(far, {{0.0F, 300.0F}, {190.0F, 480.0F}, {360.0F, 325.0F}}, color(19, 50, 57));
    solidPolygon(far, {{210.0F, 315.0F}, {470.0F, 530.0F}, {720.0F, 315.0F}}, color(15, 44, 52));
    solidPolygon(far, {{585.0F, 315.0F}, {850.0F, 495.0F}, {1110.0F, 310.0F}}, color(12, 38, 47));
    solidPolygon(far, {{930.0F, 305.0F}, {1130.0F, 452.0F}, {1280.0F, 327.0F}}, color(17, 46, 52));
    solidPolygon(far, {{0.0F, 0.0F}, {1280.0F, 0.0F}, {1280.0F, 310.0F}, {0.0F, 280.0F}}, color(18, 55, 62));
    for (std::uint32_t index = 0; index < 74; ++index) {
        const auto x = static_cast<float>((index * 173U + 31U) % 1320U) - 20.0F;
        const auto y = static_cast<float>((index * 97U + 43U) % 710U) + 10.0F;
        far->drawLine({x, y}, {x - 9.0F, y - 25.0F}, color(113, 180, 190, 78), 1.0F);
    }
    addChild(far, -900);
}

void F1GrayboxLayer::createWorld() {
    world_layer_ = ax::Node::create();
    addChild(world_layer_, 0);

    auto* ground = ax::DrawNode::create();
    const tgd::contracts::GroundPoseMm corner_a{-14'500, -6'500, 0, 0};
    const tgd::contracts::GroundPoseMm corner_b{8'000, -6'500, 0, 0};
    const tgd::contracts::GroundPoseMm corner_c{8'000, 6'500, 0, 0};
    const tgd::contracts::GroundPoseMm corner_d{-14'500, 6'500, 0, 0};
    const std::array ground_points{project(corner_a), project(corner_b), project(corner_c), project(corner_d)};
    ground->drawSolidPoly(
        ground_points.data(),
        static_cast<unsigned int>(ground_points.size()),
        color(49, 73, 68),
        2.0F,
        color(115, 137, 112)
    );
    solidPolygon(
        ground,
        {{110.0F, 115.0F}, {1180.0F, 350.0F}, {1030.0F, 438.0F}, {40.0F, 223.0F}},
        color(69, 85, 72, 210),
        color(137, 135, 97, 160),
        1.0F
    );
    for (std::uint32_t index = 0; index < 12; ++index) {
        const float x = 135.0F + static_cast<float>(index) * 82.0F;
        const float y = 151.0F + static_cast<float>(index) * 18.0F;
        ground->drawLine({x, y}, {x + 62.0F, y + 14.0F}, color(150, 142, 104, 110), 2.0F);
    }
    ground->drawSolidCircle({505.0F, 222.0F}, 48.0F, 0.0F, 28, 1.6F, 0.35F, color(31, 83, 89, 150));
    ground->drawSolidCircle({760.0F, 319.0F}, 38.0F, 0.0F, 28, 1.8F, 0.35F, color(31, 83, 89, 130));
    world_layer_->addChild(ground, -100);

    auto* town = ax::DrawNode::create();
    solidPolygon(town, {{620.0F, 345.0F}, {720.0F, 390.0F}, {720.0F, 493.0F}, {620.0F, 448.0F}}, color(39, 57, 55));
    solidPolygon(town, {{605.0F, 447.0F}, {675.0F, 499.0F}, {744.0F, 470.0F}, {675.0F, 432.0F}}, color(81, 65, 53));
    solidPolygon(town, {{805.0F, 407.0F}, {923.0F, 448.0F}, {923.0F, 555.0F}, {805.0F, 513.0F}}, color(34, 50, 51));
    solidPolygon(town, {{784.0F, 507.0F}, {864.0F, 566.0F}, {951.0F, 531.0F}, {872.0F, 482.0F}}, color(74, 59, 50));
    town->drawLine({926.0F, 443.0F}, {926.0F, 584.0F}, color(181, 139, 76), 5.0F);
    town->drawLine({904.0F, 575.0F}, {948.0F, 575.0F}, color(181, 139, 76), 4.0F);
    world_layer_->addChild(town, -80);

    auto* lanterns = ax::DrawNode::create();
    for (const auto& point : std::array{ax::Vec2(425.0F, 207.0F), ax::Vec2(575.0F, 273.0F), ax::Vec2(735.0F, 346.0F)}) {
        lanterns->drawLine(point, point + ax::Vec2(0.0F, 74.0F), color(84, 67, 49), 4.0F);
        lanterns->drawSolidCircle(point + ax::Vec2(0.0F, 69.0F), 8.0F, 0.0F, 20, color(235, 164, 72, 230));
        lanterns->drawSolidCircle(point + ax::Vec2(0.0F, 69.0F), 17.0F, 0.0F, 20, color(235, 164, 72, 35));
    }
    world_layer_->addChild(lanterns, -40);

    foreground_awning_ = ax::DrawNode::create();
    solidPolygon(
        foreground_awning_,
        {{875.0F, 0.0F}, {1280.0F, 0.0F}, {1280.0F, 185.0F}, {1040.0F, 151.0F}},
        color(42, 47, 42, 235),
        color(121, 95, 66, 230),
        3.0F
    );
    solidPolygon(
        foreground_awning_,
        {{1002.0F, 147.0F}, {1280.0F, 188.0F}, {1280.0F, 232.0F}, {1040.0F, 196.0F}},
        color(91, 65, 50, 235)
    );
    addChild(foreground_awning_, 800);
}

void F1GrayboxLayer::createActors() {
    const auto start = definition_->player.initial_pose;

    auto place = [this](ax::Node* node, tgd::contracts::GroundPoseMm pose) {
        const auto screen = project(pose);
        node->setPosition(screen);
        node->setLocalZOrder(depthOrder(screen.y));
        world_layer_->addChild(node);
    };

    place(shenYanVisual(), {start.x + 1'500, start.y + 1'000, 0, 0});
    place(umbrellaDollVisual(false), {start.x + 8'000, start.y - 1'000, 0, 0});
    place(umbrellaDollVisual(true), {start.x + 9'000, start.y + 1'200, 0, 0});
    place(paperEgretVisual(), {start.x + 10'500, start.y + 2'500, 700, 0});
    place(bossVisual(), {start.x + 16'000, start.y + 3'500, 0, 0});

    auto* evidence = ax::DrawNode::create();
    evidence->drawSolidCircle({0.0F, 0.0F}, 11.0F, 0.0F, 24, color(101, 214, 191, 210));
    evidence->drawSolidCircle({0.0F, 0.0F}, 27.0F, 0.0F, 28, color(101, 214, 191, 32));
    evidence->drawLine({0.0F, -8.0F}, {0.0F, 42.0F}, color(101, 214, 191, 150), 2.0F);
    place(evidence, {start.x + 6'300, start.y + 2'700, 0, 0});

    player_node_ = playerVisual();
    world_layer_->addChild(player_node_);
}

void F1GrayboxLayer::createHud() {
    auto* panel = ax::LayerColor::create(ax::Color4B(5, 15, 20, 214), 470.0F, 94.0F);
    panel->setPosition({22.0F, 604.0F});
    addChild(panel, 1000);

    auto* title = label("F1 · 雨夜试伞 / RAIN FERRY CONTROL GRAYBOX", 19.0F, ax::Color4B(232, 210, 161, 255));
    title->setPosition({38.0F, 684.0F});
    addChild(title, 1001);

    auto* controls = label("WASD / ARROWS  move in depth     SPACE  jump", 14.0F, ax::Color4B(155, 210, 205, 255));
    controls->setPosition({38.0F, 654.0F});
    addChild(controls, 1001);

    auto* state = label("MOVE + HEIGHT CORE: LIVE    COMBAT / QUEST PORTS: RESERVED", 12.0F, ax::Color4B(151, 159, 151, 255));
    state->setPosition({38.0F, 627.0F});
    addChild(state, 1001);

    auto* direction = label("DOUZHANSHEN-FIRST · PANORAMA / SCALE / DEPTH / CONTROLLED CAMERA", 12.0F, ax::Color4B(214, 177, 113, 220));
    direction->setAnchorPoint({1.0F, 0.0F});
    direction->setPosition({1252.0F, 20.0F});
    addChild(direction, 1001);
}

void F1GrayboxLayer::createKeyboardInput() {
    auto* listener = ax::EventListenerKeyboard::create();
    listener->onKeyPressed = [this](ax::EventKeyboard::KeyCode key, ax::Event*) {
        if (key == ax::EventKeyboard::KeyCode::KEY_SPACE) {
            updateJumpKey(true);
        } else {
            updateDirectionalKey(key, true);
        }
    };
    listener->onKeyReleased = [this](ax::EventKeyboard::KeyCode key, ax::Event*) {
        if (key == ax::EventKeyboard::KeyCode::KEY_SPACE) {
            updateJumpKey(false);
        } else {
            updateDirectionalKey(key, false);
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
}

void F1GrayboxLayer::simulateTick() noexcept {
    if (session_.lifecycle() != tgd::gameplay::VerticalSliceLifecycle::running) {
        return;
    }
    const auto next_tick = session_.current_snapshot().tick + 1;
    const auto commands = input_.commands_for_tick(
        next_tick,
        definition_->player.actor,
        command_sequence_
    );
    command_sequence_ += commands.size;
    if (session_.submit_movement(commands.view()) != tgd::gameplay::VerticalSliceError::none) {
        return;
    }
    static_cast<void>(session_.advance(1));
}

void F1GrayboxLayer::updatePlayerPresentation() noexcept {
    if (player_node_ == nullptr) {
        return;
    }
    const auto position = project(session_.current_snapshot().player_pose);
    player_node_->setPosition(position);
    player_node_->setLocalZOrder(depthOrder(position.y));
    if (foreground_awning_ != nullptr) {
        const bool behind_awning = position.x > 890.0F && position.y < 225.0F;
        foreground_awning_->setOpacity(behind_awning ? 88 : 238);
    }
}

void F1GrayboxLayer::updateDirectionalKey(
    ax::EventKeyboard::KeyCode key,
    bool pressed
) noexcept {
    const auto index = directionalKeyIndex(key);
    if (index < 0 || directional_keys_[static_cast<std::size_t>(index)] == pressed) {
        return;
    }
    directional_keys_[static_cast<std::size_t>(index)] = pressed;
    submitAxisState();
}

void F1GrayboxLayer::updateJumpKey(bool pressed) noexcept {
    if (jump_pressed_ == pressed) {
        return;
    }
    jump_pressed_ = pressed;
    const tgd::contracts::ScalarActionSample sample{
        ++platform_sequence_,
        tgd::contracts::action_id("jump"),
        pressed ? tgd::contracts::ground_axis_one : 0,
        pressed ? tgd::contracts::ActionSampleEdge::pressed
                : tgd::contracts::ActionSampleEdge::released,
        false,
    };
    static_cast<void>(input_.submit(std::span{&sample, 1}));
}

void F1GrayboxLayer::submitAxisState() noexcept {
    const bool left = directional_keys_[0] || directional_keys_[1];
    const bool right = directional_keys_[2] || directional_keys_[3];
    const bool up = directional_keys_[4] || directional_keys_[5];
    const bool down = directional_keys_[6] || directional_keys_[7];
    const auto move_x = (right ? tgd::contracts::ground_axis_one : 0) -
                        (left ? tgd::contracts::ground_axis_one : 0);
    const auto move_y = (up ? tgd::contracts::ground_axis_one : 0) -
                        (down ? tgd::contracts::ground_axis_one : 0);

    std::array<tgd::contracts::ScalarActionSample, 2> samples{};
    std::size_t count = 0;
    if (move_x != submitted_move_x_) {
        submitted_move_x_ = move_x;
        samples[count++] = {
            ++platform_sequence_,
            tgd::contracts::action_id("move_x"),
            move_x,
            tgd::contracts::ActionSampleEdge::value_changed,
            false,
        };
    }
    if (move_y != submitted_move_y_) {
        submitted_move_y_ = move_y;
        samples[count++] = {
            ++platform_sequence_,
            tgd::contracts::action_id("move_y"),
            move_y,
            tgd::contracts::ActionSampleEdge::value_changed,
            false,
        };
    }
    if (count != 0) {
        static_cast<void>(input_.submit(std::span{samples}.first(count)));
    }
}

ax::Vec2 F1GrayboxLayer::project(const tgd::contracts::GroundPoseMm& pose) const noexcept {
    const auto& origin = definition_->player.initial_pose;
    const auto relative_x = static_cast<float>(pose.x - origin.x);
    const auto relative_y = static_cast<float>(pose.y - origin.y);
    return {
        380.0F + (relative_x - relative_y) * 0.032F,
        170.0F + (relative_x + relative_y) * 0.014F +
            static_cast<float>(pose.height - origin.height) * 0.040F,
    };
}

int F1GrayboxLayer::directionalKeyIndex(ax::EventKeyboard::KeyCode key) const noexcept {
    using KeyCode = ax::EventKeyboard::KeyCode;
    switch (key) {
        case KeyCode::KEY_A:
            return 0;
        case KeyCode::KEY_LEFT_ARROW:
            return 1;
        case KeyCode::KEY_D:
            return 2;
        case KeyCode::KEY_RIGHT_ARROW:
            return 3;
        case KeyCode::KEY_W:
            return 4;
        case KeyCode::KEY_UP_ARROW:
            return 5;
        case KeyCode::KEY_S:
            return 6;
        case KeyCode::KEY_DOWN_ARROW:
            return 7;
        default:
            return -1;
    }
}

int F1GrayboxLayer::depthOrder(float screen_y) const noexcept {
    return 10'000 - static_cast<int>(screen_y * 10.0F);
}

ax::Scene* createF1GrayboxScene(F1GrayboxLayer** layer_out) {
    auto* scene = ax::Scene::create();
    auto* layer = F1GrayboxLayer::create();
    if (scene == nullptr || layer == nullptr) {
        return nullptr;
    }
    scene->addChild(layer);
    if (layer_out != nullptr) {
        *layer_out = layer;
    }
    return scene;
}
