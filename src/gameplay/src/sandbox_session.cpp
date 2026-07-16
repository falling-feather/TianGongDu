#include <tgd/gameplay/sandbox_session.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace tgd::gameplay {
namespace {

constexpr std::uint64_t fnv_offset = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t fnv_prime = 1'099'511'628'211ULL;
constexpr std::uint64_t sandbox_session_checksum_domain = 0x53414e44424f5831ULL;

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= fnv_prime;
}

template <typename Integer>
void hash_integer(std::uint64_t& hash, Integer value) noexcept {
    using Unsigned = std::make_unsigned_t<Integer>;
    static_assert(sizeof(Unsigned) <= sizeof(std::uint64_t));
    auto bits = static_cast<std::uint64_t>(static_cast<Unsigned>(value));
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        hash_byte(hash, static_cast<std::uint8_t>(bits & 0xffU));
        if (index + 1U < sizeof(Integer)) {
            bits >>= 8U;
        }
    }
}

void hash_pose(std::uint64_t& hash, contracts::GroundPoseMm pose) noexcept {
    hash_integer(hash, pose.x);
    hash_integer(hash, pose.y);
    hash_integer(hash, pose.height);
    hash_integer(hash, pose.floor_layer);
}

[[nodiscard]] bool valid_content_id(contracts::ContentId id) noexcept {
    return id.key != 0 && !id.name.empty() && contracts::stable_content_key(id.name) == id.key;
}

[[nodiscard]] SandboxOperateDispatch operate_disposition(
    contracts::SandboxOperateDisposition disposition
) noexcept {
    SandboxOperateDispatch dispatch{};
    dispatch.result.disposition = disposition;
    return dispatch;
}

}  // namespace

SandboxSessionBuildResult SandboxSession::initialize(
    const contracts::SandboxDefinition& validated_core,
    const contracts::SandboxGameplayBindingDefinition& gameplay_binding,
    const SandboxPlayerRuntimeBinding& player_binding
) noexcept {
    using BuildError = SandboxSessionBuildError;

    if (initialized_) {
        return {BuildError::already_initialized, {}};
    }
    if (validated_core.interactions.size() > interaction_capacity ||
        validated_core.mechanisms.size() > mechanism_capacity ||
        validated_core.ground_blockers.size() > ground_blocker_capacity ||
        validated_core.safe_points.size() > contracts::sandbox_safe_point_capacity) {
        return {BuildError::capacity_exceeded, {}};
    }
    if (!valid_content_id(validated_core.player.id) ||
        !valid_content_id(validated_core.player.initial_safe_point_id)) {
        return {BuildError::invalid_player_definition, {}};
    }
    if (player_binding.player_content_id.key == 0 &&
        player_binding.player_content_id.name.empty()) {
        return {BuildError::missing_player_runtime_binding, {}};
    }
    if (!valid_content_id(player_binding.player_content_id)) {
        return {BuildError::invalid_player_runtime_id, {}};
    }
    if (player_binding.player_content_id != validated_core.player.id) {
        return {BuildError::player_runtime_id_mismatch, {}};
    }
    if (player_binding.actor_key == 0) {
        return {BuildError::invalid_player_actor, {}};
    }

    const contracts::SandboxSafePointDefinition* retry_safe_point = nullptr;
    std::size_t retry_safe_point_matches = 0;
    for (const auto& safe_point : validated_core.safe_points) {
        if (safe_point.id == validated_core.player.initial_safe_point_id) {
            retry_safe_point = &safe_point;
            ++retry_safe_point_matches;
        }
    }
    if (retry_safe_point_matches != 1 || retry_safe_point == nullptr ||
        !valid_content_id(retry_safe_point->id) ||
        retry_safe_point->region_id != validated_core.player.region_id) {
        return {BuildError::invalid_initial_safe_point, {}};
    }

    const auto binding_validation =
        contracts::validate_sandbox_gameplay_binding(validated_core, gameplay_binding);
    if (!contracts::sandbox_gameplay_binding_is_valid(binding_validation)) {
        return {BuildError::invalid_gameplay_binding, binding_validation};
    }

    SandboxSession candidate{};
    candidate.player_spawn_pose_ = validated_core.player.pose;
    candidate.player_spawn_facing_millidegrees_ =
        validated_core.player.facing_millidegrees;
    candidate.player_retry_pose_ = retry_safe_point->pose;
    candidate.player_retry_facing_millidegrees_ = retry_safe_point->facing_millidegrees;

    candidate.ground_blocker_count_ = validated_core.ground_blockers.size();
    for (std::size_t index = 0; index < candidate.ground_blocker_count_; ++index) {
        candidate.ground_blockers_[index].key =
            validated_core.ground_blockers[index].id.key;
        candidate.ground_blockers_[index].state =
            contracts::sandbox_ground_blocker_initial_state;
    }
    std::sort(
        candidate.ground_blockers_.begin(),
        candidate.ground_blockers_.begin() + candidate.ground_blocker_count_,
        [](const GroundBlockerRecord& left, const GroundBlockerRecord& right) {
            return left.key < right.key;
        }
    );

    candidate.mechanism_count_ = validated_core.mechanisms.size();
    for (std::size_t index = 0; index < candidate.mechanism_count_; ++index) {
        candidate.mechanisms_[index].key = validated_core.mechanisms[index].id.key;
        candidate.mechanisms_[index].state = contracts::sandbox_mechanism_initial_state;
    }
    std::sort(
        candidate.mechanisms_.begin(),
        candidate.mechanisms_.begin() + candidate.mechanism_count_,
        [](const MechanismRecord& left, const MechanismRecord& right) {
            return left.key < right.key;
        }
    );
    for (std::size_t index = 0; index < candidate.mechanism_count_; ++index) {
        const auto binding = std::find_if(
            gameplay_binding.mechanism_bindings.begin(),
            gameplay_binding.mechanism_bindings.end(),
            [&candidate, index](const contracts::SandboxMechanismGameplayBinding& item) {
                return item.mechanism_id.key == candidate.mechanisms_[index].key;
            }
        );
        if (binding == gameplay_binding.mechanism_bindings.end()) {
            return {BuildError::invalid_owned_state, binding_validation};
        }
        const auto blocker = std::find_if(
            candidate.ground_blockers_.begin(),
            candidate.ground_blockers_.begin() + candidate.ground_blocker_count_,
            [&binding](const GroundBlockerRecord& item) {
                return item.key == binding->target_ground_blocker_id.key;
            }
        );
        if (blocker == candidate.ground_blockers_.begin() +
                           candidate.ground_blocker_count_) {
            return {BuildError::invalid_owned_state, binding_validation};
        }
        candidate.mechanisms_[index].ground_blocker_index = static_cast<std::uint16_t>(
            blocker - candidate.ground_blockers_.begin()
        );
    }

    candidate.interaction_count_ = validated_core.interactions.size();
    for (std::size_t index = 0; index < candidate.interaction_count_; ++index) {
        candidate.interactions_[index].key = validated_core.interactions[index].id.key;
        candidate.interactions_[index].pose = validated_core.interactions[index].pose;
        candidate.interactions_[index].state = contracts::sandbox_interaction_initial_state;
    }
    std::sort(
        candidate.interactions_.begin(),
        candidate.interactions_.begin() + candidate.interaction_count_,
        [](const InteractionRecord& left, const InteractionRecord& right) {
            return left.key < right.key;
        }
    );
    for (std::size_t index = 0; index < candidate.interaction_count_; ++index) {
        const auto binding = std::find_if(
            gameplay_binding.interaction_bindings.begin(),
            gameplay_binding.interaction_bindings.end(),
            [&candidate, index](const contracts::SandboxInteractionGameplayBinding& item) {
                return item.interaction_id.key == candidate.interactions_[index].key;
            }
        );
        if (binding == gameplay_binding.interaction_bindings.end()) {
            return {BuildError::invalid_owned_state, binding_validation};
        }
        const auto mechanism = std::find_if(
            candidate.mechanisms_.begin(),
            candidate.mechanisms_.begin() + candidate.mechanism_count_,
            [&binding](const MechanismRecord& item) {
                return item.key == binding->target_mechanism_id.key;
            }
        );
        if (mechanism == candidate.mechanisms_.begin() + candidate.mechanism_count_) {
            return {BuildError::invalid_owned_state, binding_validation};
        }
        candidate.interactions_[index].range_mm = binding->range_mm;
        candidate.interactions_[index].mechanism_index = static_cast<std::uint16_t>(
            mechanism - candidate.mechanisms_.begin()
        );
    }

    candidate.initialized_ = true;
    candidate.snapshot_.generation = 1;
    candidate.snapshot_.player_content = validated_core.player.id.key;
    candidate.snapshot_.player_actor = player_binding.actor_key;
    candidate.snapshot_.retry_safe_point = retry_safe_point->id.key;
    candidate.snapshot_.player_pose = candidate.player_spawn_pose_;
    candidate.snapshot_.player_facing_millidegrees =
        candidate.player_spawn_facing_millidegrees_;
    candidate.snapshot_.interaction_count =
        static_cast<std::uint16_t>(candidate.interaction_count_);
    candidate.snapshot_.mechanism_count =
        static_cast<std::uint16_t>(candidate.mechanism_count_);
    candidate.snapshot_.ground_blocker_count =
        static_cast<std::uint16_t>(candidate.ground_blocker_count_);
    candidate.update_checksum();
    if (!candidate.valid_owned_state()) {
        return {BuildError::invalid_owned_state, binding_validation};
    }

    *this = candidate;
    return {BuildError::none, binding_validation};
}

SandboxOperateDispatch SandboxSession::submit_operate(
    const contracts::SandboxOperateCommand& command
) noexcept {
    using Disposition = contracts::SandboxOperateDisposition;

    if (!valid_owned_state()) {
        return operate_disposition(Disposition::invalid_binding);
    }
    if (command.generation == 0 || command.generation != snapshot_.generation) {
        return operate_disposition(Disposition::stale_generation);
    }
    if (command.sequence == 0 || command.sequence <= snapshot_.last_command_sequence) {
        return operate_disposition(Disposition::stale_sequence);
    }
    if (command.actor != snapshot_.player_actor) {
        return operate_disposition(Disposition::invalid_binding);
    }

    const auto interaction = std::find_if(
        interactions_.begin(),
        interactions_.begin() + interaction_count_,
        [&command](const InteractionRecord& item) { return item.key == command.interaction; }
    );
    if (interaction == interactions_.begin() + interaction_count_) {
        return operate_disposition(Disposition::unknown_interaction);
    }
    if (interaction->mechanism_index >= mechanism_count_) {
        return operate_disposition(Disposition::invalid_binding);
    }
    const auto& mechanism = mechanisms_[interaction->mechanism_index];
    if (mechanism.ground_blocker_index >= ground_blocker_count_) {
        return operate_disposition(Disposition::invalid_binding);
    }
    const auto& blocker = ground_blockers_[mechanism.ground_blocker_index];
    const bool initial_state =
        interaction->state == contracts::SandboxInteractionState::uncompleted &&
        mechanism.state == contracts::SandboxMechanismState::inactive &&
        blocker.state == contracts::SandboxGroundBlockerState::enabled_solid;
    const bool completed_state =
        interaction->state == contracts::SandboxInteractionState::completed &&
        mechanism.state == contracts::SandboxMechanismState::activated &&
        blocker.state == contracts::SandboxGroundBlockerState::disabled_non_solid;
    if (!initial_state && !completed_state) {
        return operate_disposition(Disposition::invalid_binding);
    }
    if (snapshot_.player_pose.floor_layer != interaction->pose.floor_layer) {
        return operate_disposition(Disposition::floor_mismatch);
    }
    if (interaction->range_mm < contracts::sandbox_operate_range_min_mm ||
        interaction->range_mm > contracts::sandbox_operate_range_max_mm) {
        return operate_disposition(Disposition::invalid_binding);
    }
    const auto range = contracts::sandbox_check_operate_range(
        snapshot_.player_pose,
        interaction->pose,
        interaction->range_mm
    );
    if (range != contracts::SandboxOperateRangeCheck::eligible) {
        return operate_disposition(
            range == contracts::SandboxOperateRangeCheck::out_of_range
                ? Disposition::out_of_range
                : Disposition::invalid_binding
        );
    }
    if (completed_state) {
        return operate_disposition(Disposition::repeated_chain);
    }
    if (snapshot_.last_event_sequence >
        std::numeric_limits<std::uint64_t>::max() - 2U) {
        return operate_disposition(Disposition::invalid_binding);
    }

    const auto interaction_index = static_cast<std::size_t>(
        interaction - interactions_.begin()
    );
    const auto mechanism_index = static_cast<std::size_t>(interaction->mechanism_index);
    const auto blocker_index = static_cast<std::size_t>(mechanism.ground_blocker_index);
    const auto interaction_key = interaction->key;
    const auto mechanism_key = mechanism.key;
    const auto blocker_key = blocker.key;
    const auto first_event_sequence = snapshot_.last_event_sequence + 1U;

    SandboxSession candidate = *this;
    candidate.interactions_[interaction_index].state =
        contracts::SandboxInteractionState::completed;
    candidate.mechanisms_[mechanism_index].state =
        contracts::SandboxMechanismState::activated;
    candidate.ground_blockers_[blocker_index].state =
        contracts::SandboxGroundBlockerState::disabled_non_solid;
    candidate.snapshot_.last_command_sequence = command.sequence;
    candidate.snapshot_.last_completed_tick = command.completed_tick;
    candidate.snapshot_.last_event_sequence += 2U;
    candidate.update_checksum();
    if (!candidate.valid_owned_state()) {
        return operate_disposition(Disposition::invalid_binding);
    }

    *this = candidate;

    SandboxOperateDispatch dispatch{};
    dispatch.result.disposition = Disposition::completed_chain;
    dispatch.events[0] = {
        first_event_sequence,
        snapshot_.generation,
        command.completed_tick,
        contracts::SandboxGameplayEventKind::interaction_completed,
        snapshot_.player_actor,
        interaction_key,
        mechanism_key,
        blocker_key,
    };
    dispatch.events[1] = {
        first_event_sequence + 1U,
        snapshot_.generation,
        command.completed_tick,
        contracts::SandboxGameplayEventKind::mechanism_activated,
        snapshot_.player_actor,
        interaction_key,
        mechanism_key,
        blocker_key,
    };
    return dispatch;
}

SandboxSessionRetryDisposition SandboxSession::retry(
    const SandboxSessionRetryCommand& command
) noexcept {
    if (!initialized_) {
        return SandboxSessionRetryDisposition::invalid_state;
    }
    if (command.generation == 0 || command.generation != snapshot_.generation) {
        return SandboxSessionRetryDisposition::stale_generation;
    }
    if (command.sequence == 0 || command.sequence <= snapshot_.last_command_sequence) {
        return SandboxSessionRetryDisposition::stale_sequence;
    }
    if (!valid_owned_state()) {
        return SandboxSessionRetryDisposition::invalid_state;
    }
    const auto next_generation = sandbox_next_generation(snapshot_.generation);
    if (!next_generation.valid) {
        return SandboxSessionRetryDisposition::generation_exhausted;
    }

    SandboxSession candidate = *this;
    candidate.snapshot_.generation = next_generation.generation;
    candidate.snapshot_.player_pose = candidate.player_retry_pose_;
    candidate.snapshot_.player_facing_millidegrees =
        candidate.player_retry_facing_millidegrees_;
    candidate.snapshot_.last_command_sequence = 0;
    candidate.snapshot_.last_completed_tick = 0;
    for (std::size_t index = 0; index < candidate.interaction_count_; ++index) {
        candidate.interactions_[index].state = contracts::sandbox_interaction_initial_state;
    }
    for (std::size_t index = 0; index < candidate.mechanism_count_; ++index) {
        candidate.mechanisms_[index].state = contracts::sandbox_mechanism_initial_state;
    }
    for (std::size_t index = 0; index < candidate.ground_blocker_count_; ++index) {
        candidate.ground_blockers_[index].state =
            contracts::sandbox_ground_blocker_initial_state;
    }
    candidate.update_checksum();
    if (!candidate.valid_owned_state()) {
        return SandboxSessionRetryDisposition::invalid_state;
    }

    *this = candidate;
    return SandboxSessionRetryDisposition::restored;
}

const SandboxSessionSnapshot& SandboxSession::snapshot() const noexcept {
    return snapshot_;
}

contracts::SandboxInteractionState SandboxSession::interaction_state(
    contracts::StableContentKey interaction
) const noexcept {
    if (!valid_owned_state()) {
        return contracts::SandboxInteractionState::invalid;
    }
    const auto found = std::find_if(
        interactions_.begin(),
        interactions_.begin() + interaction_count_,
        [interaction](const InteractionRecord& item) { return item.key == interaction; }
    );
    return found == interactions_.begin() + interaction_count_
               ? contracts::SandboxInteractionState::invalid
               : found->state;
}

contracts::SandboxMechanismState SandboxSession::mechanism_state(
    contracts::StableContentKey mechanism
) const noexcept {
    if (!valid_owned_state()) {
        return contracts::SandboxMechanismState::invalid;
    }
    const auto found = std::find_if(
        mechanisms_.begin(),
        mechanisms_.begin() + mechanism_count_,
        [mechanism](const MechanismRecord& item) { return item.key == mechanism; }
    );
    return found == mechanisms_.begin() + mechanism_count_
               ? contracts::SandboxMechanismState::invalid
               : found->state;
}

contracts::SandboxGroundBlockerState SandboxSession::ground_blocker_state(
    contracts::StableContentKey ground_blocker
) const noexcept {
    if (!valid_owned_state()) {
        return contracts::SandboxGroundBlockerState::invalid;
    }
    const auto found = std::find_if(
        ground_blockers_.begin(),
        ground_blockers_.begin() + ground_blocker_count_,
        [ground_blocker](const GroundBlockerRecord& item) {
            return item.key == ground_blocker;
        }
    );
    return found == ground_blockers_.begin() + ground_blocker_count_
               ? contracts::SandboxGroundBlockerState::invalid
               : found->state;
}

bool SandboxSession::valid_owned_state() const noexcept {
    if (!initialized_ || interaction_count_ > interaction_capacity ||
        mechanism_count_ > mechanism_capacity ||
        ground_blocker_count_ > ground_blocker_capacity ||
        interaction_count_ != mechanism_count_ || snapshot_.generation == 0 ||
        snapshot_.player_content == 0 || snapshot_.player_actor == 0 ||
        snapshot_.retry_safe_point == 0 ||
        snapshot_.interaction_count != interaction_count_ ||
        snapshot_.mechanism_count != mechanism_count_ ||
        snapshot_.ground_blocker_count != ground_blocker_count_ ||
        (snapshot_.last_event_sequence & 1U) != 0) {
        return false;
    }

    for (std::size_t index = 0; index < ground_blocker_count_; ++index) {
        if (ground_blockers_[index].key == 0 ||
            (index > 0 && ground_blockers_[index - 1].key >= ground_blockers_[index].key) ||
            (ground_blockers_[index].state !=
                 contracts::SandboxGroundBlockerState::enabled_solid &&
             ground_blockers_[index].state !=
                 contracts::SandboxGroundBlockerState::disabled_non_solid)) {
            return false;
        }
    }

    std::array<std::uint8_t, mechanism_capacity> incoming_interactions{};
    std::array<std::uint8_t, ground_blocker_capacity> blocker_writers{};
    for (std::size_t index = 0; index < mechanism_count_; ++index) {
        const auto& mechanism = mechanisms_[index];
        if (mechanism.key == 0 ||
            (index > 0 && mechanisms_[index - 1].key >= mechanism.key) ||
            mechanism.ground_blocker_index >= ground_blocker_count_ ||
            (mechanism.state != contracts::SandboxMechanismState::inactive &&
             mechanism.state != contracts::SandboxMechanismState::activated)) {
            return false;
        }
        auto& writers = blocker_writers[mechanism.ground_blocker_index];
        ++writers;
        if (writers > 1) {
            return false;
        }
    }

    for (std::size_t index = 0; index < interaction_count_; ++index) {
        const auto& interaction = interactions_[index];
        if (interaction.key == 0 ||
            (index > 0 && interactions_[index - 1].key >= interaction.key) ||
            interaction.mechanism_index >= mechanism_count_ ||
            interaction.range_mm < contracts::sandbox_operate_range_min_mm ||
            interaction.range_mm > contracts::sandbox_operate_range_max_mm ||
            (interaction.state != contracts::SandboxInteractionState::uncompleted &&
             interaction.state != contracts::SandboxInteractionState::completed)) {
            return false;
        }
        auto& incoming = incoming_interactions[interaction.mechanism_index];
        ++incoming;
        if (incoming > 1) {
            return false;
        }
        const auto& mechanism = mechanisms_[interaction.mechanism_index];
        const auto& blocker = ground_blockers_[mechanism.ground_blocker_index];
        const bool initial =
            interaction.state == contracts::SandboxInteractionState::uncompleted &&
            mechanism.state == contracts::SandboxMechanismState::inactive &&
            blocker.state == contracts::SandboxGroundBlockerState::enabled_solid;
        const bool completed =
            interaction.state == contracts::SandboxInteractionState::completed &&
            mechanism.state == contracts::SandboxMechanismState::activated &&
            blocker.state == contracts::SandboxGroundBlockerState::disabled_non_solid;
        if (!initial && !completed) {
            return false;
        }
    }

    for (std::size_t index = 0; index < mechanism_count_; ++index) {
        if (incoming_interactions[index] != 1) {
            return false;
        }
    }
    for (std::size_t index = 0; index < ground_blocker_count_; ++index) {
        if (blocker_writers[index] == 0 &&
            ground_blockers_[index].state !=
                contracts::SandboxGroundBlockerState::enabled_solid) {
            return false;
        }
    }
    return snapshot_.checksum == compute_checksum();
}

std::uint64_t SandboxSession::compute_checksum() const noexcept {
    auto hash = fnv_offset;
    hash_integer(hash, sandbox_session_checksum_domain);
    hash_integer(hash, snapshot_.generation);
    hash_integer(hash, snapshot_.player_content);
    hash_integer(hash, snapshot_.player_actor);
    hash_integer(hash, snapshot_.retry_safe_point);
    hash_pose(hash, player_spawn_pose_);
    hash_integer(hash, player_spawn_facing_millidegrees_);
    hash_pose(hash, player_retry_pose_);
    hash_integer(hash, player_retry_facing_millidegrees_);
    hash_pose(hash, snapshot_.player_pose);
    hash_integer(hash, snapshot_.player_facing_millidegrees);
    hash_integer(hash, snapshot_.last_command_sequence);
    hash_integer(hash, snapshot_.last_completed_tick);
    hash_integer(hash, snapshot_.last_event_sequence);
    hash_integer(hash, snapshot_.interaction_count);
    hash_integer(hash, snapshot_.mechanism_count);
    hash_integer(hash, snapshot_.ground_blocker_count);

    for (std::size_t index = 0; index < interaction_count_; ++index) {
        const auto& interaction = interactions_[index];
        hash_integer(hash, interaction.key);
        hash_pose(hash, interaction.pose);
        hash_integer(hash, interaction.range_mm);
        hash_integer(hash, mechanisms_[interaction.mechanism_index].key);
        hash_integer(hash, static_cast<std::uint8_t>(interaction.state));
    }
    for (std::size_t index = 0; index < mechanism_count_; ++index) {
        const auto& mechanism = mechanisms_[index];
        hash_integer(hash, mechanism.key);
        hash_integer(hash, ground_blockers_[mechanism.ground_blocker_index].key);
        hash_integer(hash, static_cast<std::uint8_t>(mechanism.state));
    }
    for (std::size_t index = 0; index < ground_blocker_count_; ++index) {
        hash_integer(hash, ground_blockers_[index].key);
        hash_integer(hash, static_cast<std::uint8_t>(ground_blockers_[index].state));
    }
    return hash;
}

void SandboxSession::update_checksum() noexcept {
    snapshot_.checksum = compute_checksum();
}

}  // namespace tgd::gameplay
