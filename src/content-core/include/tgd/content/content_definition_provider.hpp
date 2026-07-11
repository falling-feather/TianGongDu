#pragma once

#include <tgd/contracts/content_definition.hpp>

namespace tgd::content {

class IContentDefinitionProvider {
  public:
    virtual ~IContentDefinitionProvider() = default;

    [[nodiscard]] virtual const contracts::VerticalSliceDefinition* find_vertical_slice(
        contracts::StableContentKey id
    ) const noexcept = 0;
};

class BuiltInF1ContentDefinitionProvider final : public IContentDefinitionProvider {
  public:
    [[nodiscard]] const contracts::VerticalSliceDefinition* find_vertical_slice(
        contracts::StableContentKey id
    ) const noexcept override;
};

}  // namespace tgd::content
