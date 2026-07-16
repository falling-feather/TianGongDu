#include <tgd/gameplay/playtime_audit.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace tgd::gameplay {
namespace {

constexpr std::uint64_t fnv_offset = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime = 1'099'511'628'211ULL;

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= fnv_prime;
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto bits = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & static_cast<Unsigned>(0xffU)));
        bits >>= 8U;
    }
}

[[nodiscard]] bool valid_activity(PlaytimeActivityKind kind) noexcept {
    switch (kind) {
        case PlaytimeActivityKind::movement:
        case PlaytimeActivityKind::jump:
        case PlaytimeActivityKind::combat:
        case PlaytimeActivityKind::interaction:
        case PlaytimeActivityKind::authored_progress:
            return true;
    }
    return false;
}

}  // namespace

PlaytimeAuditError DeterministicPlaytimeAudit::initialize(
    std::span<const contracts::VerticalSliceBeatDefinition> beats,
    std::uint16_t playable_target_minutes,
    std::uint16_t activity_grace_ticks
) noexcept {
    if (lifecycle_ != PlaytimeAuditLifecycle::uninitialized) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    if (beats.empty() || beats.size() > max_beats || playable_target_minutes == 0 ||
        activity_grace_ticks == 0 || activity_grace_ticks > 600) {
        return PlaytimeAuditError::invalid_definition;
    }

    std::uint64_t authored_target_ticks = 0;
    for (std::size_t index = 0; index < beats.size(); ++index) {
        const auto& beat = beats[index];
        if (beat.id.key == 0 || beat.target_minutes == 0) {
            return PlaytimeAuditError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (beats[prior].id.key == beat.id.key) {
                return PlaytimeAuditError::invalid_definition;
            }
        }
        const auto target_ticks =
            static_cast<std::uint64_t>(beat.target_minutes) * ticks_per_minute;
        if (authored_target_ticks > std::numeric_limits<std::uint64_t>::max() - target_ticks) {
            return PlaytimeAuditError::counter_overflow;
        }
        beat_target_ticks_[index] = target_ticks;
        authored_target_ticks += target_ticks;
    }

    playable_target_ticks_ =
        static_cast<std::uint64_t>(playable_target_minutes) * ticks_per_minute;
    if (authored_target_ticks != playable_target_ticks_) {
        return PlaytimeAuditError::invalid_definition;
    }
    beat_count_ = static_cast<std::uint16_t>(beats.size());
    activity_grace_ticks_ = activity_grace_ticks;
    lifecycle_ = PlaytimeAuditLifecycle::ready;
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditError DeterministicPlaytimeAudit::start() noexcept {
    if (lifecycle_ != PlaytimeAuditLifecycle::ready) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    lifecycle_ = PlaytimeAuditLifecycle::running;
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditError DeterministicPlaytimeAudit::pause() noexcept {
    if (lifecycle_ != PlaytimeAuditLifecycle::running) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    lifecycle_ = PlaytimeAuditLifecycle::paused;
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditError DeterministicPlaytimeAudit::resume() noexcept {
    if (lifecycle_ != PlaytimeAuditLifecycle::paused) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    lifecycle_ = PlaytimeAuditLifecycle::running;
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditError DeterministicPlaytimeAudit::destroy() noexcept {
    if (lifecycle_ == PlaytimeAuditLifecycle::uninitialized ||
        lifecycle_ == PlaytimeAuditLifecycle::destroyed) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    lifecycle_ = PlaytimeAuditLifecycle::destroyed;
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditError DeterministicPlaytimeAudit::note_activity(
    PlaytimeActivityKind kind
) noexcept {
    if (lifecycle_ != PlaytimeAuditLifecycle::running &&
        lifecycle_ != PlaytimeAuditLifecycle::paused) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    if (!valid_activity(kind)) {
        return PlaytimeAuditError::invalid_activity;
    }
    activity_grace_remaining_ticks_ = activity_grace_ticks_;
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditError DeterministicPlaytimeAudit::advance(std::uint32_t executed_ticks) noexcept {
    if (lifecycle_ != PlaytimeAuditLifecycle::running) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    const auto current_total = committed_eligible_ticks_ + committed_idle_ticks_ +
                               attempt_eligible_ticks_ + attempt_idle_ticks_ +
                               failure_retry_ticks_;
    if (current_total > std::numeric_limits<std::uint64_t>::max() - executed_ticks) {
        return PlaytimeAuditError::counter_overflow;
    }
    const auto eligible = std::min<std::uint64_t>(
        executed_ticks,
        activity_grace_remaining_ticks_
    );
    attempt_eligible_ticks_ += eligible;
    attempt_idle_ticks_ += static_cast<std::uint64_t>(executed_ticks) - eligible;
    activity_grace_remaining_ticks_ = static_cast<std::uint16_t>(
        activity_grace_remaining_ticks_ - eligible
    );
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditError DeterministicPlaytimeAudit::discard_current_attempt_for_retry() noexcept {
    if (lifecycle_ != PlaytimeAuditLifecycle::running &&
        lifecycle_ != PlaytimeAuditLifecycle::paused) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    const auto attempt_ticks = attempt_eligible_ticks_ + attempt_idle_ticks_;
    if (failure_retry_ticks_ > std::numeric_limits<std::uint64_t>::max() - attempt_ticks) {
        return PlaytimeAuditError::counter_overflow;
    }
    failure_retry_ticks_ += attempt_ticks;
    attempt_eligible_ticks_ = 0;
    attempt_idle_ticks_ = 0;
    activity_grace_remaining_ticks_ = 0;
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditError DeterministicPlaytimeAudit::commit_current_beat(
    std::uint16_t next_beat_index
) noexcept {
    if (lifecycle_ != PlaytimeAuditLifecycle::running) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    if (next_beat_index != static_cast<std::uint16_t>(beat_index_ + 1U) ||
        next_beat_index >= beat_count_) {
        return PlaytimeAuditError::beat_order_violation;
    }
    commit_attempt();
    beat_index_ = next_beat_index;
    activity_grace_remaining_ticks_ = 0;
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditError DeterministicPlaytimeAudit::resolve() noexcept {
    if (lifecycle_ != PlaytimeAuditLifecycle::running) {
        return PlaytimeAuditError::invalid_lifecycle;
    }
    if (beat_index_ + 1U != beat_count_) {
        return PlaytimeAuditError::beat_order_violation;
    }
    commit_attempt();
    activity_grace_remaining_ticks_ = 0;
    lifecycle_ = PlaytimeAuditLifecycle::resolved;
    refresh_snapshot();
    return PlaytimeAuditError::none;
}

PlaytimeAuditLifecycle DeterministicPlaytimeAudit::lifecycle() const noexcept {
    return lifecycle_;
}

const PlaytimeAuditSnapshot& DeterministicPlaytimeAudit::snapshot() const noexcept {
    return snapshot_;
}

void DeterministicPlaytimeAudit::commit_attempt() noexcept {
    committed_beat_eligible_ticks_[beat_index_] = attempt_eligible_ticks_;
    if (attempt_eligible_ticks_ >= beat_target_ticks_[beat_index_]) {
        ++committed_beat_targets_met_;
    }
    committed_eligible_ticks_ += attempt_eligible_ticks_;
    committed_idle_ticks_ += attempt_idle_ticks_;
    attempt_eligible_ticks_ = 0;
    attempt_idle_ticks_ = 0;
}

void DeterministicPlaytimeAudit::refresh_snapshot() noexcept {
    const bool resolved = lifecycle_ == PlaytimeAuditLifecycle::resolved;
    const auto current_eligible = resolved
                                      ? committed_beat_eligible_ticks_[beat_index_]
                                      : attempt_eligible_ticks_;
    const bool current_target_met = current_eligible >= beat_target_ticks_[beat_index_];
    const auto eligible_ticks = committed_eligible_ticks_ + attempt_eligible_ticks_;
    const auto idle_ticks = committed_idle_ticks_ + attempt_idle_ticks_;
    snapshot_.total_ticks = eligible_ticks + idle_ticks + failure_retry_ticks_;
    snapshot_.eligible_ticks = eligible_ticks;
    snapshot_.idle_ticks = idle_ticks;
    snapshot_.failure_retry_ticks = failure_retry_ticks_;
    snapshot_.current_attempt_ticks = attempt_eligible_ticks_ + attempt_idle_ticks_;
    snapshot_.current_beat_eligible_ticks = current_eligible;
    snapshot_.current_beat_target_ticks = beat_target_ticks_[beat_index_];
    snapshot_.playable_target_ticks = playable_target_ticks_;
    snapshot_.beat_index = beat_index_;
    snapshot_.beat_count = beat_count_;
    snapshot_.beat_targets_met = static_cast<std::uint16_t>(
        committed_beat_targets_met_ + (!resolved && current_target_met ? 1U : 0U)
    );
    snapshot_.activity_grace_remaining_ticks = activity_grace_remaining_ticks_;
    snapshot_.current_beat_target_met = current_target_met;
    snapshot_.playable_target_met = resolved && eligible_ticks >= playable_target_ticks_ &&
                                     committed_beat_targets_met_ == beat_count_;
    snapshot_.resolved = resolved;
    update_checksum();
}

void DeterministicPlaytimeAudit::update_checksum() noexcept {
    auto hash = fnv_offset;
    hash_integer(hash, snapshot_.total_ticks);
    hash_integer(hash, snapshot_.eligible_ticks);
    hash_integer(hash, snapshot_.idle_ticks);
    hash_integer(hash, snapshot_.failure_retry_ticks);
    hash_integer(hash, snapshot_.current_attempt_ticks);
    hash_integer(hash, snapshot_.current_beat_eligible_ticks);
    hash_integer(hash, snapshot_.current_beat_target_ticks);
    hash_integer(hash, snapshot_.playable_target_ticks);
    hash_integer(hash, snapshot_.beat_index);
    hash_integer(hash, snapshot_.beat_count);
    hash_integer(hash, snapshot_.beat_targets_met);
    hash_integer(hash, snapshot_.activity_grace_remaining_ticks);
    hash_byte(hash, snapshot_.current_beat_target_met ? 1U : 0U);
    hash_byte(hash, snapshot_.playable_target_met ? 1U : 0U);
    hash_byte(hash, snapshot_.resolved ? 1U : 0U);
    hash_byte(hash, static_cast<std::uint8_t>(lifecycle_));
    snapshot_.checksum = hash;
}

}  // namespace tgd::gameplay
