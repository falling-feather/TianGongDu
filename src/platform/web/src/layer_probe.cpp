#include <tgd/contracts/build_identity.hpp>
#include <tgd/runtime/runtime_facade.hpp>

namespace tgd::platform::web::detail {

[[maybe_unused]] bool layer_dependencies_are_available() noexcept {
    const auto identity = contracts::current_build_identity();
    runtime::RuntimeFacade runtime;
    return !identity.channel.empty() && runtime.lifecycle() == runtime::RuntimeLifecycle::cold;
}

}  // namespace tgd::platform::web::detail
