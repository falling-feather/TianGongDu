#pragma once

#include <cstdint>

namespace tgd::runtime {

enum class RuntimeLifecycle : std::uint8_t {
    cold,
    ready,
    stopped,
};

enum class RuntimeError : std::uint8_t {
    none,
    already_initialized,
    not_initialized,
};

class RuntimeFacade final {
  public:
    [[nodiscard]] RuntimeError initialize() noexcept;
    [[nodiscard]] RuntimeError shutdown() noexcept;
    [[nodiscard]] RuntimeLifecycle lifecycle() const noexcept;

  private:
    RuntimeLifecycle lifecycle_{RuntimeLifecycle::cold};
};

}  // namespace tgd::runtime
