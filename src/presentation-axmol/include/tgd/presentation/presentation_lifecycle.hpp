#pragma once

#include <cstdint>

namespace tgd::runtime {
class RuntimeFacade;
}

namespace tgd::presentation {

enum class PresentationState : std::uint8_t {
    stopped,
    running,
    suspended,
    context_lost,
};

enum class PresentationError : std::uint8_t {
    none,
    runtime_not_ready,
    invalid_transition,
};

class PresentationLifecycle final {
  public:
    [[nodiscard]] PresentationError start(const runtime::RuntimeFacade& runtime) noexcept;
    [[nodiscard]] PresentationError suspend() noexcept;
    [[nodiscard]] PresentationError resume() noexcept;
    [[nodiscard]] PresentationError context_lost() noexcept;
    [[nodiscard]] PresentationError context_restored() noexcept;
    [[nodiscard]] PresentationError stop() noexcept;
    [[nodiscard]] PresentationState state() const noexcept;

  private:
    PresentationState state_{PresentationState::stopped};
    PresentationState state_before_context_loss_{PresentationState::stopped};
};

}  // namespace tgd::presentation
