#pragma once

#include <tgd/contracts/action_registry.generated.hpp>
#include <tgd/contracts/combat_types.hpp>

#include <cstdint>

namespace tgd::gameplay {

enum class CombatActionIntentError : std::uint8_t {
    none,
    out_of_order_platform_sequence,
    unsupported_action,
    invalid_sample,
    invalid_identity,
};

// Platform/Presentation supplies only the stable Action sample. Gameplay attaches
// the authoritative actor and targeting context. The mapper converts ActionId to
// a slot; CombatResolver alone resolves the actor-owned Ability.
struct CombatActionIntent final {
    contracts::ScalarActionSample sample{};
    contracts::TickIndex tick{};
    contracts::StableActorKey actor{};
    contracts::CommandSequence sequence{};
    contracts::StableActorKey requested_target{};
};

struct CombatActionIntentResult final {
    CombatActionIntentError error{CombatActionIntentError::none};
    contracts::CombatCommand command{};
    bool has_command{};
};

class DeterministicCombatActionIntentMapper final {
  public:
    [[nodiscard]] CombatActionIntentResult resolve(
        const CombatActionIntent& intent
    ) noexcept;

    [[nodiscard]] contracts::PlatformSequence last_platform_sequence() const noexcept;

  private:
    contracts::PlatformSequence last_platform_sequence_{};
};

}  // namespace tgd::gameplay
