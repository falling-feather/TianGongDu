#pragma once

#include <tgd/contracts/profile_progress.hpp>
#include <tgd/contracts/save_envelope.hpp>

#include <cstddef>
#include <cstdint>

namespace tgd::runtime {

enum class ProfileProgressCoordinatorState : std::uint8_t {
    uninitialized,
    ready,
    pending,
};

enum class ProfileProgressCoordinatorError : std::uint8_t {
    none,
    invalid_config,
    invalid_state,
    invalid_snapshot,
    invalid_progress,
    allocation_failed,
    revision_overflow,
};

enum class ProfileProgressSaveKind : std::uint8_t {
    checkpoint,
    reward_claim,
};

enum class ProfileProgressPrepareDisposition : std::uint8_t {
    none,
    prepared,
    already_committed,
    already_pending,
};

struct ProfileProgressPrepareResult final {
    ProfileProgressCoordinatorError error{ProfileProgressCoordinatorError::none};
    ProfileProgressPrepareDisposition disposition{ProfileProgressPrepareDisposition::none};
};

class ProfileProgressCoordinator final {
  public:
    [[nodiscard]] ProfileProgressCoordinatorError initialize(
        contracts::StableId128 profile_id,
        contracts::StableId128 package_set_id
    ) noexcept;
    [[nodiscard]] ProfileProgressCoordinatorError restore(
        const contracts::SaveEnvelopeV1& snapshot
    ) noexcept;
    [[nodiscard]] ProfileProgressPrepareResult prepare_checkpoint(
        contracts::StableId128 snapshot_id,
        contracts::CheckpointKind checkpoint_kind
    ) noexcept;
    [[nodiscard]] ProfileProgressPrepareResult prepare_reward_claim(
        contracts::StableContentKey source_id,
        contracts::StableContentKey reward_id,
        contracts::StableContentKey reward_dedup_key
    ) noexcept;
    [[nodiscard]] ProfileProgressCoordinatorError accept_commit(
        contracts::StableId128 snapshot_id
    ) noexcept;
    [[nodiscard]] ProfileProgressCoordinatorError discard_pending() noexcept;

    [[nodiscard]] ProfileProgressCoordinatorState state() const noexcept;
    [[nodiscard]] bool has_pending() const noexcept;
    [[nodiscard]] ProfileProgressSaveKind pending_kind() const noexcept;
    [[nodiscard]] const contracts::SaveEnvelopeV1& pending_snapshot() const noexcept;
    [[nodiscard]] const contracts::ProfileProgressV1& committed_progress() const noexcept;
    [[nodiscard]] contracts::StableId128 committed_snapshot_id() const noexcept;
    [[nodiscard]] bool has_reward_claim(
        contracts::StableContentKey reward_dedup_key
    ) const noexcept;
    [[nodiscard]] std::size_t committed_operation_count() const noexcept;

  private:
    [[nodiscard]] ProfileProgressPrepareResult build_pending(
        contracts::ProfileProgressV1&& progress,
        contracts::StableId128 snapshot_id,
        contracts::CheckpointKind checkpoint_kind,
        ProfileProgressSaveKind kind
    ) noexcept;
    void clear_pending() noexcept;

    contracts::StableId128 profile_id_{};
    contracts::StableId128 package_set_id_{};
    contracts::StableId128 committed_snapshot_id_{};
    contracts::ProfileProgressV1 committed_progress_{};
    contracts::ProfileProgressV1 pending_progress_{};
    contracts::SaveEnvelopeV1 pending_snapshot_{};
    ProfileProgressCoordinatorState state_{ProfileProgressCoordinatorState::uninitialized};
    ProfileProgressSaveKind pending_kind_{ProfileProgressSaveKind::checkpoint};
};

}  // namespace tgd::runtime
