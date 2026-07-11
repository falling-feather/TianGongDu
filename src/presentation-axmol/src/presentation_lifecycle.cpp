#include <tgd/presentation/presentation_lifecycle.hpp>

#include <tgd/runtime/runtime_facade.hpp>

namespace tgd::presentation {

PresentationError PresentationLifecycle::start(const runtime::RuntimeFacade& runtime) noexcept {
    if (runtime.lifecycle() != runtime::RuntimeLifecycle::ready) {
        return PresentationError::runtime_not_ready;
    }
    if (state_ != PresentationState::stopped) {
        return PresentationError::invalid_transition;
    }

    state_ = PresentationState::running;
    return PresentationError::none;
}

PresentationError PresentationLifecycle::suspend() noexcept {
    if (state_ == PresentationState::context_lost && state_before_context_loss_ == PresentationState::running) {
        state_before_context_loss_ = PresentationState::suspended;
        return PresentationError::none;
    }
    if (state_ != PresentationState::running) {
        return PresentationError::invalid_transition;
    }

    state_ = PresentationState::suspended;
    return PresentationError::none;
}

PresentationError PresentationLifecycle::resume() noexcept {
    if (state_ == PresentationState::context_lost && state_before_context_loss_ == PresentationState::suspended) {
        state_before_context_loss_ = PresentationState::running;
        return PresentationError::none;
    }
    if (state_ != PresentationState::suspended) {
        return PresentationError::invalid_transition;
    }

    state_ = PresentationState::running;
    return PresentationError::none;
}

PresentationError PresentationLifecycle::context_lost() noexcept {
    if (state_ != PresentationState::running && state_ != PresentationState::suspended) {
        return PresentationError::invalid_transition;
    }

    state_before_context_loss_ = state_;
    state_ = PresentationState::context_lost;
    return PresentationError::none;
}

PresentationError PresentationLifecycle::context_restored() noexcept {
    if (state_ != PresentationState::context_lost) {
        return PresentationError::invalid_transition;
    }

    state_ = state_before_context_loss_;
    state_before_context_loss_ = PresentationState::stopped;
    return PresentationError::none;
}

PresentationError PresentationLifecycle::stop() noexcept {
    if (state_ == PresentationState::stopped) {
        return PresentationError::invalid_transition;
    }

    state_ = PresentationState::stopped;
    state_before_context_loss_ = PresentationState::stopped;
    return PresentationError::none;
}

PresentationState PresentationLifecycle::state() const noexcept {
    return state_;
}

}  // namespace tgd::presentation
