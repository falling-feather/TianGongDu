#include <tgd/contracts/craft_definition.hpp>
#include <tgd/contracts/workshop_definition.hpp>
#include <tgd/gameplay/workshop_session.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

using namespace tgd::contracts;
using namespace tgd::gameplay;

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "workshop session failure: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, std::string_view message) {
    if (!condition) fail(message);
}

constexpr auto process_id = content_id("craft.process.demo");
constexpr auto material_pass = content_id("craft.material.flexible");
constexpr auto material_rework = content_id("craft.material.stiff");
constexpr auto workstation_id = content_id("craft.workstation.demo");
constexpr auto step_align = content_id("craft.step.align");
constexpr auto step_paste = content_id("craft.step.paste");
constexpr auto step_trial = content_id("craft.step.trial");
constexpr auto step_rework = content_id("craft.step.rework");
constexpr auto workshop_id = content_id("workshop.demo");
constexpr auto order_id = content_id("workshop.order.demo");
constexpr auto shortcut_interaction_id =
    content_id("sandbox.interaction.workshop-shortcut");

struct Fixture final {
    std::array<CraftMaterialDefinition, 2> materials{{
        {material_pass},
        {material_rework},
    }};
    std::array<CraftWorkstationDefinition, 1> workstations{{
        {workstation_id, content_id("region.demo"), content_id("asset.interaction"),
         {0, 0, 0, 0}, 0},
    }};
    std::array<CraftProcessDefinition, 1> processes{{
        {process_id, workstation_id, content_id("craft.need.demo"),
         content_id("craft.output.demo"), step_trial},
    }};
    std::array<CraftMaterialChoiceDefinition, 2> choices{{
        {process_id, material_pass, CraftMaterialOutcome::passes_trial},
        {process_id, material_rework, CraftMaterialOutcome::requires_rework},
    }};
    std::array<CraftStepDefinition, 4> steps{{
        {step_trial, process_id, step_paste, content_id("craft.action.trial"),
         CraftStepKind::trial},
        {step_align, process_id, {}, content_id("craft.action.align"),
         CraftStepKind::operation},
        {step_rework, process_id, step_trial, content_id("craft.action.rework"),
         CraftStepKind::rework},
        {step_paste, process_id, step_align, content_id("craft.action.paste"),
         CraftStepKind::operation},
    }};
    CraftDefinition craft{materials, workstations, processes, choices, steps};
    std::array<WorkshopDefinitionRecord, 1> workshops{{
        {workshop_id, workstation_id, 100},
    }};
    std::array<WorkshopMaterialStockDefinition, 2> stocks{{
        {workshop_id, material_pass, 80, 1, 9'000, 0},
        {workshop_id, material_rework, 30, 2, 6'500, 1'900},
    }};
    std::array<WorkshopOrderDefinition, 1> orders{{
        {order_id, workshop_id, process_id, shortcut_interaction_id, 1, 8'000,
         25, WorkshopOrderConsequenceKind::operate_route_interaction},
    }};
    WorkshopDefinition workshop{workshops, stocks, orders};
};

void apply_operations(WorkshopSession& session) {
    expect(
        session.perform_operation(step_align.key).disposition ==
                WorkshopActionDisposition::applied &&
            session.perform_operation(step_paste.key).disposition ==
                WorkshopActionDisposition::applied,
        "ordered craft operations apply"
    );
}

[[nodiscard]] WorkshopSessionSnapshot complete_rework_route() {
    Fixture fixture;
    WorkshopSession session;
    expect(
        session.initialize(
            fixture.craft, fixture.workshop, workshop_id.key, order_id.key
        ) == WorkshopSessionBuildError::none,
        "valid workshop initializes"
    );
    const auto initial = session.snapshot();
    expect(
        initial.initialized && initial.funds == 100 &&
            initial.minimum_quality == 8'000 &&
            session.stock_remaining(material_pass.key) == 1 &&
            session.stock_remaining(material_rework.key) == 2,
        "initial funds, order threshold, and finite stocks are visible"
    );
    expect(
        session.initialize(
            fixture.craft, fixture.workshop, workshop_id.key, order_id.key
        ) == WorkshopSessionBuildError::invalid_definition &&
            session.snapshot() == initial,
        "duplicate initialization preserves the valid session"
    );

    const auto unknown =
        session.select_material(content_id("craft.material.unknown").key);
    expect(
        unknown.disposition == WorkshopActionDisposition::unknown_target &&
            unknown.snapshot == initial,
        "unknown purchase produces zero state drift"
    );
    expect(
        session.select_material(material_rework.key).disposition ==
            WorkshopActionDisposition::applied,
        "salvage material can be purchased"
    );
    const auto purchased = session.snapshot();
    expect(
        purchased.funds == 70 && purchased.spent_funds == 30 &&
            purchased.item_quality == 6'500 &&
            purchased.workstation_occupied &&
            session.stock_remaining(material_rework.key) == 1,
        "purchase atomically spends funds, consumes stock, and occupies station"
    );

    const auto wrong_order = session.perform_operation(step_paste.key);
    expect(
        wrong_order.disposition == WorkshopActionDisposition::wrong_order &&
            wrong_order.snapshot == purchased,
        "wrong operation order preserves economy and craft state"
    );
    apply_operations(session);
    expect(
        session.run_trial().disposition == WorkshopActionDisposition::applied &&
            session.snapshot().craft.stage ==
                CraftSessionStage::rework_required,
        "salvage choice exposes authored trial failure"
    );
    const auto premature = session.deliver_order();
    expect(
        premature.disposition == WorkshopActionDisposition::wrong_stage &&
            !premature.snapshot.order_fulfilled,
        "failed trial cannot be delivered"
    );
    expect(
        session.perform_rework().disposition ==
                WorkshopActionDisposition::applied &&
            session.snapshot().item_quality == 8'400 &&
            session.run_trial().disposition ==
                WorkshopActionDisposition::applied,
        "rework raises quality and unlocks a second trial"
    );
    expect(
        session.deliver_order().disposition ==
            WorkshopActionDisposition::applied,
        "qualifying output can be delivered"
    );
    const auto delivered = session.snapshot();
    expect(
        delivered.order_fulfilled && delivered.delivery_count == 1 &&
            delivered.funds == 95 && !delivered.workstation_occupied &&
            !delivered.output_ready &&
            delivered.unlocked_route == shortcut_interaction_id.key,
        "delivery rewards funds and exposes exactly one authored route consequence"
    );
    const auto duplicate = session.deliver_order();
    expect(
        duplicate.disposition ==
                WorkshopActionDisposition::already_fulfilled &&
            duplicate.snapshot == delivered,
        "duplicate delivery cannot duplicate reward or consequence"
    );

    expect(
        session.restart() == WorkshopSessionBuildError::none,
        "local retry rebuilds workshop state"
    );
    expect(
        session.snapshot().funds == 100 &&
            session.stock_remaining(material_pass.key) == 1 &&
            session.stock_remaining(material_rework.key) == 2 &&
            !session.snapshot().order_fulfilled,
        "retry restores authored funds, stock, station, and order"
    );
    return delivered;
}

void test_pass_route_and_insufficient_funds() {
    Fixture fixture;
    WorkshopSession session;
    expect(
        session.initialize(
            fixture.craft, fixture.workshop, workshop_id.key, order_id.key
        ) == WorkshopSessionBuildError::none,
        "pass route initializes"
    );
    expect(
        session.select_material(material_pass.key).disposition ==
            WorkshopActionDisposition::applied,
        "premium material purchase applies"
    );
    apply_operations(session);
    expect(
        session.run_trial().disposition == WorkshopActionDisposition::applied &&
            session.snapshot().craft.completed &&
            session.snapshot().item_quality == 9'000 &&
            session.deliver_order().disposition ==
                WorkshopActionDisposition::applied &&
            session.snapshot().funds == 45,
        "premium route passes once and settles cost plus reward"
    );

    fixture.workshops[0].initial_funds = 50;
    WorkshopSession constrained;
    expect(
        constrained.initialize(
            fixture.craft, fixture.workshop, workshop_id.key, order_id.key
        ) == WorkshopSessionBuildError::none,
        "constrained workshop initializes"
    );
    const auto before = constrained.snapshot();
    const auto rejected = constrained.select_material(material_pass.key);
    expect(
        rejected.disposition ==
                WorkshopActionDisposition::insufficient_funds &&
            rejected.snapshot == before &&
            constrained.stock_remaining(material_pass.key) == 1,
        "insufficient funds preserve stock and session checksum"
    );
}

}  // namespace

int main() {
    const auto first = complete_rework_route();
    const auto second = complete_rework_route();
    expect(
        first.checksum == second.checksum,
        "fixed workshop route produces a deterministic checksum"
    );
    test_pass_route_and_insufficient_funds();
    return 0;
}
