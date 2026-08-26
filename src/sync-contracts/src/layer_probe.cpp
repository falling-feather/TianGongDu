#include <tgd/contracts/build_identity.hpp>

namespace tgd::sync::detail {

[[maybe_unused]] bool contracts_are_available() noexcept {
    return !contracts::current_build_identity().git_commit.empty();
}

}  // namespace tgd::sync::detail
