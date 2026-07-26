#pragma once

#include <tgd/contracts/craft_definition.hpp>
#include <tgd/contracts/sandbox_pack.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace tgd::gameplay {

enum class CraftSessionBuildError : std::uint8_t {
    none = 0,
    invalid_definition = 1,
    process_not_found = 2,
    capacity_exceeded = 3,
    invalid = 255,
};

enum class CraftSessionStage : std::uint8_t {
    awaiting_material = 1,
    performing_operations = 2,
    trial_ready = 3,
    rework_required = 4,
    completed = 5,
    invalid = 255,
};

enum class CraftActionDisposition : std::uint8_t {
    applied = 1,
    wrong_stage = 2,
    unknown_target = 3,
    wrong_order = 4,
    invalid_state = 5,
    invalid = 255,
};

struct CraftSessionSnapshot final {
    bool initialized{};
    contracts::StableContentKey process{};
    contracts::StableContentKey need{};
    contracts::StableContentKey selected_material{};
    contracts::StableContentKey expected_step{};
    contracts::StableContentKey output_item{};
    CraftSessionStage stage{CraftSessionStage::invalid};
    std::uint16_t completed_operation_count{};
    std::uint16_t operation_count{};
    std::uint16_t trial_count{};
    std::uint16_t mistake_count{};
    std::uint16_t rework_count{};
    bool completed{};
    std::uint64_t checksum{};

    [[nodiscard]] friend constexpr bool operator==(
        const CraftSessionSnapshot&,
        const CraftSessionSnapshot&
    ) noexcept = default;
};

struct CraftActionResult final {
    CraftActionDisposition disposition{CraftActionDisposition::invalid};
    CraftSessionSnapshot snapshot{};
};

class CraftSession final {
  public:
    static constexpr std::size_t operation_capacity =
        contracts::sandbox_craft_step_capacity;

    [[nodiscard]] CraftSessionBuildError initialize(
        const contracts::CraftDefinition& definition,
        contracts::StableContentKey process
    ) noexcept;
    [[nodiscard]] CraftActionResult select_material(
        contracts::StableContentKey material
    ) noexcept;
    [[nodiscard]] CraftActionResult perform_operation(
        contracts::StableContentKey step
    ) noexcept;
    [[nodiscard]] CraftActionResult run_trial() noexcept;
    [[nodiscard]] CraftActionResult perform_rework() noexcept;
    [[nodiscard]] CraftSessionBuildError restart() noexcept;
    [[nodiscard]] CraftSessionSnapshot snapshot() const noexcept;

  private:
    [[nodiscard]] CraftSessionBuildError build_runtime() noexcept;
    [[nodiscard]] CraftActionResult reject(CraftActionDisposition disposition)
        const noexcept;
    void refresh_snapshot() noexcept;

    const contracts::CraftDefinition* definition_{};
    const contracts::CraftProcessDefinition* process_{};
    std::array<contracts::StableContentKey, operation_capacity> operations_{};
    std::size_t operation_count_{};
    contracts::StableContentKey trial_step_{};
    contracts::StableContentKey rework_step_{};
    contracts::CraftMaterialOutcome selected_outcome_{
        contracts::CraftMaterialOutcome::invalid
    };
    std::size_t completed_operation_count_{};
    std::uint16_t trial_count_{};
    std::uint16_t mistake_count_{};
    std::uint16_t rework_count_{};
    CraftSessionStage stage_{CraftSessionStage::invalid};
    bool initialized_{};
    CraftSessionSnapshot snapshot_{};
};

}  // namespace tgd::gameplay
