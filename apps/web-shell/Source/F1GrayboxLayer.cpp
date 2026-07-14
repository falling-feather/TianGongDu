#include "F1GrayboxLayer.hpp"

#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/session_types.hpp>
#include <tgd/runtime/collision_world.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>

namespace {

constexpr float design_width = 1280.0F;
constexpr float design_height = 720.0F;
constexpr float fixed_tick_seconds = 1.0F / 60.0F;
constexpr std::uint32_t max_catch_up_ticks = 4;
constexpr std::uint8_t combat_feedback_ticks = 12;
inline constexpr std::array<std::string_view, 7> quest_beat_labels{
    "RAIN FERRY ARRIVAL",
    "SHEN YAN TRAINING",
    "UMBRELLA LANE ENCOUNTER",
    "WORKBENCH INVESTIGATION",
    "CANOPY RETURN ENCOUNTER",
    "FOUR SEASONS WRAITH",
    "RESOLUTION AND RETURN",
};

[[nodiscard]] std::string playtimeText(std::uint64_t ticks) {
    const auto total_seconds = ticks / 60U;
    const auto minutes = total_seconds / 60U;
    const auto seconds = total_seconds % 60U;
    return std::to_string(minutes) + ":" + (seconds < 10U ? "0" : "") +
           std::to_string(seconds);
}

[[nodiscard]] std::uint32_t qaTickCount(std::uint64_t ticks) noexcept {
    return ticks > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(ticks);
}

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

[[nodiscard]] ax::Color4F bossPhaseColor(
    tgd::contracts::StableContentKey stance,
    std::uint8_t alpha
) noexcept {
    if (stance == tgd::contracts::stable_content_key("stance_wraith_summer")) {
        return color(231, 111, 58, alpha);
    }
    if (stance == tgd::contracts::stable_content_key("stance_wraith_autumn")) {
        return color(220, 169, 72, alpha);
    }
    if (stance == tgd::contracts::stable_content_key("stance_wraith_winter")) {
        return color(112, 191, 218, alpha);
    }
    return color(111, 199, 143, alpha);
}

[[nodiscard]] std::string_view bossPhaseName(
    tgd::contracts::StableContentKey stance
) noexcept {
    if (stance == tgd::contracts::stable_content_key("stance_wraith_summer")) {
        return "SUMMER";
    }
    if (stance == tgd::contracts::stable_content_key("stance_wraith_autumn")) {
        return "AUTUMN";
    }
    if (stance == tgd::contracts::stable_content_key("stance_wraith_winter")) {
        return "WINTER";
    }
    return "SPRING";
}

void drawBossPhaseAura(
    ax::DrawNode* aura,
    tgd::contracts::StableContentKey stance,
    std::int32_t health,
    std::int32_t health_max
) {
    if (aura == nullptr) {
        return;
    }
    aura->clear();
    const auto ratio = health_max > 0
                           ? static_cast<float>(health) / static_cast<float>(health_max)
                           : 0.0F;
    const auto radius = 84.0F + 18.0F * std::clamp(ratio, 0.0F, 1.0F);
    aura->drawSolidCircle({0.0F, 72.0F}, radius, 0.0F, 40, bossPhaseColor(stance, 28));
    aura->drawCircle(
        {0.0F, 72.0F},
        radius,
        0.0F,
        40,
        false,
        1.0F,
        1.0F,
        bossPhaseColor(stance, 205),
        3.0F
    );
    aura->drawCircle(
        {0.0F, 72.0F},
        radius - 12.0F,
        0.0F,
        40,
        false,
        1.0F,
        1.0F,
        bossPhaseColor(stance, 92),
        2.0F
    );
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

[[nodiscard]] ax::Node* bossVisual(ax::DrawNode** aura_out) {
    auto* node = ax::Node::create();
    auto* aura = ax::DrawNode::create();
    node->addChild(aura, -1);
    if (aura_out != nullptr) {
        *aura_out = aura;
    }
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

[[nodiscard]] ax::Node* questInteractionVisual(
    tgd::contracts::QuestInteractionKind kind
) {
    const bool inspect = kind == tgd::contracts::QuestInteractionKind::inspect;
    const auto accent = inspect ? color(103, 213, 191, 235) : color(230, 174, 87, 235);
    auto* node = ax::Node::create();
    auto* draw = ax::DrawNode::create();
    draw->drawSolidCircle({0.0F, 0.0F}, 11.0F, 0.0F, 24, accent);
    draw->drawSolidCircle({0.0F, 0.0F}, 27.0F, 0.0F, 28, color(103, 213, 191, 34));
    draw->drawLine({0.0F, 8.0F}, {0.0F, 39.0F}, accent, 2.0F);
    solidPolygon(
        draw,
        {{0.0F, 52.0F}, {8.0F, 42.0F}, {0.0F, 32.0F}, {-8.0F, 42.0F}},
        accent,
        color(242, 222, 171, 245),
        1.0F
    );
    node->addChild(draw);
    auto* key = label("F", 12.0F, ax::Color4B(5, 18, 22, 255));
    key->setAnchorPoint({0.5F, 0.5F});
    key->setPosition({0.0F, 42.0F});
    node->addChild(key, 1);
    return node;
}

[[nodiscard]] ax::Node* workbenchPropVisual(
    tgd::contracts::StableContentKey interaction
) {
    const auto spring_trace =
        tgd::contracts::stable_content_key("f1_interaction_reveal_spring_trace");
    const auto spring_trace_from_drain = tgd::contracts::stable_content_key(
        "f1_interaction_reveal_spring_trace_from_drain"
    );
    const auto winter_trace =
        tgd::contracts::stable_content_key("f1_interaction_reveal_winter_trace");
    const auto ledger =
        tgd::contracts::stable_content_key("f1_interaction_review_shared_ledger");
    const auto spring_rib =
        tgd::contracts::stable_content_key("f1_interaction_calibrate_rib_spring");
    const bool is_shortcut = interaction == tgd::contracts::stable_content_key(
                                               "f1_interaction_open_return_shortcut"
                                           );
    const bool is_ledger = interaction == ledger;
    const bool is_calibration = interaction == spring_rib ||
                                interaction == tgd::contracts::stable_content_key(
                                                   "f1_interaction_calibrate_rib_winter"
                                               );
    const auto accent = interaction == spring_trace ||
                                interaction == spring_trace_from_drain ||
                                interaction == spring_rib
                            ? color(116, 205, 135, 235)
                            : interaction == winter_trace
                                  ? color(128, 193, 225, 235)
                                  : is_ledger ? color(218, 173, 103, 235)
                                              : color(134, 180, 220, 235);

    auto* node = ax::Node::create();
    auto* draw = ax::DrawNode::create();
    if (is_shortcut) {
        draw->drawSolidCircle(
            {0.0F, -6.0F},
            34.0F,
            0.0F,
            28,
            1.65F,
            0.38F,
            color(2, 8, 11, 115)
        );
        draw->drawLine({-26.0F, -12.0F}, {-26.0F, 58.0F}, color(119, 88, 56), 7.0F);
        draw->drawLine({26.0F, -12.0F}, {26.0F, 58.0F}, color(119, 88, 56), 7.0F);
        solidPolygon(
            draw,
            {{-39.0F, 58.0F}, {0.0F, 76.0F}, {39.0F, 58.0F}, {0.0F, 49.0F}},
            color(83, 62, 49),
            color(211, 162, 91),
            1.5F
        );
        draw->drawLine({-16.0F, 5.0F}, {16.0F, 40.0F}, color(113, 211, 185), 3.0F);
        node->addChild(draw);
        return node;
    }
    draw->drawSolidCircle({0.0F, -4.0F}, 30.0F, 0.0F, 28, 1.65F, 0.38F, color(2, 8, 11, 110));
    solidPolygon(
        draw,
        {{-32.0F, 0.0F}, {32.0F, 0.0F}, {27.0F, 12.0F}, {-27.0F, 12.0F}},
        color(63, 60, 51),
        color(148, 118, 77),
        1.5F
    );
    draw->drawLine({-22.0F, 1.0F}, {-18.0F, -25.0F}, color(82, 66, 48), 4.0F);
    draw->drawLine({22.0F, 1.0F}, {18.0F, -25.0F}, color(82, 66, 48), 4.0F);
    if (is_ledger) {
        solidPolygon(
            draw,
            {{-21.0F, 15.0F}, {22.0F, 19.0F}, {18.0F, 48.0F}, {-24.0F, 44.0F}},
            color(174, 151, 108),
            accent,
            1.5F
        );
        for (float y : std::array{25.0F, 32.0F, 39.0F}) {
            draw->drawLine({-15.0F, y}, {12.0F, y + 2.0F}, color(75, 62, 49), 1.0F);
        }
    } else if (is_calibration) {
        draw->drawLine({0.0F, 13.0F}, {0.0F, 59.0F}, accent, 3.0F);
        draw->drawLine({0.0F, 21.0F}, {-28.0F, 47.0F}, accent, 2.0F);
        draw->drawLine({0.0F, 25.0F}, {-13.0F, 59.0F}, accent, 2.0F);
        draw->drawLine({0.0F, 25.0F}, {13.0F, 59.0F}, accent, 2.0F);
        draw->drawLine({0.0F, 21.0F}, {28.0F, 47.0F}, accent, 2.0F);
        draw->drawCircle({0.0F, 25.0F}, 8.0F, 0.0F, 24, false, accent);
    } else {
        draw->drawSolidCircle({0.0F, 29.0F}, 16.0F, 0.0F, 24, color(20, 42, 43, 220));
        draw->drawCircle({0.0F, 29.0F}, 22.0F, 0.0F, 28, false, accent);
        draw->drawLine({-12.0F, 29.0F}, {12.0F, 29.0F}, accent, 2.0F);
        draw->drawLine({0.0F, 17.0F}, {0.0F, 41.0F}, accent, 2.0F);
    }
    node->addChild(draw);
    return node;
}

[[nodiscard]] std::string_view interactionPrompt(
    tgd::contracts::StableContentKey interaction,
    tgd::contracts::QuestInteractionKind kind
) noexcept {
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_travel_writ")) {
        return "F / INSPECT TRAVEL WRIT";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_read_flood_marks")) {
        return "F / READ FLOOD MARKS";
    }
    if (interaction ==
        tgd::contracts::stable_content_key("f1_interaction_secure_ferry_mooring")) {
        return "F / SECURE FERRY MOORING";
    }
    if (interaction ==
        tgd::contracts::stable_content_key("f1_interaction_raise_wayfinding_lantern")) {
        return "F / RAISE WAYFINDING LANTERN";
    }
    if (interaction ==
        tgd::contracts::stable_content_key("f1_interaction_sound_workshop_bell")) {
        return "F / SOUND WORKSHOP BELL";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_ferry_gate")) {
        return "F / OPEN FERRY GATE";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_meet_shen_yan")) {
        return "F / TALK TO SHEN YAN";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_choose_lane_route")) {
        return "F / CHOOSE CANOPY ROUTE";
    }
    if (interaction ==
        tgd::contracts::stable_content_key("f1_interaction_choose_lane_drain_route")) {
        return "F / CHOOSE DRAIN ROUTE";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_reveal_spring_trace")) {
        return "F / REVEAL SPRING TRACE / CANOPY ENTRY";
    }
    if (interaction == tgd::contracts::stable_content_key(
                           "f1_interaction_reveal_spring_trace_from_drain"
                       )) {
        return "F / REVEAL SPRING TRACE / DRAIN ENTRY";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_reveal_winter_trace")) {
        return "F / REVEAL WINTER TRACE";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_review_shared_ledger")) {
        return "F / REVIEW SHARED LEDGER";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_calibrate_rib_spring")) {
        return "F / LOCK SPRING RIB CALIBRATION";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_calibrate_rib_winter")) {
        return "F / LOCK WINTER RIB CALIBRATION";
    }
    if (interaction ==
        tgd::contracts::stable_content_key("f1_interaction_prime_return_calibration")) {
        return "F / PRIME RETURN CALIBRATION FRAME";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_open_return_shortcut")) {
        return "F / OPEN CANOPY RETURN SHORTCUT";
    }
    if (interaction == tgd::contracts::stable_content_key("f1_interaction_resolution_subdue")) {
        return "F / RESOLUTION: DIRECT SUBDUE";
    }
    if (interaction == tgd::contracts::stable_content_key(
                           "f1_interaction_resolution_restore_shared_mark"
                       )) {
        return "F / RESOLUTION: RESTORE SHARED MARK";
    }
    if (interaction == tgd::contracts::stable_content_key(
                           "f1_interaction_return_to_shen_yan"
                       )) {
        return "F / RETURN FINDINGS TO SHEN YAN";
    }
    switch (kind) {
        case tgd::contracts::QuestInteractionKind::inspect:
            return "F / INSPECT";
        case tgd::contracts::QuestInteractionKind::operate:
            return "F / OPERATE";
        case tgd::contracts::QuestInteractionKind::talk:
            return "F / TALK";
        case tgd::contracts::QuestInteractionKind::choose:
            return "F / CHOOSE";
    }
    return "F / INTERACT";
}

}  // namespace

bool F1GrayboxLayer::init() {
    if (!Layer::init()) {
        return false;
    }

    definition_ = content_.find_vertical_slice(
        tgd::contracts::stable_content_key("f1_rainy_umbrella_trial")
    );
    combat_definition_ = content_.find_combat_encounter(
        tgd::contracts::stable_content_key("f1_encounter_umbrella_lane_bootstrap")
    );
    if (definition_ == nullptr || combat_definition_ == nullptr) {
        return false;
    }
    if (std::any_of(
            definition_->quest_encounter_activations.begin(),
            definition_->quest_encounter_activations.end(),
            [this](const tgd::contracts::QuestEncounterActivationDefinition& activation) {
                return activation.encounter_id.key != combat_definition_->id.key;
            }
        )) {
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
        session_.start() != tgd::gameplay::VerticalSliceError::none ||
        (!definition_->quest_ui_cues.empty() &&
         quest_ui_projection_.initialize(*definition_) !=
             tgd::gameplay::QuestUiProjectionError::none) ||
        quest_interactions_.initialize(definition_->quest_interactions) !=
            tgd::gameplay::QuestInteractionError::none ||
        quest_combat_triggers_.initialize(definition_->quest_combat_triggers) !=
            tgd::gameplay::QuestCombatTriggerError::none ||
        quest_combat_outcomes_.initialize(definition_->quest_combat_outcomes) !=
            tgd::gameplay::QuestCombatOutcomeError::none ||
        quest_boss_phases_.initialize(definition_->quest_boss_phases) !=
            tgd::gameplay::QuestBossPhaseError::none ||
        quest_resolution_rewards_.initialize(definition_->quest_resolution_rewards) !=
            tgd::gameplay::QuestResolutionRewardError::none ||
        combat_.initialize(combat_definition_->actors, combat_definition_->abilities) !=
            tgd::gameplay::CombatError::none ||
        combat_.start() != tgd::gameplay::CombatError::none ||
        encounter_.initialize(
            combat_definition_->director,
            combat_definition_->actors,
            combat_definition_->abilities
        ) != tgd::gameplay::EncounterDirectorError::none) {
        return false;
    }

    createBackdrop();
    createWorld();
    createActors();
    createHud();
    createKeyboardInput();
    updatePlayerPresentation();
    updateCombatPresentation();
    updateQuestPresentation();
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
    updateCombatPresentation();
    updateQuestPresentation();
}

void F1GrayboxLayer::clearInput(tgd::contracts::InputClearReason reason) noexcept {
    clearHeldInput(reason, true);
    tick_accumulator_ = 0.0F;
}

void F1GrayboxLayer::setRewardClaimSink(IF1RewardClaimSink* sink) noexcept {
    reward_claim_sink_ = sink;
}

void F1GrayboxLayer::setQuestUiProjectionSink(IF1QuestUiProjectionSink* sink) noexcept {
    quest_ui_projection_sink_ = sink;
}

tgd::gameplay::QuestUiSelectionIntentError
F1GrayboxLayer::submitQuestUiSelectionIntent(
    const tgd::contracts::QuestUiSelectionIntent& intent
) noexcept {
    using tgd::gameplay::QuestUiSelectionIntentError;
    if (!quest_ui_projection_.initialized()) {
        return QuestUiSelectionIntentError::invalid_lifecycle;
    }
    if (!quest_ui_choice_.matches(intent)) {
        return QuestUiSelectionIntentError::stale_projection;
    }
    const auto validated =
        quest_ui_projection_.validate_choice_intent(intent, session_.quest_runtime());
    if (validated != QuestUiSelectionIntentError::none) {
        return validated;
    }

    const auto completed = session_.complete_objective(
        intent.objective,
        intent.selection,
        *this
    );
    if (completed.error != tgd::gameplay::VerticalSliceError::none ||
        !completed.accepted) {
        switch (completed.error) {
            case tgd::gameplay::VerticalSliceError::objective_not_active:
                return QuestUiSelectionIntentError::objective_not_active;
            case tgd::gameplay::VerticalSliceError::invalid_selection:
                return QuestUiSelectionIntentError::selection_not_authored;
            case tgd::gameplay::VerticalSliceError::selection_conflict:
                return QuestUiSelectionIntentError::selection_already_committed;
            default:
                return QuestUiSelectionIntentError::quest_context_changed;
        }
    }
    quest_ui_choice_.finish();
    publishAcceptedChoiceFeedback(intent);
    return QuestUiSelectionIntentError::none;
}

F1GrayboxLayer::QuestUiPublication F1GrayboxLayer::publishQuestUiProjection(
    const tgd::contracts::QuestUiProjectionSignal& signal
) noexcept {
    QuestUiPublication publication;
    if (!quest_ui_projection_.initialized()) {
        return publication;
    }
    const auto projected = quest_ui_projection_.project(
        signal,
        session_.quest_runtime(),
        session_.current_snapshot().safe_point_id.key,
        combat_.actors()
    );
    if (projected.error != tgd::gameplay::QuestUiProjectionError::none) {
        return publication;
    }
    publication.projection = projected.projection;
    publication.projected = true;
    publication.external_consumer_accepted =
        quest_ui_projection_sink_ != nullptr &&
        quest_ui_projection_sink_->submitF1QuestUiProjection(projected.projection);
    return publication;
}

void F1GrayboxLayer::publishAcceptedChoiceFeedback(
    const tgd::contracts::QuestUiSelectionIntent& intent
) noexcept {
    const auto stage = session_.quest_snapshot().stage;
    const auto beat = std::find_if(
        definition_->beats.begin(),
        definition_->beats.end(),
        [stage](const tgd::contracts::VerticalSliceBeatDefinition& candidate) {
            return candidate.id.key == stage;
        }
    );
    if (beat == definition_->beats.end()) {
        return;
    }
    const auto origin = std::find_if(
        beat->objectives.begin(),
        beat->objectives.end(),
        [&intent](const tgd::contracts::ContentId& objective) {
            return objective.key == intent.objective;
        }
    );
    if (origin == beat->objectives.end()) {
        return;
    }

    tgd::contracts::QuestUiProjectionSignal signal;
    signal.source = tgd::contracts::QuestUiProjectionSource::interaction_feedback;
    signal.primary_result = {
        intent.interaction,
        intent.objective,
        tgd::contracts::QuestUiResultStatus::accepted,
        tgd::contracts::QuestUiRejectionReason::none,
    };
    const auto next = origin + 1;
    if (next != beat->objectives.end() &&
        session_.objective_state(next->key) == tgd::gameplay::QuestObjectiveState::active) {
        signal.objective = next->key;
        if (publishQuestUiProjection(signal).projected) {
            return;
        }
    }
    signal.objective = intent.objective;
    static_cast<void>(publishQuestUiProjection(signal));
}

void F1GrayboxLayer::notifyRewardClaimCommitted(
    tgd::contracts::StableContentKey reward_dedup_key
) noexcept {
    if (reward_dedup_key == 0 || reward_dedup_key != resolution_reward_dedup_key_) {
        return;
    }
    resolution_reward_committed_ = true;
    if (combat_event_label_ != nullptr) {
        combat_event_label_->setString(
            "RESOLUTION COMMITTED / REWARD CLAIM PERSISTED / REPLAY DEDUP ARMED"
        );
    }
}

void F1GrayboxLayer::clearHeldInput(
    tgd::contracts::InputClearReason reason,
    bool release_guard
) noexcept {
    const bool guard_was_held = combat_keys_[5] || combat_keys_[6];
    directional_keys_.fill(false);
    combat_keys_.fill(false);
    combat_intent_count_ = 0;
    if (release_guard && guard_was_held &&
        combat_.lifecycle() == tgd::gameplay::CombatLifecycle::running) {
        queueCombatIntent(tgd::contracts::CombatCommandType::guard_ended);
    }
    jump_pressed_ = false;
    interact_pressed_ = false;
    submitted_move_x_ = 0;
    submitted_move_y_ = 0;
    static_cast<void>(input_.clear(++platform_sequence_, reason));
}

void F1GrayboxLayer::shutdown() noexcept {
    clearInput(tgd::contracts::InputClearReason::pause);
    quest_ui_choice_.finish();
    const auto lifecycle = session_.lifecycle();
    if (lifecycle != tgd::gameplay::VerticalSliceLifecycle::uninitialized &&
        lifecycle != tgd::gameplay::VerticalSliceLifecycle::destroyed) {
        static_cast<void>(session_.destroy());
    }
    const auto combat_lifecycle = combat_.lifecycle();
    if (combat_lifecycle != tgd::gameplay::CombatLifecycle::uninitialized &&
        combat_lifecycle != tgd::gameplay::CombatLifecycle::destroyed) {
        static_cast<void>(combat_.destroy());
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

    quest_marker_count_ = 0;
    for (const auto& interaction : definition_->quest_interactions) {
        if (quest_marker_count_ >= quest_marker_capacity) {
            break;
        }
        auto* marker = questInteractionVisual(interaction.kind);
        const auto screen = project(interaction.pose);
        marker->setPosition(screen);
        world_layer_->addChild(marker, depthOrder(screen.y) + 2);
        quest_marker_nodes_[quest_marker_count_] = marker;
        quest_marker_definitions_[quest_marker_count_] = &interaction;
        ++quest_marker_count_;
    }

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
    std::size_t hostile_index = 0;
    for (const auto& actor : combat_definition_->actors) {
        if (actor.faction != tgd::contracts::CombatFaction::hostile ||
            hostile_index >= hostile_capacity) {
            continue;
        }
        ax::Node* visual = nullptr;
        if (actor.archetype_id.key == definition_->boss_id.key) {
            visual = bossVisual(&boss_phase_aura_);
        } else if (actor.archetype_id.key ==
                       tgd::contracts::stable_content_key(
                           "jn_enemy_faded_paper_egret"
                       ) ||
                   actor.archetype_id.key ==
                       tgd::contracts::stable_content_key(
                           "f1_training_egret_rig"
                       )) {
            visual = paperEgretVisual();
        } else {
            visual = umbrellaDollVisual((actor.actor % 2U) == 0U);
        }
        place(visual, actor.initial_pose);
        hostile_nodes_[hostile_index] = visual;
        hostile_actor_keys_[hostile_index] = actor.actor;
        ++hostile_index;
    }

    auto* evidence = ax::DrawNode::create();
    evidence->drawSolidCircle({0.0F, 0.0F}, 11.0F, 0.0F, 24, color(101, 214, 191, 210));
    evidence->drawSolidCircle({0.0F, 0.0F}, 27.0F, 0.0F, 28, color(101, 214, 191, 32));
    evidence->drawLine({0.0F, -8.0F}, {0.0F, 42.0F}, color(101, 214, 191, 150), 2.0F);
    place(evidence, {start.x + 6'300, start.y + 2'700, 0, 0});

    const auto workbench_cell =
        tgd::contracts::stable_content_key("f1_cell_canopy_workstation");
    quest_prop_count_ = 0;
    for (const auto& interaction : definition_->quest_interactions) {
        if (interaction.cell_id.key == workbench_cell &&
            quest_prop_count_ < quest_marker_capacity) {
            auto* prop = workbenchPropVisual(interaction.id.key);
            place(prop, interaction.pose);
            quest_prop_nodes_[quest_prop_count_] = prop;
            quest_prop_definitions_[quest_prop_count_] = &interaction;
            ++quest_prop_count_;
        }
    }

    player_node_ = playerVisual();
    world_layer_->addChild(player_node_);

    combat_fx_ = ax::DrawNode::create();
    world_layer_->addChild(combat_fx_, 500);
}

void F1GrayboxLayer::createHud() {
    auto* panel = ax::LayerColor::create(ax::Color4B(5, 15, 20, 218), 1010.0F, 154.0F);
    panel->setPosition({22.0F, 542.0F});
    addChild(panel, 1000);

    auto* title = label("F1 / RAIN FERRY PLAYABLE GRAYBOX", 19.0F, ax::Color4B(232, 210, 161, 255));
    title->setPosition({38.0F, 684.0F});
    addChild(title, 1001);

    auto* controls = label(
        "WASD move | F interact | SPACE jump | J light | K heavy | SHIFT guard | C evade | 1/2 stance | R retry",
        13.0F,
        ax::Color4B(155, 210, 205, 255)
    );
    controls->setPosition({38.0F, 654.0F});
    addChild(controls, 1001);

    playtime_audit_label_ = label("", 12.0F, ax::Color4B(151, 159, 151, 255));
    playtime_audit_label_->setPosition({38.0F, 627.0F});
    addChild(playtime_audit_label_, 1001);

    combat_resources_label_ = label("", 13.0F, ax::Color4B(230, 193, 126, 255));
    combat_resources_label_->setPosition({38.0F, 605.0F});
    addChild(combat_resources_label_, 1001);

    quest_state_label_ = label("", 12.0F, ax::Color4B(151, 213, 198, 255));
    quest_state_label_->setPosition({38.0F, 582.0F});
    addChild(quest_state_label_, 1001);

    combat_event_label_ = label(
        "Inspect the travel writ, then prepare the Rain Ferry route.",
        12.0F,
        ax::Color4B(174, 217, 203, 255)
    );
    combat_event_label_->setPosition({38.0F, 560.0F});
    addChild(combat_event_label_, 1001);

    interaction_prompt_label_ = label("", 18.0F, ax::Color4B(244, 218, 157, 255));
    interaction_prompt_label_->setAnchorPoint({0.5F, 0.5F});
    interaction_prompt_label_->setPosition({640.0F, 112.0F});
    interaction_prompt_label_->setVisible(false);
    addChild(interaction_prompt_label_, 1001);

    auto* direction = label(
        "DOUZHANSHEN-FIRST / 2.5D OBLIQUE PANORAMA / SCALE / DEPTH / CONTROLLED CAMERA",
        12.0F,
        ax::Color4B(214, 177, 113, 220)
    );
    direction->setAnchorPoint({1.0F, 0.0F});
    direction->setPosition({1252.0F, 20.0F});
    addChild(direction, 1001);
    refreshCombatHud();
    updateQuestPresentation();
}

void F1GrayboxLayer::createKeyboardInput() {
    auto* listener = ax::EventListenerKeyboard::create();
    listener->onKeyPressed = [this](ax::EventKeyboard::KeyCode key, ax::Event*) {
        if (quest_ui_choice_.pending()) {
            if (quest_ui_choice_.native_pending()) {
                const auto option_index = nativeChoiceKeyIndex(key);
                if (option_index >= 0) {
                    const auto selection = quest_ui_choice_.native_intent(
                        static_cast<std::size_t>(option_index)
                    );
                    if (selection.error == F1QuestUiChoiceError::none) {
                        const auto submitted = submitQuestUiSelectionIntent(selection.intent);
                        if (combat_event_label_ != nullptr) {
                            combat_event_label_->setString(
                                submitted ==
                                        tgd::gameplay::QuestUiSelectionIntentError::none
                                    ? "CHOICE ACCEPTED"
                                    : "CHOICE REJECTED - PRESS A LISTED NUMBER TO RETRY"
                            );
                        }
                    }
                }
            }
            return;
        }
        if (key == ax::EventKeyboard::KeyCode::KEY_R) {
            retry_requested_ = player_defeated_;
            return;
        }
        if (player_defeated_) {
            return;
        }
        if (key == ax::EventKeyboard::KeyCode::KEY_F) {
            updateInteractKey(true);
            return;
        }
        if (updateCombatKey(key, true)) {
            return;
        }
        if (key == ax::EventKeyboard::KeyCode::KEY_SPACE) {
            updateJumpKey(true);
        } else {
            updateDirectionalKey(key, true);
        }
    };
    listener->onKeyReleased = [this](ax::EventKeyboard::KeyCode key, ax::Event*) {
        if (quest_ui_choice_.pending()) {
            return;
        }
        if (key == ax::EventKeyboard::KeyCode::KEY_R || player_defeated_) {
            return;
        }
        if (key == ax::EventKeyboard::KeyCode::KEY_F) {
            updateInteractKey(false);
            return;
        }
        if (updateCombatKey(key, false)) {
            return;
        }
        if (key == ax::EventKeyboard::KeyCode::KEY_SPACE) {
            updateJumpKey(false);
        } else {
            updateDirectionalKey(key, false);
        }
    };
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
}

void F1GrayboxLayer::renderNativeQuestUiChoice() noexcept {
    if (combat_event_label_ == nullptr || !quest_ui_choice_.native_pending()) {
        return;
    }
    std::string text = "QUEST CHOICE - PRESS A LISTED NUMBER";
    for (std::size_t index = 0; index < quest_ui_choice_.option_count(); ++index) {
        text += "\n";
        text += std::to_string(index + 1U);
        text += "  ";
        text += quest_ui_choice_.option_label(index);
    }
    combat_event_label_->setString(text);
}

void F1GrayboxLayer::simulateTick() noexcept {
    if (session_.lifecycle() != tgd::gameplay::VerticalSliceLifecycle::running) {
        return;
    }
    if (quest_ui_choice_.pending()) {
        return;
    }
    if (retry_requested_ && !retryEncounter()) {
        retry_requested_ = false;
        if (combat_event_label_ != nullptr) {
            combat_event_label_->setString("RETRY REJECTED / SESSION BOUNDARY DRIFT");
        }
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
    const auto movement_result = session_.advance(1);
    if (movement_result.error != tgd::gameplay::VerticalSliceError::none ||
        movement_result.executed_ticks != 1) {
        return;
    }
    if (commands.interact_pressed) {
        const auto& snapshot = session_.current_snapshot();
        const auto interaction = quest_interactions_.resolve(
            {definition_->player.actor, snapshot.cell_id.key, snapshot.player_pose},
            session_.quest_runtime()
        );
        if (interaction.error != tgd::gameplay::QuestInteractionError::none) {
            if (combat_event_label_ != nullptr) {
                combat_event_label_->setString("INTERACTION REJECTED / QUERY CONTRACT DRIFT");
            }
            return;
        }
        if (!interaction.found) {
            if (combat_event_label_ != nullptr) {
                combat_event_label_->setString("NO ACTIVE QUEST INTERACTION IN RANGE");
            }
        } else if (
            interaction.kind == tgd::contracts::QuestInteractionKind::choose &&
            quest_ui_projection_.has_authored_cue(
                session_.quest_runtime().snapshot().stage,
                interaction.objective,
                tgd::contracts::QuestUiProjectionSource::choice_available
            )
        ) {
            tgd::contracts::QuestUiProjectionSignal signal;
            signal.source = tgd::contracts::QuestUiProjectionSource::choice_available;
            signal.objective = interaction.objective;
            const auto publication = publishQuestUiProjection(signal);
            if (publication.projected &&
                quest_ui_choice_.begin(
                    publication.projection,
                    publication.external_consumer_accepted
                ) == F1QuestUiChoiceError::none) {
                clearHeldInput(tgd::contracts::InputClearReason::context_changed, true);
                if (quest_ui_choice_.native_pending()) {
                    renderNativeQuestUiChoice();
                }
            } else if (combat_event_label_ != nullptr) {
                combat_event_label_->setString("CHOICE PANEL REJECTED / QUEST UI PROJECTION DRIFT");
            }
        } else {
            const auto completed = session_.complete_objective(
                interaction.objective,
                interaction.selection,
                *this
            );
            if (completed.error != tgd::gameplay::VerticalSliceError::none) {
                if (combat_event_label_ != nullptr) {
                    combat_event_label_->setString("OBJECTIVE REJECTED / QUEST STATE DRIFT");
                }
                return;
            }
        }
    }
    if (!applyPendingEncounterActivation()) {
        return;
    }
    if (!submitCombatTick(next_tick)) {
        return;
    }
    if (!applyPendingEncounterActivation()) {
        return;
    }
    if (player_action_ticks_ > 0) {
        --player_action_ticks_;
    }
    if (player_hit_ticks_ > 0) {
        --player_hit_ticks_;
    }
    if (attack_fx_ticks_ > 0) {
        --attack_fx_ticks_;
    }
    for (auto& flash : hostile_flash_ticks_) {
        if (flash > 0) {
            --flash;
        }
    }
    refreshCombatHud();
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

void F1GrayboxLayer::updateCombatPresentation() noexcept {
    if (player_node_ == nullptr) {
        return;
    }
    const auto player_snapshot = std::find_if(
        combat_.actors().begin(),
        combat_.actors().end(),
        [this](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == definition_->player.actor;
        }
    );
    const bool player_active =
        player_snapshot != combat_.actors().end() && player_snapshot->active;
    player_node_->setOpacity(player_active ? 255 : 96);
    player_node_->setScale(
        1.0F + static_cast<float>(player_action_ticks_) * 0.008F +
        static_cast<float>(player_hit_ticks_) * 0.012F
    );
    const auto player_position = project(session_.current_snapshot().player_pose);
    if (combat_fx_ != nullptr) {
        combat_fx_->clear();
        if (attack_fx_ticks_ > 0) {
            const auto progress = static_cast<float>(combat_feedback_ticks - attack_fx_ticks_) /
                                  static_cast<float>(combat_feedback_ticks);
            combat_fx_->drawCircle(
                player_position + ax::Vec2(24.0F, 33.0F),
                35.0F + progress * 58.0F,
                0.0F,
                36,
                false,
                1.55F,
                0.48F,
                color(109, 226, 205, static_cast<std::uint8_t>(220.0F * (1.0F - progress))),
                3.0F
            );
        }
    }

    for (std::size_t node_index = 0; node_index < hostile_capacity; ++node_index) {
        auto* node = hostile_nodes_[node_index];
        if (node == nullptr) {
            continue;
        }
        const auto actor_key = hostile_actor_keys_[node_index];
        const auto snapshot = std::find_if(
            combat_.actors().begin(),
            combat_.actors().end(),
            [actor_key](const tgd::contracts::CombatActorSnapshot& actor) {
                return actor.actor == actor_key;
            }
        );
        if (snapshot == combat_.actors().end()) {
            node->setVisible(false);
            continue;
        }
        node->setVisible(snapshot->active);
        node->setPosition(project(snapshot->pose));
        node->setLocalZOrder(depthOrder(node->getPositionY()));
        const bool boss = snapshot->actor == 201;
        node->setScale(
            (boss ? 1.18F : 1.0F) +
            static_cast<float>(hostile_flash_ticks_[node_index]) * 0.018F
        );
        if (boss && snapshot->active) {
            drawBossPhaseAura(
                boss_phase_aura_,
                snapshot->stance,
                snapshot->resources.health,
                snapshot->resources.health_max
            );
        }
    }
}

void F1GrayboxLayer::updateQuestPresentation() noexcept {
    const auto& snapshot = session_.current_snapshot();
    if (playtime_audit_label_ != nullptr) {
        const auto& audit = snapshot.playtime;
        playtime_audit_label_->setString(
            "AUDIT Q " + playtimeText(audit.eligible_ticks) + "/" +
            playtimeText(audit.playable_target_ticks) + " | IDLE " +
            playtimeText(audit.idle_ticks) + " | RETRY " +
            playtimeText(audit.failure_retry_ticks) + " | TARGETS " +
            std::to_string(audit.beat_targets_met) + "/" +
            std::to_string(audit.beat_count) +
            (audit.playable_target_met ? " | 1H PASS" : " | 1H PENDING")
        );
    }
    if (quest_state_label_ != nullptr) {
        const auto beat_name = snapshot.beat_index < quest_beat_labels.size()
                                   ? quest_beat_labels[snapshot.beat_index]
                                   : std::string_view{"UNKNOWN BEAT"};
        quest_state_label_->setString(
            "QUEST " + std::to_string(snapshot.beat_index + 1U) + "/" +
            std::to_string(snapshot.beat_count) + " | OBJECTIVES " +
            std::to_string(snapshot.completed_objectives) + "/" +
            std::to_string(snapshot.required_objectives) + " | CHOICES " +
            std::to_string(snapshot.selected_choices) + " | " + std::string{beat_name}
        );
    }
    const auto selection_matches = [this](
                                       const tgd::contracts::QuestInteractionDefinition&
                                           interaction
                                   ) noexcept {
        return interaction.required_selection_id.key == 0 ||
               session_.quest_runtime().selected_option(
                   interaction.required_selection_objective_id.key
               ) == interaction.required_selection_id.key;
    };
    for (std::size_t index = 0; index < quest_marker_count_; ++index) {
        const auto* interaction = quest_marker_definitions_[index];
        if (quest_marker_nodes_[index] != nullptr && interaction != nullptr) {
            quest_marker_nodes_[index]->setVisible(
                session_.objective_state(interaction->objective_id.key) ==
                    tgd::gameplay::QuestObjectiveState::active &&
                selection_matches(*interaction)
            );
        }
    }
    for (std::size_t index = 0; index < quest_prop_count_; ++index) {
        const auto* interaction = quest_prop_definitions_[index];
        if (quest_prop_nodes_[index] != nullptr && interaction != nullptr) {
            quest_prop_nodes_[index]->setVisible(selection_matches(*interaction));
        }
    }
    if (interaction_prompt_label_ == nullptr) {
        return;
    }
    const auto interaction = quest_interactions_.resolve(
        {definition_->player.actor, snapshot.cell_id.key, snapshot.player_pose},
        session_.quest_runtime()
    );
    const bool show = !player_defeated_ &&
                      interaction.error == tgd::gameplay::QuestInteractionError::none &&
                      interaction.found;
    interaction_prompt_label_->setVisible(show);
    if (show) {
        interaction_prompt_label_->setString(
            interactionPrompt(interaction.interaction, interaction.kind)
        );
    }
}

void F1GrayboxLayer::publish(
    std::span<const tgd::contracts::CombatEvent> events
) noexcept {
    for (const auto& event : events) {
        const bool accepted_player_combat =
            (event.source == definition_->player.actor &&
             (event.type == tgd::contracts::CombatEventType::ability_started ||
              event.type == tgd::contracts::CombatEventType::guard_changed ||
              event.type == tgd::contracts::CombatEventType::stance_changed)) ||
            (event.target == definition_->player.actor &&
             (event.type == tgd::contracts::CombatEventType::hit_guarded ||
              event.type == tgd::contracts::CombatEventType::hit_evaded));
        if (accepted_player_combat &&
            session_.report_playtime_activity(tgd::gameplay::PlaytimeActivityKind::combat) !=
                tgd::gameplay::VerticalSliceError::none &&
            combat_event_label_ != nullptr) {
            combat_event_label_->setString("PLAYTIME AUDIT REJECTED / SESSION STATE DRIFT");
        }
        submitQuestCombatSignal(event);
        submitQuestCombatOutcome(event);
        if (event.type == tgd::contracts::CombatEventType::ability_started &&
            event.source == definition_->player.actor) {
            player_action_ticks_ = combat_feedback_ticks;
            if ((event.feedback_tags & tgd::contracts::feedback_evade) == 0U) {
                attack_fx_ticks_ = combat_feedback_ticks;
            }
            if (combat_event_label_ != nullptr) {
                combat_event_label_->setString(
                    (event.feedback_tags & tgd::contracts::feedback_evade) != 0U
                        ? "EVADE WINDOW ACTIVE"
                        : "ABILITY COMMITTED / WAIT FOR HIT WINDOW"
                );
            }
        } else if (event.type == tgd::contracts::CombatEventType::ability_started) {
            const auto ability = std::find_if(
                combat_definition_->abilities.begin(),
                combat_definition_->abilities.end(),
                [&event](const tgd::contracts::AbilityDefinition& definition) {
                    return definition.id.key == event.ability;
                }
            );
            if (ability != combat_definition_->abilities.end()) {
                incoming_attack_tick_ = event.tick + ability->windup_ticks;
                incoming_attack_source_ = event.source;
            }
            for (std::size_t index = 0; index < hostile_capacity; ++index) {
                if (hostile_actor_keys_[index] == event.source) {
                    hostile_flash_ticks_[index] = combat_feedback_ticks;
                }
            }
            if (combat_event_label_ != nullptr) {
                combat_event_label_->setString("HOSTILE ATTACK TELEGRAPH / GUARD OR EVADE");
            }
        }
        if (event.type == tgd::contracts::CombatEventType::hit_landed ||
            event.type == tgd::contracts::CombatEventType::hit_guarded ||
            event.type == tgd::contracts::CombatEventType::poise_broken) {
            for (std::size_t index = 0; index < hostile_capacity; ++index) {
                if (hostile_actor_keys_[index] == event.target) {
                    hostile_flash_ticks_[index] = combat_feedback_ticks;
                }
            }
        }
        if ((event.type == tgd::contracts::CombatEventType::hit_landed ||
             event.type == tgd::contracts::CombatEventType::hit_guarded ||
             event.type == tgd::contracts::CombatEventType::poise_broken) &&
            event.target == definition_->player.actor) {
            player_hit_ticks_ = combat_feedback_ticks;
        }
        if ((event.type == tgd::contracts::CombatEventType::attack_missed ||
             event.type == tgd::contracts::CombatEventType::hit_landed ||
             event.type == tgd::contracts::CombatEventType::hit_guarded ||
             event.type == tgd::contracts::CombatEventType::hit_evaded) &&
            event.source == incoming_attack_source_) {
            incoming_attack_tick_ = 0;
            incoming_attack_source_ = 0;
        }
        if (event.type == tgd::contracts::CombatEventType::actor_defeated &&
            event.target == definition_->player.actor) {
            player_defeated_ = true;
            retry_requested_ = false;
            clearHeldInput(tgd::contracts::InputClearReason::player_defeated, false);
        } else if (event.type == tgd::contracts::CombatEventType::encounter_restarted) {
            player_defeated_ = false;
            retry_requested_ = false;
        }
        if (event.type == tgd::contracts::CombatEventType::stance_changed &&
            event.source == 201) {
            pending_boss_stance_ = 0;
        }
        if (combat_event_label_ == nullptr) {
            continue;
        }
        switch (event.type) {
            case tgd::contracts::CombatEventType::stance_changed:
                if (event.source == 201) {
                    combat_event_label_->setString(
                        "WRAITH PHASE: " + std::string{bossPhaseName(event.ability)}
                    );
                } else {
                    combat_event_label_->setString(
                        event.ability == tgd::contracts::stable_content_key("stance_flower_turn")
                            ? "STANCE: FLOWER TURN"
                            : "STANCE: EAVESGUARD"
                    );
                }
                break;
            case tgd::contracts::CombatEventType::guard_changed:
                combat_event_label_->setString(event.value != 0 ? "GUARD HELD" : "GUARD RELEASED");
                break;
            case tgd::contracts::CombatEventType::attack_missed:
                combat_event_label_->setString("MISS / MOVE INTO RANGE AND MATCH HEIGHT");
                break;
            case tgd::contracts::CombatEventType::hit_landed:
                combat_event_label_->setString(
                    event.target == definition_->player.actor
                        ? "PLAYER HIT / HEALTH AND POISE RESOLVED"
                        : "HIT CONFIRMED / HEALTH AND POISE RESOLVED"
                );
                break;
            case tgd::contracts::CombatEventType::hit_guarded:
                combat_event_label_->setString(
                    event.target == definition_->player.actor
                        ? "GUARD ABSORBED THE HOSTILE IMPACT"
                        : "GUARDED HIT / POISE ABSORBED THE IMPACT"
                );
                break;
            case tgd::contracts::CombatEventType::hit_evaded:
                combat_event_label_->setString("EVADED / ACTIVE WINDOW WON");
                break;
            case tgd::contracts::CombatEventType::poise_broken:
                combat_event_label_->setString("POISE BROKEN");
                break;
            case tgd::contracts::CombatEventType::actor_defeated:
                combat_event_label_->setString(
                    event.target == definition_->player.actor
                        ? "PLAYER DEFEATED / PRESS R TO RETRY"
                        : "HOSTILE DEFEATED"
                );
                break;
            case tgd::contracts::CombatEventType::encounter_restarted:
                combat_event_label_->setString("ENCOUNTER RETRIED / SAFE POINT RESTORED");
                break;
            case tgd::contracts::CombatEventType::encounter_replaced:
                combat_event_label_->setString("ENCOUNTER GROUP REPLACED");
                break;
            case tgd::contracts::CombatEventType::encounter_reinforced:
                combat_event_label_->setString("ENCOUNTER REINFORCEMENT DEPLOYED");
                break;
            case tgd::contracts::CombatEventType::command_ignored:
                if (event.source == definition_->player.actor) {
                    combat_event_label_->setString("ACTION UNAVAILABLE / RECOVERY OR RESOURCE LOCK");
                }
                break;
            default:
                break;
        }
    }
    submitQuestBossPhase();
}

void F1GrayboxLayer::publish(
    std::span<const tgd::contracts::QuestEvent> events
) noexcept {
    for (const auto& event : events) {
        if (event.type == tgd::contracts::QuestEventType::quest_resolved) {
            const auto receipt = quest_resolution_rewards_.resolve(session_.quest_runtime());
            if (receipt.error == tgd::gameplay::QuestResolutionRewardError::none &&
                receipt.found) {
                resolution_reward_ = receipt.reward;
                resolution_reward_dedup_key_ = receipt.reward_dedup_key;
                if (reward_claim_sink_ != nullptr) {
                    reward_claim_sink_->submitF1RewardClaim({
                        receipt.resolution,
                        receipt.reward,
                        receipt.reward_dedup_key,
                    });
                }
            }
        }
        if (event.type == tgd::contracts::QuestEventType::objective_completed) {
            const auto activation = std::find_if(
                definition_->quest_encounter_activations.begin(),
                definition_->quest_encounter_activations.end(),
                [&event](
                    const tgd::contracts::QuestEncounterActivationDefinition& candidate
                ) {
                    return candidate.trigger_objective_id.key == event.objective;
                }
            );
            if (activation != definition_->quest_encounter_activations.end()) {
                pending_encounter_activation_beat_ = activation->beat_id.key;
                pending_encounter_activation_objective_ = event.objective;
            }
        } else if (event.type == tgd::contracts::QuestEventType::stage_advanced) {
            pending_encounter_activation_beat_ = event.stage;
            pending_encounter_activation_objective_ = 0;
        }
        if (combat_event_label_ == nullptr) {
            continue;
        }
        switch (event.type) {
            case tgd::contracts::QuestEventType::objective_completed:
                if (event.objective == tgd::contracts::stable_content_key(
                                           "f1_objective_read_flood_marks"
                                       )) {
                    combat_event_label_->setString("FLOOD MARKS READ / SAFE WATERLINE FOUND");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_secure_ferry_mooring"
                                              )) {
                    combat_event_label_->setString("FERRY MOORING SECURED / CURRENT HELD");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_raise_wayfinding_lantern"
                                              )) {
                    combat_event_label_->setString("WAYFINDING LANTERN RAISED / ROUTE VISIBLE");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_sound_workshop_bell"
                                              )) {
                    combat_event_label_->setString("WORKSHOP BELL SOUNDED / GATE CREW READY");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_commit_eavesguard_heavy"
                                              )) {
                    combat_event_label_->setString("EAVESGUARD HEAVY COMMITTED / WEIGHT LEARNED");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_eavesguard_counter"
                                              )) {
                    combat_event_label_->setString(
                        "EAVESGUARD COUNTER COMPLETE / EGRET RIG RELEASED"
                    );
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_enter_flower_turn"
                                              )) {
                    combat_event_label_->setString("FLOWER TURN ENTERED / TEMPO SHIFTED");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_commit_flower_turn_light"
                                              )) {
                    combat_event_label_->setString("FLOWER LIGHT COMMITTED / QUICK ARC LEARNED");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_flower_turn_counter"
                                              )) {
                    combat_event_label_->setString(
                        "FLOWER COUNTER COMPLETE / UMBRELLA LANE UNLOCKED"
                    );
                } else if (event.objective == tgd::contracts::stable_content_key(
                                           "f1_objective_reveal_spring_trace"
                                       )) {
                    combat_event_label_->setString("SPRING TRACE REVEALED / EVIDENCE COMMITTED");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_reveal_winter_trace"
                                              )) {
                    combat_event_label_->setString("WINTER TRACE REVEALED / EVIDENCE COMMITTED");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_review_shared_ledger"
                                              )) {
                    combat_event_label_->setString("SHARED LEDGER REVIEWED / CALIBRATION UNLOCKED");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_demonstrate_rib_calibration"
                                              )) {
                    combat_event_label_->setString(
                        "RIB CALIBRATION ACTION VERIFIED / CLEAR THE RETURN GROUP"
                    );
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_survive_spring_phase"
                                              )) {
                    combat_event_label_->setString("SPRING SEAL BROKEN / SUMMER PHASE RISING");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_survive_summer_phase"
                                              )) {
                    combat_event_label_->setString("SUMMER SEAL BROKEN / AUTUMN PHASE RISING");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_survive_autumn_phase"
                                              )) {
                    combat_event_label_->setString("AUTUMN SEAL BROKEN / WINTER PHASE RISING");
                } else if (event.objective == tgd::contracts::stable_content_key(
                                                  "f1_objective_survive_winter_phase"
                                              )) {
                    combat_event_label_->setString("WINTER SEAL BROKEN / WRAITH DEFEATED");
                } else {
                    combat_event_label_->setString("OBJECTIVE COMPLETE / QUEST STATE COMMITTED");
                }
                break;
            case tgd::contracts::QuestEventType::stage_advanced:
                if (event.selection == tgd::contracts::stable_content_key(
                                           "f1_choice_rib_spring_calibration"
                                       )) {
                    combat_event_label_->setString(
                        "SPRING RIB LOCKED / CANOPY RETURN ENCOUNTER UNLOCKED"
                    );
                } else if (event.selection == tgd::contracts::stable_content_key(
                                                  "f1_choice_rib_winter_calibration"
                                              )) {
                    combat_event_label_->setString(
                        "WINTER RIB LOCKED / CANOPY RETURN ENCOUNTER UNLOCKED"
                    );
                } else if (session_.current_snapshot().beat_index < quest_beat_labels.size()) {
                    combat_event_label_->setString(
                        "BEAT ADVANCED / " +
                        std::string{
                            quest_beat_labels[session_.current_snapshot().beat_index]
                        } +
                        " UNLOCKED"
                    );
                } else {
                    combat_event_label_->setString("BEAT ADVANCED");
                }
                break;
            case tgd::contracts::QuestEventType::quest_resolved:
                if (resolution_reward_committed_) {
                    combat_event_label_->setString(
                        "RESOLUTION COMMITTED / REWARD CLAIM PERSISTED / REPLAY DEDUP ARMED"
                    );
                } else {
                    combat_event_label_->setString(
                        resolution_reward_ != 0 && resolution_reward_dedup_key_ != 0
                            ? "RESOLUTION COMMITTED / REWARD CLAIM SUBMITTED / PROFILE COMMIT PENDING"
                            : "RESOLUTION REWARD REJECTED / CONTRACT DRIFT"
                    );
                }
                break;
            case tgd::contracts::QuestEventType::objective_already_completed:
                combat_event_label_->setString("OBJECTIVE ALREADY COMPLETE");
                break;
        }
    }
    updateQuestPresentation();
}

void F1GrayboxLayer::submitQuestCombatSignal(
    const tgd::contracts::CombatEvent& event
) noexcept {
    const bool player_action =
        event.source == definition_->player.actor &&
        (event.type == tgd::contracts::CombatEventType::ability_started ||
         event.type == tgd::contracts::CombatEventType::stance_changed);
    const bool player_defense =
        event.target == definition_->player.actor &&
        (event.type == tgd::contracts::CombatEventType::hit_guarded ||
         event.type == tgd::contracts::CombatEventType::hit_evaded);
    if (!player_action && !player_defense) {
        return;
    }
    const auto actors = combat_.actors();
    const auto player = std::find_if(
        actors.begin(),
        actors.end(),
        [this](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == definition_->player.actor;
        }
    );
    if (player == actors.end()) {
        return;
    }
    auto kind = tgd::contracts::QuestCombatTriggerKind::player_hit_evaded;
    if (event.type == tgd::contracts::CombatEventType::ability_started) {
        kind = tgd::contracts::QuestCombatTriggerKind::player_ability_started;
    } else if (event.type == tgd::contracts::CombatEventType::stance_changed) {
        kind = tgd::contracts::QuestCombatTriggerKind::player_stance_changed;
    } else if (event.type == tgd::contracts::CombatEventType::hit_guarded) {
        kind = tgd::contracts::QuestCombatTriggerKind::player_hit_guarded;
    }
    const auto resolved = quest_combat_triggers_.resolve(
        {
            player->actor,
            kind,
            player->stance,
            event.type == tgd::contracts::CombatEventType::ability_started
                ? event.ability
                : 0,
        },
        session_.quest_runtime()
    );
    if (resolved.error != tgd::gameplay::QuestCombatTriggerError::none) {
        if (combat_event_label_ != nullptr) {
            combat_event_label_->setString("COMBAT QUEST SIGNAL REJECTED / CONTRACT DRIFT");
        }
        return;
    }
    if (resolved.found) {
        const auto completed = session_.complete_objective(resolved.objective, *this);
        if (completed.error != tgd::gameplay::VerticalSliceError::none &&
            combat_event_label_ != nullptr) {
            combat_event_label_->setString("COMBAT OBJECTIVE REJECTED / QUEST STATE DRIFT");
        }
    }
}

void F1GrayboxLayer::submitQuestCombatOutcome(
    const tgd::contracts::CombatEvent& event
) noexcept {
    const bool hostile_defeated =
        event.type == tgd::contracts::CombatEventType::actor_defeated &&
        event.target != definition_->player.actor;
    const bool quest_signal_may_have_unlocked_outcome =
        (event.source == definition_->player.actor &&
         (event.type == tgd::contracts::CombatEventType::ability_started ||
          event.type == tgd::contracts::CombatEventType::stance_changed)) ||
        (event.target == definition_->player.actor &&
         (event.type == tgd::contracts::CombatEventType::hit_guarded ||
          event.type == tgd::contracts::CombatEventType::hit_evaded));
    if (!hostile_defeated && !quest_signal_may_have_unlocked_outcome) {
        return;
    }
    const auto resolved = quest_combat_outcomes_.resolve(
        combat_.actors(),
        session_.quest_runtime()
    );
    if (resolved.error != tgd::gameplay::QuestCombatOutcomeError::none) {
        if (combat_event_label_ != nullptr) {
            combat_event_label_->setString("COMBAT OUTCOME REJECTED / SNAPSHOT CONTRACT DRIFT");
        }
        return;
    }
    if (resolved.found) {
        const auto completed = session_.complete_objective(resolved.objective, *this);
        if (completed.error != tgd::gameplay::VerticalSliceError::none &&
            combat_event_label_ != nullptr) {
            combat_event_label_->setString("COMBAT OUTCOME REJECTED / QUEST STATE DRIFT");
        }
    }
}

void F1GrayboxLayer::submitQuestBossPhase() noexcept {
    const auto resolved = quest_boss_phases_.resolve(
        combat_.actors(),
        session_.quest_runtime()
    );
    if (resolved.error != tgd::gameplay::QuestBossPhaseError::none) {
        if (combat_event_label_ != nullptr) {
            combat_event_label_->setString("BOSS PHASE REJECTED / SNAPSHOT CONTRACT DRIFT");
        }
        return;
    }
    if (!resolved.found) {
        return;
    }
    const auto completed = session_.complete_objective(resolved.objective, *this);
    if (completed.error != tgd::gameplay::VerticalSliceError::none) {
        if (combat_event_label_ != nullptr) {
            combat_event_label_->setString("BOSS OBJECTIVE REJECTED / QUEST STATE DRIFT");
        }
        return;
    }
    if (completed.accepted) {
        pending_boss_stance_ = resolved.next_stance;
    }
}

void F1GrayboxLayer::syncBossStanceForQuest() noexcept {
    pending_boss_stance_ = 0;
    for (std::size_t index = 0; index < definition_->quest_boss_phases.size(); ++index) {
        if (session_.objective_state(definition_->quest_boss_phases[index].objective_id.key) !=
            tgd::gameplay::QuestObjectiveState::active) {
            continue;
        }
        if (index != 0) {
            pending_boss_stance_ = definition_->quest_boss_phases[index - 1].next_stance;
        }
        return;
    }
}

std::int32_t F1GrayboxLayer::qaPlayerHealth() const noexcept {
    const auto actors = combat_.actors();
    const auto player = std::find_if(
        actors.begin(),
        actors.end(),
        [this](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == definition_->player.actor;
        }
    );
    return player == actors.end() ? -1 : player->resources.health;
}

bool F1GrayboxLayer::qaPlayerActive() const noexcept {
    const auto actors = combat_.actors();
    const auto player = std::find_if(
        actors.begin(),
        actors.end(),
        [this](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == definition_->player.actor;
        }
    );
    return player != actors.end() && player->active;
}

std::uint32_t F1GrayboxLayer::qaActiveHostiles() const noexcept {
    const auto actors = combat_.actors();
    return static_cast<std::uint32_t>(std::count_if(
        actors.begin(),
        actors.end(),
        [](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.faction == tgd::contracts::CombatFaction::hostile && actor.active;
        }
    ));
}

std::uint32_t F1GrayboxLayer::qaRetryCount() const noexcept {
    return retry_count_;
}

std::uint32_t F1GrayboxLayer::qaQuestBeatIndex() const noexcept {
    return session_.current_snapshot().beat_index;
}

std::uint32_t F1GrayboxLayer::qaQuestCompletedObjectives() const noexcept {
    return session_.current_snapshot().completed_objectives;
}

std::uint32_t F1GrayboxLayer::qaQuestRequiredObjectives() const noexcept {
    return session_.current_snapshot().required_objectives;
}

std::uint32_t F1GrayboxLayer::qaQuestSelectedChoices() const noexcept {
    return session_.current_snapshot().selected_choices;
}

bool F1GrayboxLayer::qaQuestResolved() const noexcept {
    return session_.quest_snapshot().resolved;
}

bool F1GrayboxLayer::qaResolutionRewardReady() const noexcept {
    return resolution_reward_ != 0 && resolution_reward_dedup_key_ != 0;
}

bool F1GrayboxLayer::qaResolutionRewardCommitted() const noexcept {
    return resolution_reward_committed_;
}

std::int32_t F1GrayboxLayer::qaSafePointPoseX() const noexcept {
    return session_.current_snapshot().safe_point_pose.x;
}

std::int32_t F1GrayboxLayer::qaSafePointPoseY() const noexcept {
    return session_.current_snapshot().safe_point_pose.y;
}

std::int32_t F1GrayboxLayer::qaPlayerPoseX() const noexcept {
    return session_.current_snapshot().player_pose.x;
}

std::int32_t F1GrayboxLayer::qaPlayerPoseY() const noexcept {
    return session_.current_snapshot().player_pose.y;
}

std::uint32_t F1GrayboxLayer::qaEligiblePlayTicks() const noexcept {
    return qaTickCount(session_.current_snapshot().playtime.eligible_ticks);
}

std::uint32_t F1GrayboxLayer::qaIdleTicks() const noexcept {
    return qaTickCount(session_.current_snapshot().playtime.idle_ticks);
}

std::uint32_t F1GrayboxLayer::qaFailureRetryTicks() const noexcept {
    return qaTickCount(session_.current_snapshot().playtime.failure_retry_ticks);
}

std::uint32_t F1GrayboxLayer::qaBeatTargetsMet() const noexcept {
    return session_.current_snapshot().playtime.beat_targets_met;
}

bool F1GrayboxLayer::qaPlayableTargetMet() const noexcept {
    return session_.current_snapshot().playtime.playable_target_met;
}

std::uint32_t F1GrayboxLayer::qaIncomingAttackTicks() const noexcept {
    if (incoming_attack_tick_ <= combat_.current_tick()) {
        return 0;
    }
    const auto remaining = incoming_attack_tick_ - combat_.current_tick();
    return remaining > std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(remaining);
}

bool F1GrayboxLayer::qaPlayerBusy() const noexcept {
    const auto actors = combat_.actors();
    const auto player = std::find_if(
        actors.begin(),
        actors.end(),
        [this](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == definition_->player.actor;
        }
    );
    return player != actors.end() && player->active_ability != 0;
}

bool F1GrayboxLayer::submitCombatTick(tgd::contracts::TickIndex tick) noexcept {
    if (combat_.lifecycle() != tgd::gameplay::CombatLifecycle::running ||
        combat_.current_tick() + 1 != tick) {
        return false;
    }
    const auto encounter_plan = encounter_.plan_tick(
        tick,
        combat_.actors(),
        encounter_command_sequence_
    );
    if (encounter_plan.error != tgd::gameplay::EncounterDirectorError::none) {
        return false;
    }
    encounter_command_sequence_ += encounter_plan.batch.command_count;
    const tgd::contracts::CombatPoseUpdate pose_update{
        tick,
        definition_->player.actor,
        session_.current_snapshot().player_pose,
    };
    if (combat_.synchronize_poses(std::span{&pose_update, 1}) !=
        tgd::gameplay::CombatError::none) {
        return false;
    }
    if (!encounter_plan.batch.poses().empty() &&
        combat_.synchronize_poses(encounter_plan.batch.poses()) !=
            tgd::gameplay::CombatError::none) {
        return false;
    }

    std::array<
        tgd::contracts::CombatCommand,
        combat_intent_capacity + tgd::gameplay::EncounterPlanBatch::capacity + 1>
        commands{};
    std::size_t command_count = 0;
    for (std::size_t index = 0; index < combat_intent_count_; ++index) {
        const auto& intent = combat_intents_[index];
        tgd::contracts::StableActorKey target = 0;
        if (intent.type == tgd::contracts::CombatCommandType::light_attack ||
            intent.type == tgd::contracts::CombatCommandType::heavy_attack) {
            target = nearestActiveHostile();
            if (target == 0) {
                if (combat_event_label_ != nullptr) {
                    combat_event_label_->setString("NO ACTIVE HOSTILE TARGET");
                }
                continue;
            }
        }
        commands[command_count++] = {
            tick,
            definition_->player.actor,
            combat_command_sequence_++,
            intent.type,
            target,
            intent.stance,
        };
    }
    for (const auto& command : encounter_plan.batch.command_view()) {
        commands[command_count++] = command;
    }
    if (pending_boss_stance_ != 0 && !definition_->quest_boss_phases.empty()) {
        const auto boss_actor = definition_->quest_boss_phases.front().actor;
        const auto boss = std::find_if(
            combat_.actors().begin(),
            combat_.actors().end(),
            [boss_actor](const tgd::contracts::CombatActorSnapshot& actor) {
                return actor.actor == boss_actor;
            }
        );
        if (boss != combat_.actors().end() && boss->stance == pending_boss_stance_) {
            pending_boss_stance_ = 0;
        } else if (boss != combat_.actors().end() && boss->active) {
            commands[command_count++] = {
                tick,
                boss_actor,
                encounter_command_sequence_++,
                tgd::contracts::CombatCommandType::switch_stance,
                0,
                pending_boss_stance_,
            };
        }
    }
    if (command_count != 0 &&
        combat_.submit(std::span{commands}.first(command_count)) !=
            tgd::gameplay::CombatError::none) {
        return false;
    }
    combat_intent_count_ = 0;
    return combat_.advance_one_tick(*this) == tgd::gameplay::CombatError::none;
}

bool F1GrayboxLayer::retryEncounter() noexcept {
    const auto completed_tick = session_.current_snapshot().tick;
    const auto actors = combat_.actors();
    const auto player = std::find_if(
        actors.begin(),
        actors.end(),
        [this](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == definition_->player.actor;
        }
    );
    if (!player_defeated_ || player == actors.end() || player->active ||
        combat_.current_tick() != completed_tick ||
        encounter_.current_tick() != completed_tick) {
        return false;
    }
    pending_encounter_activation_beat_ = 0;
    pending_encounter_activation_objective_ = 0;
    const tgd::contracts::SafePointRetryCommand command{
        completed_tick,
        definition_->player.actor,
        retry_command_sequence_,
    };
    clearHeldInput(tgd::contracts::InputClearReason::safe_point_retry, false);
    if (encounter_.retry_from_initial(command) !=
            tgd::gameplay::EncounterDirectorError::none ||
        session_.retry_from_safe_point(command) != tgd::gameplay::VerticalSliceError::none ||
        combat_.retry_from_initial(command, *this) != tgd::gameplay::CombatError::none) {
        return false;
    }
    ++retry_command_sequence_;
    if (!restoreEncounterForBeat(session_.current_snapshot().beat_id.key)) {
        return false;
    }
    ++retry_count_;
    player_action_ticks_ = 0;
    player_hit_ticks_ = 0;
    attack_fx_ticks_ = 0;
    hostile_flash_ticks_.fill(0);
    incoming_attack_tick_ = 0;
    incoming_attack_source_ = 0;
    if (combat_event_label_ != nullptr) {
        combat_event_label_->setString("ENCOUNTER RETRIED / SAFE POINT RESTORED");
    }
    return true;
}

bool F1GrayboxLayer::restoreEncounterForBeat(
    tgd::contracts::StableContentKey beat
) noexcept {
    if (!activateEncounterForBeat(beat, 0)) {
        return false;
    }
    for (std::size_t index = 0;
         index < definition_->quest_encounter_activations.size();
         ++index) {
        const auto& activation = definition_->quest_encounter_activations[index];
        if (activation.beat_id.key != beat || activation.trigger_objective_id.key == 0 ||
            session_.quest_runtime().objective_state(activation.trigger_objective_id.key) !=
                tgd::gameplay::QuestObjectiveState::completed) {
            continue;
        }
        const bool boundary_already_seen = std::any_of(
            definition_->quest_encounter_activations.begin(),
            definition_->quest_encounter_activations.begin() +
                static_cast<std::ptrdiff_t>(index),
            [&activation](
                const tgd::contracts::QuestEncounterActivationDefinition& candidate
            ) {
                return candidate.beat_id.key == activation.beat_id.key &&
                       candidate.trigger_objective_id.key ==
                           activation.trigger_objective_id.key;
            }
        );
        if (boundary_already_seen) {
            continue;
        }
        if (!activateEncounterForBeat(beat, activation.trigger_objective_id.key)) {
            return false;
        }
    }
    return true;
}

bool F1GrayboxLayer::applyPendingEncounterActivation() noexcept {
    if (pending_encounter_activation_beat_ == 0) {
        return true;
    }
    const auto beat = pending_encounter_activation_beat_;
    const auto trigger_objective = pending_encounter_activation_objective_;
    pending_encounter_activation_beat_ = 0;
    pending_encounter_activation_objective_ = 0;
    if (activateEncounterForBeat(beat, trigger_objective)) {
        return true;
    }
    if (combat_event_label_ != nullptr) {
        combat_event_label_->setString(
            "BEAT ENCOUNTER ACTIVATION REJECTED / CONTRACT DRIFT"
        );
    }
    return false;
}

bool F1GrayboxLayer::activateEncounterForBeat(
    tgd::contracts::StableContentKey beat,
    tgd::contracts::StableContentKey trigger_objective
) noexcept {
    const auto match = session_.encounter_activation(beat, trigger_objective);
    if (match.ambiguous || (match.boundary_defined && match.activation == nullptr)) {
        return false;
    }
    const auto* activation = match.activation;
    if (activation == nullptr) {
        if (trigger_objective != 0) {
            return false;
        }
        pending_boss_stance_ = 0;
        return true;
    }
    if (activation->encounter_id.key != combat_definition_->id.key ||
        combat_.current_tick() != encounter_.current_tick()) {
        return false;
    }
    const tgd::contracts::EncounterActivationCommand command{
        combat_.current_tick(),
        definition_->player.actor,
        retry_command_sequence_,
        activation->mode,
    };
    clearHeldInput(tgd::contracts::InputClearReason::safe_point_retry, false);
    if (encounter_.activate_group(command, activation->actor_placements, combat_.actors()) !=
            tgd::gameplay::EncounterDirectorError::none ||
        combat_.activate_group(command, activation->actor_placements, *this) !=
            tgd::gameplay::CombatError::none) {
        return false;
    }
    ++retry_command_sequence_;
    if (!definition_->quest_boss_phases.empty() &&
        std::any_of(
            activation->actor_placements.begin(),
            activation->actor_placements.end(),
            [this](const tgd::contracts::EncounterActorPlacementDefinition& placement) {
                return placement.actor == definition_->quest_boss_phases.front().actor;
            }
        )) {
        syncBossStanceForQuest();
    } else {
        pending_boss_stance_ = 0;
    }
    player_action_ticks_ = 0;
    player_hit_ticks_ = 0;
    attack_fx_ticks_ = 0;
    hostile_flash_ticks_.fill(0);
    incoming_attack_tick_ = 0;
    incoming_attack_source_ = 0;
    if (combat_event_label_ != nullptr) {
        if (activation->required_selection_id.key == tgd::contracts::stable_content_key(
                                                         "f1_choice_rib_spring_calibration"
                                                     )) {
            combat_event_label_->setString(
                "SPRING RIB / UMBRELLA DOLL REINFORCEMENT"
            );
        } else if (activation->required_selection_id.key ==
                   tgd::contracts::stable_content_key(
                       "f1_choice_rib_winter_calibration"
                   )) {
            combat_event_label_->setString(
                "WINTER RIB / PAPER EGRET REINFORCEMENT"
            );
        } else if (trigger_objective == tgd::contracts::stable_content_key(
                                     "f1_objective_raise_paper_egret_lure"
                                 )) {
            combat_event_label_->setString(
                "LANE WAVE 2 / PAPER EGRET DEPLOYED"
            );
        } else if (trigger_objective == tgd::contracts::stable_content_key(
                                     "f1_objective_eavesguard_counter"
                                 )) {
            combat_event_label_->setString(
                "TRAINING WAVE 2 / PAPER EGRET RIG RELEASED"
            );
        } else if (beat == tgd::contracts::stable_content_key(
                               "f1_beat_shen_yan_training"
                           )) {
            combat_event_label_->setString(
                "TRAINING WAVE 1 / UMBRELLA RIG RELEASED"
            );
        } else if (beat == tgd::contracts::stable_content_key(
                                      "f1_beat_umbrella_lane_first_encounter"
                                  )) {
            combat_event_label_->setString(
                "LANE WAVE 1 / LEAKING DOLLS DEPLOYED"
            );
        } else if (beat == tgd::contracts::stable_content_key(
                                      "f1_beat_canopy_return_encounter"
                                  )) {
            combat_event_label_->setString("CALIBRATION RETURN GROUP DEPLOYED");
        } else if (beat == tgd::contracts::stable_content_key(
                                      "f1_beat_four_seasons_wraith"
                                  )) {
            combat_event_label_->setString("FOUR-SEASONS WRAITH AWAKENED");
        }
    }
    return true;
}

void F1GrayboxLayer::refreshCombatHud() noexcept {
    if (combat_resources_label_ == nullptr) {
        return;
    }
    const auto actors = combat_.actors();
    const auto player = std::find_if(
        actors.begin(),
        actors.end(),
        [this](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == definition_->player.actor;
        }
    );
    if (player == actors.end()) {
        combat_resources_label_->setString("COMBAT SNAPSHOT UNAVAILABLE");
        return;
    }
    const auto active_hostiles = static_cast<std::size_t>(std::count_if(
        actors.begin(),
        actors.end(),
        [](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.faction == tgd::contracts::CombatFaction::hostile && actor.active;
        }
    ));
    const auto stance = player->stance == tgd::contracts::stable_content_key("stance_flower_turn")
                            ? "FLOWER TURN"
                            : "EAVESGUARD";
    std::string text = "HP " + std::to_string(player->resources.health) + "/" +
                       std::to_string(player->resources.health_max) + " | ST " +
                       std::to_string(player->resources.stamina) + "/" +
                       std::to_string(player->resources.stamina_max) + " | POISE " +
                       std::to_string(player->resources.poise) + "/" +
                       std::to_string(player->resources.poise_max) + " | STANCE " + stance +
                       " | HOSTILES " + std::to_string(active_hostiles) +
                       (player->active ? "" : " | DOWN: PRESS R");
    if (!definition_->quest_boss_phases.empty()) {
        const auto boss_actor = definition_->quest_boss_phases.front().actor;
        const auto boss = std::find_if(
            actors.begin(),
            actors.end(),
            [boss_actor](const tgd::contracts::CombatActorSnapshot& actor) {
                return actor.actor == boss_actor;
            }
        );
        if (boss != actors.end() &&
            (boss->active || session_.current_snapshot().beat_index == 5)) {
            text += " | WRAITH " + std::to_string(boss->resources.health) + "/" +
                    std::to_string(boss->resources.health_max) + " " +
                    std::string{bossPhaseName(boss->stance)};
        }
    }
    combat_resources_label_->setString(text);
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

bool F1GrayboxLayer::updateCombatKey(
    ax::EventKeyboard::KeyCode key,
    bool pressed
) noexcept {
    const auto index = combatKeyIndex(key);
    if (index < 0) {
        return false;
    }
    const auto key_index = static_cast<std::size_t>(index);
    if (combat_keys_[key_index] == pressed) {
        return true;
    }
    if (key_index >= 5) {
        const bool guard_before = combat_keys_[5] || combat_keys_[6];
        combat_keys_[key_index] = pressed;
        const bool guard_after = combat_keys_[5] || combat_keys_[6];
        if (guard_before != guard_after) {
            queueCombatIntent(
                guard_after ? tgd::contracts::CombatCommandType::guard_started
                            : tgd::contracts::CombatCommandType::guard_ended
            );
        }
        return true;
    }

    combat_keys_[key_index] = pressed;
    if (!pressed) {
        return true;
    }
    switch (key_index) {
        case 0:
            queueCombatIntent(tgd::contracts::CombatCommandType::light_attack);
            break;
        case 1:
            queueCombatIntent(tgd::contracts::CombatCommandType::heavy_attack);
            break;
        case 2:
            queueCombatIntent(tgd::contracts::CombatCommandType::evade);
            break;
        case 3:
            queueCombatIntent(
                tgd::contracts::CombatCommandType::switch_stance,
                tgd::contracts::stable_content_key("stance_eavesguard")
            );
            break;
        case 4:
            queueCombatIntent(
                tgd::contracts::CombatCommandType::switch_stance,
                tgd::contracts::stable_content_key("stance_flower_turn")
            );
            break;
        default:
            break;
    }
    return true;
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

void F1GrayboxLayer::updateInteractKey(bool pressed) noexcept {
    if (interact_pressed_ == pressed) {
        return;
    }
    interact_pressed_ = pressed;
    const tgd::contracts::ScalarActionSample sample{
        ++platform_sequence_,
        tgd::contracts::action_id("interact"),
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

void F1GrayboxLayer::queueCombatIntent(
    tgd::contracts::CombatCommandType type,
    tgd::contracts::StableContentKey stance
) noexcept {
    if (combat_.lifecycle() != tgd::gameplay::CombatLifecycle::running ||
        combat_intent_count_ >= combat_intent_capacity) {
        return;
    }
    combat_intents_[combat_intent_count_++] = {type, stance};
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

int F1GrayboxLayer::combatKeyIndex(ax::EventKeyboard::KeyCode key) const noexcept {
    using KeyCode = ax::EventKeyboard::KeyCode;
    switch (key) {
        case KeyCode::KEY_J:
            return 0;
        case KeyCode::KEY_K:
            return 1;
        case KeyCode::KEY_C:
            return 2;
        case KeyCode::KEY_1:
            return 3;
        case KeyCode::KEY_2:
            return 4;
        case KeyCode::KEY_LEFT_SHIFT:
            return 5;
        case KeyCode::KEY_RIGHT_SHIFT:
            return 6;
        default:
            return -1;
    }
}

int F1GrayboxLayer::nativeChoiceKeyIndex(
    ax::EventKeyboard::KeyCode key
) const noexcept {
    using KeyCode = ax::EventKeyboard::KeyCode;
    switch (key) {
        case KeyCode::KEY_1:
            return 0;
        case KeyCode::KEY_2:
            return 1;
        case KeyCode::KEY_3:
            return 2;
        case KeyCode::KEY_4:
            return 3;
        case KeyCode::KEY_5:
            return 4;
        case KeyCode::KEY_6:
            return 5;
        case KeyCode::KEY_7:
            return 6;
        case KeyCode::KEY_8:
            return 7;
        default:
            return -1;
    }
}

tgd::contracts::StableActorKey F1GrayboxLayer::nearestActiveHostile() const noexcept {
    const auto actors = combat_.actors();
    const auto player = std::find_if(
        actors.begin(),
        actors.end(),
        [this](const tgd::contracts::CombatActorSnapshot& actor) {
            return actor.actor == definition_->player.actor;
        }
    );
    if (player == actors.end()) {
        return 0;
    }

    auto nearest_distance = std::numeric_limits<std::int64_t>::max();
    tgd::contracts::StableActorKey nearest = 0;
    for (const auto& actor : actors) {
        if (actor.faction != tgd::contracts::CombatFaction::hostile || !actor.active ||
            actor.pose.floor_layer != player->pose.floor_layer) {
            continue;
        }
        const auto delta_x = static_cast<std::int64_t>(actor.pose.x) - player->pose.x;
        const auto delta_y = static_cast<std::int64_t>(actor.pose.y) - player->pose.y;
        const auto distance = delta_x * delta_x + delta_y * delta_y;
        if (distance < nearest_distance || (distance == nearest_distance && actor.actor < nearest)) {
            nearest_distance = distance;
            nearest = actor.actor;
        }
    }
    return nearest;
}

int F1GrayboxLayer::depthOrder(float screen_y) const noexcept {
    return 10'000 - static_cast<int>(screen_y * 10.0F);
}

ax::Scene* createF1GrayboxScene(
    F1GrayboxLayer** layer_out,
    IF1RewardClaimSink* reward_claim_sink
) {
    auto* scene = ax::Scene::create();
    auto* layer = F1GrayboxLayer::create();
    if (scene == nullptr || layer == nullptr) {
        return nullptr;
    }
    layer->setRewardClaimSink(reward_claim_sink);
    scene->addChild(layer);
    if (layer_out != nullptr) {
        *layer_out = layer;
    }
    return scene;
}
