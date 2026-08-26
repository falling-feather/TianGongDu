#include <tgd/contracts/content_definition.hpp>
#include <tgd/gameplay/playtime_audit.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>

namespace {

using tgd::gameplay::DeterministicPlaytimeAudit;
using tgd::gameplay::PlaytimeActivityKind;
using tgd::gameplay::PlaytimeAuditError;
using tgd::gameplay::PlaytimeAuditLifecycle;

inline constexpr std::array<tgd::contracts::ContentId, 1> first_objectives{{
    tgd::contracts::content_id("test_objective_first"),
}};
inline constexpr std::array<tgd::contracts::ContentId, 1> second_objectives{{
    tgd::contracts::content_id("test_objective_second"),
}};
inline constexpr std::array<tgd::contracts::VerticalSliceBeatDefinition, 2> beats{{
    {
        tgd::contracts::content_id("test_beat_first"),
        tgd::contracts::VerticalSliceBeatKind::exploration,
        1,
        tgd::contracts::content_id("test_cell_first"),
        std::span<const tgd::contracts::ContentId>{first_objectives},
    },
    {
        tgd::contracts::content_id("test_beat_second"),
        tgd::contracts::VerticalSliceBeatKind::combat,
        1,
        tgd::contracts::content_id("test_cell_second"),
        std::span<const tgd::contracts::ContentId>{second_objectives},
    },
}};

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "playtime audit failure: " << message << '\n';
    }
    return condition;
}

bool initialize_and_start(DeterministicPlaytimeAudit& audit) {
    return audit.initialize(beats, 2, 180) == PlaytimeAuditError::none &&
           audit.start() == PlaytimeAuditError::none;
}

bool advance_eligible(
    DeterministicPlaytimeAudit& audit,
    std::uint64_t ticks,
    PlaytimeActivityKind kind = PlaytimeActivityKind::movement
) {
    while (ticks != 0) {
        const auto batch = static_cast<std::uint32_t>(std::min<std::uint64_t>(ticks, 180));
        if (audit.note_activity(kind) != PlaytimeAuditError::none ||
            audit.advance(batch) != PlaytimeAuditError::none) {
            return false;
        }
        ticks -= batch;
    }
    return true;
}

bool test_activity_grace_excludes_idle_ticks() {
    DeterministicPlaytimeAudit audit;
    bool ok = initialize_and_start(audit);
    ok &= audit.note_activity(PlaytimeActivityKind::movement) == PlaytimeAuditError::none;
    ok &= audit.advance(180) == PlaytimeAuditError::none;
    ok &= audit.advance(60) == PlaytimeAuditError::none;
    const auto& snapshot = audit.snapshot();
    ok &= expect(
        snapshot.total_ticks == 240 && snapshot.eligible_ticks == 180 &&
            snapshot.idle_ticks == 60 && snapshot.failure_retry_ticks == 0 &&
            snapshot.current_attempt_ticks == 240,
        "only the bounded activity window contributes eligible playtime"
    );
    ok &= expect(!snapshot.playable_target_met, "an unfinished audit never passes the target");
    ok &= expect(audit.pause() == PlaytimeAuditError::none, "audit pauses explicitly");
    ok &= expect(
        audit.note_activity(PlaytimeActivityKind::jump) == PlaytimeAuditError::none,
        "queued paused input may arm activity without accruing paused time"
    );
    ok &= expect(
        audit.advance(1) == PlaytimeAuditError::invalid_lifecycle,
        "paused time cannot enter the audit"
    );
    ok &= expect(audit.resume() == PlaytimeAuditError::none, "audit resumes explicitly");
    ok &= audit.advance(1) == PlaytimeAuditError::none;
    ok &= expect(
        audit.snapshot().eligible_ticks == 181 && audit.snapshot().idle_ticks == 60,
        "queued activity starts counting only after simulation resumes"
    );
    return ok;
}

bool test_retry_reclassifies_the_whole_attempt() {
    DeterministicPlaytimeAudit audit;
    bool ok = initialize_and_start(audit);
    ok &= audit.note_activity(PlaytimeActivityKind::combat) == PlaytimeAuditError::none;
    ok &= audit.advance(180) == PlaytimeAuditError::none;
    ok &= audit.advance(60) == PlaytimeAuditError::none;
    ok &= audit.discard_current_attempt_for_retry() == PlaytimeAuditError::none;
    const auto& discarded = audit.snapshot();
    ok &= expect(
        discarded.total_ticks == 240 && discarded.eligible_ticks == 0 &&
            discarded.idle_ticks == 0 && discarded.failure_retry_ticks == 240 &&
            discarded.current_attempt_ticks == 0,
        "failure moves active and idle attempt time into the excluded retry bucket"
    );
    ok &= advance_eligible(audit, 180, PlaytimeActivityKind::interaction);
    ok &= expect(
        audit.snapshot().eligible_ticks == 180 &&
            audit.snapshot().failure_retry_ticks == 240,
        "the next attempt starts clean without erasing retry evidence"
    );
    return ok;
}

bool test_each_beat_must_meet_its_own_budget() {
    DeterministicPlaytimeAudit audit;
    bool ok = initialize_and_start(audit);
    ok &= advance_eligible(audit, DeterministicPlaytimeAudit::ticks_per_minute);
    ok &= audit.commit_current_beat(1) == PlaytimeAuditError::none;
    ok &= advance_eligible(audit, DeterministicPlaytimeAudit::ticks_per_minute);
    ok &= audit.resolve() == PlaytimeAuditError::none;
    const auto& snapshot = audit.snapshot();
    ok &= expect(
        snapshot.resolved && snapshot.eligible_ticks == 2 *
                DeterministicPlaytimeAudit::ticks_per_minute &&
            snapshot.beat_targets_met == 2 && snapshot.playable_target_met,
        "a resolved route passes only after every authored beat reaches its target"
    );
    return ok;
}

bool test_padding_one_beat_cannot_hide_another_shortfall() {
    DeterministicPlaytimeAudit audit;
    bool ok = initialize_and_start(audit);
    ok &= advance_eligible(audit, 2 * DeterministicPlaytimeAudit::ticks_per_minute);
    ok &= audit.commit_current_beat(1) == PlaytimeAuditError::none;
    ok &= advance_eligible(audit, 180);
    ok &= audit.resolve() == PlaytimeAuditError::none;
    const auto& snapshot = audit.snapshot();
    ok &= expect(
        snapshot.eligible_ticks > snapshot.playable_target_ticks &&
            snapshot.beat_targets_met == 1 && !snapshot.playable_target_met,
        "surplus time in one beat cannot compensate for a thin later beat"
    );
    return ok;
}

bool test_same_activity_trace_has_the_same_checksum() {
    DeterministicPlaytimeAudit left;
    DeterministicPlaytimeAudit right;
    bool ok = initialize_and_start(left) && initialize_and_start(right);
    for (auto* audit : {&left, &right}) {
        ok &= advance_eligible(*audit, 360, PlaytimeActivityKind::jump);
        ok &= audit->advance(15) == PlaytimeAuditError::none;
        ok &= audit->discard_current_attempt_for_retry() == PlaytimeAuditError::none;
        ok &= advance_eligible(*audit, 180, PlaytimeActivityKind::combat);
    }
    ok &= expect(
        left.snapshot().checksum == right.snapshot().checksum,
        "the same classified activity trace produces the same checksum"
    );
    ok &= expect(
        left.commit_current_beat(0) == PlaytimeAuditError::beat_order_violation,
        "beat commits reject skips and rewinds"
    );
    ok &= expect(
        left.note_activity(static_cast<PlaytimeActivityKind>(255)) ==
            PlaytimeAuditError::invalid_activity,
        "unknown activity categories fail closed"
    );
    return ok;
}

}  // namespace

int main() {
    bool ok = true;
    ok &= test_activity_grace_excludes_idle_ticks();
    ok &= test_retry_reclassifies_the_whole_attempt();
    ok &= test_each_beat_must_meet_its_own_budget();
    ok &= test_padding_one_beat_cannot_hide_another_shortfall();
    ok &= test_same_activity_trace_has_the_same_checksum();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
