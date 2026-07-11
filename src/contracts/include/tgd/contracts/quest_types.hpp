#pragma once

#include <tgd/contracts/content_definition.hpp>

#include <cstdint>

namespace tgd::contracts {

enum class QuestCommandType : std::uint8_t {
    complete_objective,
};

enum class QuestEventType : std::uint8_t {
    objective_completed,
    objective_already_completed,
    stage_advanced,
    quest_resolved,
};

struct QuestCommand final {
    TickIndex completed_tick{};
    StableActorKey actor{};
    CommandSequence sequence{};
    QuestCommandType type{QuestCommandType::complete_objective};
    StableContentKey objective{};
};

struct QuestEvent final {
    TickIndex tick{};
    QuestEventType type{QuestEventType::objective_completed};
    StableActorKey actor{};
    StableContentKey quest{};
    StableContentKey stage{};
    StableContentKey objective{};
};

struct QuestSnapshot final {
    TickIndex tick{};
    StableContentKey quest{};
    StableContentKey stage{};
    std::uint16_t stage_index{};
    std::uint16_t stage_count{};
    std::uint16_t completed_in_stage{};
    std::uint16_t required_in_stage{};
    std::uint16_t completed_total{};
    bool resolved{};
    std::uint64_t checksum{};
};

}  // namespace tgd::contracts
