#include <tgd/content/content_definition_provider.hpp>
#include <tgd/content/f1_vertical_slice.generated.hpp>

namespace tgd::content {

const contracts::VerticalSliceDefinition* BuiltInF1ContentDefinitionProvider::find_vertical_slice(
    contracts::StableContentKey id
) const noexcept {
    if (id == generated::f1_vertical_slice_definition.id.key) {
        return &generated::f1_vertical_slice_definition;
    }
    return nullptr;
}

const contracts::CombatEncounterDefinition*
BuiltInF1ContentDefinitionProvider::find_combat_encounter(
    contracts::StableContentKey id
) const noexcept {
    if (id == generated::f1_combat_encounter_definition.id.key) {
        return &generated::f1_combat_encounter_definition;
    }
    return nullptr;
}

}  // namespace tgd::content
