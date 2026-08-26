#include <tgd/contracts/craft_definition.hpp>
#include <tgd/gameplay/craft_session.hpp>

#include <array>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

using namespace tgd::contracts;
using namespace tgd::gameplay;

[[noreturn]] void fail(std::string_view message) {
    std::cerr << "craft session failure: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

void expect(bool condition, std::string_view message) {
    if (!condition) fail(message);
}

constexpr auto process_id = content_id("craft.process.demo");
constexpr auto material_pass = content_id("craft.material.flexible");
constexpr auto material_rework = content_id("craft.material.stiff");
constexpr auto workstation_id = content_id("craft.workstation.demo");
constexpr auto need_id = content_id("craft.need.canopy_tension");
constexpr auto output_id = content_id("craft.output.tuned_canopy");
constexpr auto step_align = content_id("craft.step.align");
constexpr auto step_paste = content_id("craft.step.paste");
constexpr auto step_trial = content_id("craft.step.trial");
constexpr auto step_rework = content_id("craft.step.rework");

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
        {process_id, workstation_id, need_id, output_id, step_trial},
    }};
    std::array<CraftMaterialChoiceDefinition, 2> choices{{
        {process_id, material_pass, CraftMaterialOutcome::passes_trial},
        {process_id, material_rework, CraftMaterialOutcome::requires_rework},
    }};
    // Deliberately not authored in execution order.
    std::array<CraftStepDefinition, 4> steps{{
        {step_trial, process_id, step_paste, content_id("craft.action.trial"),
         CraftStepKind::trial},
        {step_align, process_id, {}, content_id("craft.action.align"),
         CraftStepKind::operation},
        {step_rework, process_id, step_trial, content_id("craft.action.retension"),
         CraftStepKind::rework},
        {step_paste, process_id, step_align, content_id("craft.action.paste"),
         CraftStepKind::operation},
    }};
    CraftDefinition definition{materials, workstations, processes, choices, steps};
};

[[nodiscard]] CraftSessionSnapshot complete_rework_route() {
    Fixture fixture;
    CraftSession session;
    expect(
        session.initialize(fixture.definition, process_id.key) ==
            CraftSessionBuildError::none,
        "valid definition initializes"
    );
    const auto initial = session.snapshot();
    expect(initial.stage == CraftSessionStage::awaiting_material, "need is visible");
    expect(initial.operation_count == 2, "two operations are authored");

    const auto unknown = session.select_material(content_id("material.unknown").key);
    expect(
        unknown.disposition == CraftActionDisposition::unknown_target &&
            unknown.snapshot == initial,
        "unknown material produces zero drift"
    );
    expect(
        session.select_material(material_rework.key).disposition ==
            CraftActionDisposition::applied,
        "rework material can be selected"
    );
    const auto before_wrong_order = session.snapshot();
    const auto wrong_order = session.perform_operation(step_paste.key);
    expect(
        wrong_order.disposition == CraftActionDisposition::wrong_order &&
            wrong_order.snapshot == before_wrong_order,
        "wrong operation order produces zero drift"
    );
    expect(
        session.perform_operation(step_align.key).disposition ==
            CraftActionDisposition::applied,
        "first operation applies"
    );
    expect(
        session.perform_operation(step_paste.key).disposition ==
            CraftActionDisposition::applied,
        "second operation applies"
    );
    expect(
        session.snapshot().stage == CraftSessionStage::trial_ready,
        "ordered operations unlock trial"
    );
    expect(
        session.run_trial().disposition == CraftActionDisposition::applied,
        "first trial runs"
    );
    const auto failed_trial = session.snapshot();
    expect(
        failed_trial.stage == CraftSessionStage::rework_required &&
            failed_trial.trial_count == 1 &&
            failed_trial.mistake_count == 1 &&
            !failed_trial.completed,
        "authored mismatch yields recoverable feedback"
    );
    const auto repeated_trial = session.run_trial();
    expect(
        repeated_trial.disposition == CraftActionDisposition::wrong_stage &&
            repeated_trial.snapshot == failed_trial,
        "trial cannot bypass required rework"
    );
    expect(
        session.perform_rework().disposition == CraftActionDisposition::applied,
        "rework operation applies"
    );
    expect(
        session.run_trial().disposition == CraftActionDisposition::applied,
        "retrial applies"
    );
    const auto completed = session.snapshot();
    expect(
        completed.completed &&
            completed.stage == CraftSessionStage::completed &&
            completed.trial_count == 2 &&
            completed.mistake_count == 1 &&
            completed.rework_count == 1,
        "rework route completes with explicit counters"
    );

    expect(
        session.restart() == CraftSessionBuildError::none,
        "local restart rebuilds the process"
    );
    const auto restarted = session.snapshot();
    expect(
        restarted.stage == CraftSessionStage::awaiting_material &&
            restarted.selected_material == 0 &&
            restarted.completed_operation_count == 0 &&
            restarted.trial_count == 0 &&
            restarted.mistake_count == 0 &&
            restarted.rework_count == 0 &&
            !restarted.completed,
        "restart clears authored run state"
    );
    return completed;
}

void test_pass_route() {
    Fixture fixture;
    CraftSession session;
    expect(
        session.initialize(fixture.definition, process_id.key) ==
            CraftSessionBuildError::none,
        "pass route initializes"
    );
    expect(
        session.select_material(material_pass.key).disposition ==
            CraftActionDisposition::applied,
        "pass material selects"
    );
    expect(
        session.perform_operation(step_align.key).disposition ==
            CraftActionDisposition::applied &&
            session.perform_operation(step_paste.key).disposition ==
                CraftActionDisposition::applied &&
            session.run_trial().disposition == CraftActionDisposition::applied,
        "pass route actions apply"
    );
    const auto completed = session.snapshot();
    expect(
        completed.completed &&
            completed.trial_count == 1 &&
            completed.mistake_count == 0 &&
            completed.rework_count == 0,
        "pass material completes without synthetic failure"
    );
}

void test_invalid_definition() {
    Fixture fixture;
    fixture.steps[2].predecessor_step_id = step_align;
    CraftSession session;
    expect(
        session.initialize(fixture.definition, process_id.key) ==
            CraftSessionBuildError::invalid_definition,
        "rework that bypasses trial is rejected"
    );
    expect(
        !session.snapshot().initialized,
        "failed initialization exposes no partial session"
    );
}

}  // namespace

int main() {
    const auto first = complete_rework_route();
    const auto second = complete_rework_route();
    expect(
        first.checksum == second.checksum,
        "fixed route produces a deterministic checksum"
    );
    test_pass_route();
    test_invalid_definition();
    return 0;
}
