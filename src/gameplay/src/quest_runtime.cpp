#include <tgd/gameplay/quest_runtime.hpp>

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

[[nodiscard]] std::uint64_t squared_component(
    std::int32_t left,
    std::int32_t right
) noexcept {
    const auto delta = static_cast<std::int64_t>(left) - static_cast<std::int64_t>(right);
    const auto magnitude = static_cast<std::uint64_t>(delta < 0 ? -delta : delta);
    return magnitude * magnitude;
}

[[nodiscard]] std::uint64_t saturating_add(
    std::uint64_t left,
    std::uint64_t right
) noexcept {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    return right > maximum - left ? maximum : left + right;
}

[[nodiscard]] std::uint64_t ground_distance_squared(
    const contracts::GroundPoseMm& left,
    const contracts::GroundPoseMm& right
) noexcept {
    return saturating_add(
        squared_component(left.x, right.x),
        squared_component(left.y, right.y)
    );
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    auto bits = static_cast<std::uint64_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & 0xffU));
        bits >>= 8U;
    }
}

}  // namespace

QuestInteractionError DeterministicQuestInteractionResolver::initialize(
    std::span<const contracts::QuestInteractionDefinition> definitions
) noexcept {
    if (initialized_) {
        return QuestInteractionError::invalid_lifecycle;
    }
    if (definitions.empty() || definitions.size() > interaction_capacity) {
        return QuestInteractionError::invalid_definition;
    }
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const auto& definition = definitions[index];
        if (definition.id.key == 0 || definition.cell_id.key == 0 ||
            definition.objective_id.key == 0 || definition.radius_mm <= 0) {
            return QuestInteractionError::invalid_definition;
        }
        if (definition.kind != contracts::QuestInteractionKind::inspect &&
            definition.kind != contracts::QuestInteractionKind::operate &&
            definition.kind != contracts::QuestInteractionKind::talk &&
            definition.kind != contracts::QuestInteractionKind::choose) {
            return QuestInteractionError::invalid_definition;
        }
        if ((definition.kind == contracts::QuestInteractionKind::choose &&
             definition.selection_id.key == 0) ||
            (definition.kind != contracts::QuestInteractionKind::choose &&
             definition.selection_id.key != 0)) {
            return QuestInteractionError::invalid_definition;
        }
        for (std::size_t prerequisite = 0;
             prerequisite < definition.prerequisite_objectives.size();
             ++prerequisite) {
            const auto key = definition.prerequisite_objectives[prerequisite].key;
            if (key == 0 || key == definition.objective_id.key) {
                return QuestInteractionError::invalid_definition;
            }
            for (std::size_t prior = 0; prior < prerequisite; ++prior) {
                if (definition.prerequisite_objectives[prior].key == key) {
                    return QuestInteractionError::invalid_definition;
                }
            }
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definitions[prior].id.key == definition.id.key) {
                return QuestInteractionError::invalid_definition;
            }
            if (definitions[prior].objective_id.key == definition.objective_id.key &&
                (definitions[prior].kind != contracts::QuestInteractionKind::choose ||
                 definition.kind != contracts::QuestInteractionKind::choose ||
                 definitions[prior].selection_id.key == definition.selection_id.key)) {
                return QuestInteractionError::invalid_definition;
            }
        }
    }
    definitions_ = definitions;
    initialized_ = true;
    return QuestInteractionError::none;
}

QuestInteractionResult DeterministicQuestInteractionResolver::resolve(
    const QuestInteractionQuery& query,
    const IQuestRuntime& quest
) const noexcept {
    QuestInteractionResult result{};
    if (!initialized_) {
        result.error = QuestInteractionError::invalid_lifecycle;
        return result;
    }
    if (query.actor == 0 || query.cell == 0) {
        result.error = QuestInteractionError::invalid_query;
        return result;
    }

    auto best_distance = std::numeric_limits<std::uint64_t>::max();
    for (const auto& definition : definitions_) {
        if (definition.cell_id.key != query.cell ||
            definition.pose.floor_layer != query.pose.floor_layer ||
            quest.objective_state(definition.objective_id.key) != QuestObjectiveState::active) {
            continue;
        }
        const auto prerequisites_complete = std::all_of(
            definition.prerequisite_objectives.begin(),
            definition.prerequisite_objectives.end(),
            [&quest](const contracts::ContentId& prerequisite) {
                return quest.objective_state(prerequisite.key) ==
                       QuestObjectiveState::completed;
            }
        );
        if (!prerequisites_complete) {
            continue;
        }
        const auto distance = ground_distance_squared(query.pose, definition.pose);
        const auto radius = static_cast<std::uint64_t>(definition.radius_mm);
        if (distance > radius * radius) {
            continue;
        }
        if (!result.found || distance < best_distance ||
            (distance == best_distance && definition.id.key < result.interaction)) {
            result.found = true;
            result.interaction = definition.id.key;
            result.objective = definition.objective_id.key;
            result.selection = definition.selection_id.key;
            result.kind = definition.kind;
            best_distance = distance;
        }
    }
    return result;
}

QuestCombatTriggerError DeterministicQuestCombatTriggerResolver::initialize(
    std::span<const contracts::QuestCombatTriggerDefinition> definitions
) noexcept {
    if (initialized_) {
        return QuestCombatTriggerError::invalid_lifecycle;
    }
    if (definitions.empty() || definitions.size() > trigger_capacity) {
        return QuestCombatTriggerError::invalid_definition;
    }
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const auto& definition = definitions[index];
        if (definition.id.key == 0 || definition.objective_id.key == 0 ||
            definition.required_stance == 0) {
            return QuestCombatTriggerError::invalid_definition;
        }
        if (definition.kind != contracts::QuestCombatTriggerKind::player_hit_guarded &&
            definition.kind != contracts::QuestCombatTriggerKind::player_hit_evaded) {
            return QuestCombatTriggerError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definitions[prior].id.key == definition.id.key ||
                definitions[prior].objective_id.key == definition.objective_id.key) {
                return QuestCombatTriggerError::invalid_definition;
            }
        }
    }
    definitions_ = definitions;
    initialized_ = true;
    return QuestCombatTriggerError::none;
}

QuestCombatTriggerResult DeterministicQuestCombatTriggerResolver::resolve(
    const QuestCombatSignal& signal,
    const IQuestRuntime& quest
) const noexcept {
    QuestCombatTriggerResult result{};
    if (!initialized_) {
        result.error = QuestCombatTriggerError::invalid_lifecycle;
        return result;
    }
    if (signal.actor == 0 || signal.stance == 0) {
        result.error = QuestCombatTriggerError::invalid_signal;
        return result;
    }
    for (const auto& definition : definitions_) {
        if (definition.kind != signal.kind || definition.required_stance != signal.stance ||
            quest.objective_state(definition.objective_id.key) != QuestObjectiveState::active) {
            continue;
        }
        if (!result.found || definition.id.key < result.trigger) {
            result.found = true;
            result.trigger = definition.id.key;
            result.objective = definition.objective_id.key;
        }
    }
    return result;
}

QuestCombatOutcomeError DeterministicQuestCombatOutcomeResolver::initialize(
    std::span<const contracts::QuestCombatOutcomeDefinition> definitions
) noexcept {
    if (initialized_) {
        return QuestCombatOutcomeError::invalid_lifecycle;
    }
    if (definitions.empty() || definitions.size() > outcome_capacity) {
        return QuestCombatOutcomeError::invalid_definition;
    }
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const auto& definition = definitions[index];
        if (definition.id.key == 0 || definition.objective_id.key == 0) {
            return QuestCombatOutcomeError::invalid_definition;
        }
        const auto archetype_group =
            definition.kind ==
                contracts::QuestCombatOutcomeKind::hostile_archetype_defeated &&
            definition.archetype_id.key != 0 && definition.required_count != 0;
        const auto all_hostiles =
            definition.kind == contracts::QuestCombatOutcomeKind::all_hostiles_defeated &&
            definition.archetype_id.key == 0 && definition.required_count == 0;
        if (!archetype_group && !all_hostiles) {
            return QuestCombatOutcomeError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (definitions[prior].id.key == definition.id.key ||
                definitions[prior].objective_id.key == definition.objective_id.key) {
                return QuestCombatOutcomeError::invalid_definition;
            }
        }
    }
    definitions_ = definitions;
    initialized_ = true;
    return QuestCombatOutcomeError::none;
}

QuestCombatOutcomeResult DeterministicQuestCombatOutcomeResolver::resolve(
    std::span<const contracts::CombatActorSnapshot> actors,
    const IQuestRuntime& quest
) const noexcept {
    QuestCombatOutcomeResult result{};
    if (!initialized_) {
        result.error = QuestCombatOutcomeError::invalid_lifecycle;
        return result;
    }
    for (std::size_t index = 0; index < actors.size(); ++index) {
        if (actors[index].actor == 0 || actors[index].archetype == 0) {
            result.error = QuestCombatOutcomeError::invalid_actor_snapshot;
            return result;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (actors[prior].actor == actors[index].actor) {
                result.error = QuestCombatOutcomeError::invalid_actor_snapshot;
                return result;
            }
        }
    }

    for (const auto& definition : definitions_) {
        if (quest.objective_state(definition.objective_id.key) !=
            QuestObjectiveState::active) {
            continue;
        }
        bool condition_met = false;
        if (definition.kind ==
            contracts::QuestCombatOutcomeKind::hostile_archetype_defeated) {
            const auto defeated = static_cast<std::size_t>(std::count_if(
                actors.begin(),
                actors.end(),
                [&definition](const contracts::CombatActorSnapshot& actor) {
                    return actor.faction == contracts::CombatFaction::hostile &&
                           !actor.active &&
                           actor.archetype == definition.archetype_id.key;
                }
            ));
            condition_met = defeated >= definition.required_count;
        } else {
            const auto hostile_count = static_cast<std::size_t>(std::count_if(
                actors.begin(),
                actors.end(),
                [](const contracts::CombatActorSnapshot& actor) {
                    return actor.faction == contracts::CombatFaction::hostile;
                }
            ));
            condition_met = hostile_count != 0 &&
                            std::none_of(
                                actors.begin(),
                                actors.end(),
                                [](const contracts::CombatActorSnapshot& actor) {
                                    return actor.faction ==
                                               contracts::CombatFaction::hostile &&
                                           actor.active;
                                }
                            );
        }
        if (!condition_met) {
            continue;
        }
        if (!result.found || definition.id.key < result.outcome) {
            result.found = true;
            result.outcome = definition.id.key;
            result.objective = definition.objective_id.key;
        }
    }
    return result;
}

QuestBossPhaseError DeterministicQuestBossPhaseResolver::initialize(
    std::span<const contracts::QuestBossPhaseDefinition> definitions
) noexcept {
    if (initialized_) {
        return QuestBossPhaseError::invalid_lifecycle;
    }
    if (definitions.empty() || definitions.size() > phase_capacity) {
        return QuestBossPhaseError::invalid_definition;
    }
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const auto& definition = definitions[index];
        const auto valid_transition =
            (definition.health_percent == 0 && definition.next_stance == 0) ||
            (definition.health_percent > 0 && definition.health_percent <= 100 &&
             definition.next_stance != 0);
        if (definition.id.key == 0 || definition.id.name.empty() ||
            definition.objective_id.key == 0 || definition.objective_id.name.empty() ||
            definition.actor == 0 || !valid_transition) {
            return QuestBossPhaseError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            const auto& previous = definitions[prior];
            if (previous.id.key == definition.id.key ||
                previous.objective_id.key == definition.objective_id.key ||
                (previous.actor == definition.actor &&
                 previous.health_percent <= definition.health_percent)) {
                return QuestBossPhaseError::invalid_definition;
            }
        }
    }
    definitions_ = definitions;
    initialized_ = true;
    return QuestBossPhaseError::none;
}

QuestBossPhaseResult DeterministicQuestBossPhaseResolver::resolve(
    std::span<const contracts::CombatActorSnapshot> actors,
    const IQuestRuntime& quest
) const noexcept {
    QuestBossPhaseResult result{};
    if (!initialized_) {
        result.error = QuestBossPhaseError::invalid_lifecycle;
        return result;
    }
    for (std::size_t index = 0; index < actors.size(); ++index) {
        const auto& actor = actors[index];
        if (actor.actor == 0 || actor.archetype == 0 || actor.resources.health_max <= 0 ||
            actor.resources.health < 0 || actor.resources.health > actor.resources.health_max) {
            result.error = QuestBossPhaseError::invalid_actor_snapshot;
            return result;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (actors[prior].actor == actor.actor) {
                result.error = QuestBossPhaseError::invalid_actor_snapshot;
                return result;
            }
        }
    }

    for (const auto& definition : definitions_) {
        if (quest.objective_state(definition.objective_id.key) != QuestObjectiveState::active) {
            continue;
        }
        const auto actor = std::find_if(
            actors.begin(),
            actors.end(),
            [&definition](const contracts::CombatActorSnapshot& candidate) {
                return candidate.actor == definition.actor;
            }
        );
        if (actor == actors.end()) {
            result.error = QuestBossPhaseError::invalid_actor_snapshot;
            return result;
        }
        const auto health_scaled = static_cast<std::int64_t>(actor->resources.health) * 100;
        const auto threshold_scaled =
            static_cast<std::int64_t>(actor->resources.health_max) * definition.health_percent;
        if (health_scaled <= threshold_scaled) {
            result.found = true;
            result.phase = definition.id.key;
            result.objective = definition.objective_id.key;
            result.actor = definition.actor;
            result.next_stance = definition.next_stance;
        }
        return result;
    }
    return result;
}

QuestError DeterministicQuestRuntime::initialize(
    const contracts::VerticalSliceDefinition& definition,
    contracts::StableActorKey player_actor
) noexcept {
    if (lifecycle_ != QuestLifecycle::uninitialized) {
        return QuestError::invalid_lifecycle;
    }
    const auto validation = validate_definition(definition);
    if (validation != QuestError::none) {
        return validation;
    }
    if (player_actor == 0 || player_actor != definition.player.actor) {
        return QuestError::invalid_definition;
    }
    definition_ = &definition;
    player_actor_ = player_actor;
    lifecycle_ = QuestLifecycle::ready;
    refresh_snapshot();
    return QuestError::none;
}

QuestError DeterministicQuestRuntime::start() noexcept {
    if (lifecycle_ != QuestLifecycle::ready) {
        return QuestError::invalid_lifecycle;
    }
    lifecycle_ = QuestLifecycle::running;
    return QuestError::none;
}

QuestError DeterministicQuestRuntime::pause() noexcept {
    if (lifecycle_ != QuestLifecycle::running) {
        return QuestError::invalid_lifecycle;
    }
    lifecycle_ = QuestLifecycle::paused;
    return QuestError::none;
}

QuestError DeterministicQuestRuntime::resume() noexcept {
    if (lifecycle_ != QuestLifecycle::paused) {
        return QuestError::invalid_lifecycle;
    }
    lifecycle_ = QuestLifecycle::running;
    return QuestError::none;
}

QuestError DeterministicQuestRuntime::destroy() noexcept {
    if (lifecycle_ == QuestLifecycle::uninitialized ||
        lifecycle_ == QuestLifecycle::destroyed) {
        return QuestError::invalid_lifecycle;
    }
    lifecycle_ = QuestLifecycle::destroyed;
    definition_ = nullptr;
    completed_objective_count_ = 0;
    selection_count_ = 0;
    return QuestError::none;
}

QuestApplyResult DeterministicQuestRuntime::apply(
    const contracts::QuestCommand& command,
    IQuestEventSink& sink
) noexcept {
    QuestApplyResult result{};
    if (lifecycle_ != QuestLifecycle::running && lifecycle_ != QuestLifecycle::resolved) {
        result.error = QuestError::invalid_lifecycle;
        return result;
    }
    if (command.actor != player_actor_ || command.sequence == 0 || command.objective == 0 ||
        command.type != contracts::QuestCommandType::complete_objective) {
        result.error = QuestError::invalid_command;
        return result;
    }
    if (command.completed_tick < snapshot_.tick) {
        result.error = QuestError::tick_regressed;
        return result;
    }
    if (command.sequence <= last_command_sequence_) {
        result.error = QuestError::stale_command_sequence;
        return result;
    }
    const auto state = objective_state(command.objective);
    if (state == QuestObjectiveState::unknown) {
        result.error = QuestError::unknown_objective;
        return result;
    }
    if (state == QuestObjectiveState::locked) {
        result.error = QuestError::objective_not_active;
        return result;
    }
    if (!valid_selection(command.objective, command.selection)) {
        result.error = QuestError::invalid_selection;
        return result;
    }
    if (state == QuestObjectiveState::completed &&
        selected_option(command.objective) != command.selection) {
        result.error = QuestError::selection_conflict;
        return result;
    }

    const auto objective_stage_index = objective_stage(command.objective);
    std::size_t event_count = 0;
    last_command_sequence_ = command.sequence;
    snapshot_.tick = command.completed_tick;
    if (state == QuestObjectiveState::completed) {
        events_[event_count++] = {
            command.completed_tick,
            contracts::QuestEventType::objective_already_completed,
            command.actor,
            definition_->id.key,
            definition_->beats[objective_stage_index].id.key,
            command.objective,
            command.selection,
        };
        refresh_snapshot();
        sink.publish(std::span{events_}.first(event_count));
        result.quest_resolved = lifecycle_ == QuestLifecycle::resolved;
        return result;
    }

    completed_objectives_[completed_objective_count_++] = command.objective;
    if (command.selection != 0) {
        selected_objectives_[selection_count_] = command.objective;
        selected_options_[selection_count_] = command.selection;
        ++selection_count_;
    }
    result.accepted = true;
    events_[event_count++] = {
        command.completed_tick,
        contracts::QuestEventType::objective_completed,
        command.actor,
        definition_->id.key,
        definition_->beats[stage_index_].id.key,
        command.objective,
        command.selection,
    };
    if (active_stage_complete()) {
        result.stage_advanced = true;
        if (stage_index_ + 1 == definition_->beats.size()) {
            lifecycle_ = QuestLifecycle::resolved;
            result.quest_resolved = true;
            events_[event_count++] = {
                command.completed_tick,
                contracts::QuestEventType::quest_resolved,
                command.actor,
                definition_->id.key,
                definition_->beats[stage_index_].id.key,
                command.objective,
                command.selection,
            };
        } else {
            ++stage_index_;
            events_[event_count++] = {
                command.completed_tick,
                contracts::QuestEventType::stage_advanced,
                command.actor,
                definition_->id.key,
                definition_->beats[stage_index_].id.key,
                command.objective,
                command.selection,
            };
        }
    }
    refresh_snapshot();
    sink.publish(std::span{events_}.first(event_count));
    return result;
}

QuestObjectiveState DeterministicQuestRuntime::objective_state(
    contracts::StableContentKey objective
) const noexcept {
    const auto stage = objective_stage(objective);
    if (stage == stage_capacity) {
        return QuestObjectiveState::unknown;
    }
    if (is_completed(objective)) {
        return QuestObjectiveState::completed;
    }
    return stage == stage_index_ ? QuestObjectiveState::active : QuestObjectiveState::locked;
}

contracts::StableContentKey DeterministicQuestRuntime::selected_option(
    contracts::StableContentKey objective
) const noexcept {
    for (std::size_t index = 0; index < selection_count_; ++index) {
        if (selected_objectives_[index] == objective) {
            return selected_options_[index];
        }
    }
    return 0;
}

const contracts::QuestSnapshot& DeterministicQuestRuntime::snapshot() const noexcept {
    return snapshot_;
}

QuestLifecycle DeterministicQuestRuntime::lifecycle() const noexcept {
    return lifecycle_;
}

QuestError DeterministicQuestRuntime::validate_definition(
    const contracts::VerticalSliceDefinition& definition
) const noexcept {
    if (definition.id.key == 0 || definition.beats.empty()) {
        return QuestError::invalid_definition;
    }
    if (definition.beats.size() > stage_capacity) {
        return QuestError::too_many_stages;
    }
    std::array<contracts::StableContentKey, objective_capacity> objectives{};
    std::size_t objective_count = 0;
    for (const auto& stage : definition.beats) {
        if (stage.id.key == 0 || stage.objectives.empty()) {
            return QuestError::invalid_definition;
        }
        if (stage.objectives.size() > objective_capacity - objective_count) {
            return QuestError::too_many_objectives;
        }
        for (const auto& objective : stage.objectives) {
            if (objective.key == 0) {
                return QuestError::invalid_definition;
            }
            if (std::find(objectives.begin(), objectives.begin() + objective_count, objective.key) !=
                objectives.begin() + objective_count) {
                return QuestError::duplicate_objective;
            }
            objectives[objective_count++] = objective.key;
        }
    }
    for (std::size_t index = 0; index < definition.quest_interactions.size(); ++index) {
        const auto& interaction = definition.quest_interactions[index];
        const auto objective_known = std::find(
            objectives.begin(),
            objectives.begin() + objective_count,
            interaction.objective_id.key
        ) != objectives.begin() + objective_count;
        if (interaction.id.key == 0 || !objective_known ||
            (interaction.kind == contracts::QuestInteractionKind::choose &&
             interaction.selection_id.key == 0) ||
            (interaction.kind != contracts::QuestInteractionKind::choose &&
             interaction.selection_id.key != 0)) {
            return QuestError::invalid_definition;
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            const auto& previous = definition.quest_interactions[prior];
            if (previous.id.key == interaction.id.key ||
                (previous.objective_id.key == interaction.objective_id.key &&
                 (previous.kind != contracts::QuestInteractionKind::choose ||
                  interaction.kind != contracts::QuestInteractionKind::choose ||
                  previous.selection_id.key == interaction.selection_id.key))) {
                return QuestError::invalid_definition;
            }
        }
    }
    return QuestError::none;
}

std::size_t DeterministicQuestRuntime::objective_stage(
    contracts::StableContentKey objective
) const noexcept {
    if (definition_ == nullptr) {
        return stage_capacity;
    }
    for (std::size_t stage = 0; stage < definition_->beats.size(); ++stage) {
        const auto& objectives = definition_->beats[stage].objectives;
        if (std::find_if(
                objectives.begin(),
                objectives.end(),
                [objective](const contracts::ContentId& value) {
                    return value.key == objective;
                }
            ) != objectives.end()) {
            return stage;
        }
    }
    return stage_capacity;
}

bool DeterministicQuestRuntime::is_completed(
    contracts::StableContentKey objective
) const noexcept {
    return std::find(
               completed_objectives_.begin(),
               completed_objectives_.begin() + completed_objective_count_,
               objective
           ) != completed_objectives_.begin() + completed_objective_count_;
}

std::size_t DeterministicQuestRuntime::completed_in_stage(std::size_t stage_index) const noexcept {
    const auto& objectives = definition_->beats[stage_index].objectives;
    return static_cast<std::size_t>(std::count_if(
        objectives.begin(),
        objectives.end(),
        [this](const contracts::ContentId& objective) {
            return is_completed(objective.key);
        }
    ));
}

bool DeterministicQuestRuntime::active_stage_complete() const noexcept {
    return completed_in_stage(stage_index_) == definition_->beats[stage_index_].objectives.size();
}

bool DeterministicQuestRuntime::objective_requires_selection(
    contracts::StableContentKey objective
) const noexcept {
    return definition_ != nullptr &&
           std::any_of(
               definition_->quest_interactions.begin(),
               definition_->quest_interactions.end(),
               [objective](const contracts::QuestInteractionDefinition& interaction) {
                   return interaction.objective_id.key == objective &&
                          interaction.kind == contracts::QuestInteractionKind::choose;
               }
           );
}

bool DeterministicQuestRuntime::valid_selection(
    contracts::StableContentKey objective,
    contracts::StableContentKey selection
) const noexcept {
    if (!objective_requires_selection(objective)) {
        return selection == 0;
    }
    return selection != 0 &&
           std::any_of(
               definition_->quest_interactions.begin(),
               definition_->quest_interactions.end(),
               [objective, selection](
                   const contracts::QuestInteractionDefinition& interaction
               ) {
                   return interaction.objective_id.key == objective &&
                          interaction.kind == contracts::QuestInteractionKind::choose &&
                          interaction.selection_id.key == selection;
               }
           );
}

void DeterministicQuestRuntime::refresh_snapshot() noexcept {
    snapshot_.quest = definition_->id.key;
    snapshot_.stage = definition_->beats[stage_index_].id.key;
    snapshot_.stage_index = static_cast<std::uint16_t>(stage_index_);
    snapshot_.stage_count = static_cast<std::uint16_t>(definition_->beats.size());
    snapshot_.completed_in_stage = static_cast<std::uint16_t>(completed_in_stage(stage_index_));
    snapshot_.required_in_stage =
        static_cast<std::uint16_t>(definition_->beats[stage_index_].objectives.size());
    snapshot_.completed_total = static_cast<std::uint16_t>(completed_objective_count_);
    snapshot_.selection_count = static_cast<std::uint16_t>(selection_count_);
    snapshot_.resolved = lifecycle_ == QuestLifecycle::resolved;
    update_checksum();
}

void DeterministicQuestRuntime::update_checksum() noexcept {
    auto hash = fnv_offset;
    hash_integer(hash, snapshot_.tick);
    hash_integer(hash, snapshot_.quest);
    hash_integer(hash, snapshot_.stage);
    hash_integer(hash, snapshot_.stage_index);
    hash_integer(hash, snapshot_.stage_count);
    hash_integer(hash, snapshot_.completed_in_stage);
    hash_integer(hash, snapshot_.required_in_stage);
    hash_integer(hash, snapshot_.completed_total);
    hash_integer(hash, snapshot_.selection_count);
    hash_byte(hash, snapshot_.resolved ? 1U : 0U);
    hash_integer(hash, last_command_sequence_);
    for (std::size_t index = 0; index < completed_objective_count_; ++index) {
        hash_integer(hash, completed_objectives_[index]);
    }
    for (std::size_t index = 0; index < selection_count_; ++index) {
        hash_integer(hash, selected_objectives_[index]);
        hash_integer(hash, selected_options_[index]);
    }
    snapshot_.checksum = hash;
}

}  // namespace tgd::gameplay
