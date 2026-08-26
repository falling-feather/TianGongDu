#include <tgd/runtime/runtime_facade.hpp>

namespace tgd::runtime {

RuntimeError RuntimeFacade::initialize() noexcept {
    if (lifecycle_ == RuntimeLifecycle::ready) {
        return RuntimeError::already_initialized;
    }

    lifecycle_ = RuntimeLifecycle::ready;
    return RuntimeError::none;
}

RuntimeError RuntimeFacade::shutdown() noexcept {
    if (lifecycle_ != RuntimeLifecycle::ready) {
        return RuntimeError::not_initialized;
    }

    lifecycle_ = RuntimeLifecycle::stopped;
    return RuntimeError::none;
}

RuntimeLifecycle RuntimeFacade::lifecycle() const noexcept {
    return lifecycle_;
}

}  // namespace tgd::runtime
