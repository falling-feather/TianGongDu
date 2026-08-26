#include <tgd/contracts/build_identity.hpp>

namespace tgd::content::detail {

[[maybe_unused]] bool contracts_are_available() noexcept {
    return !contracts::current_build_identity().semantic_version.empty();
}

}  // namespace tgd::content::detail
