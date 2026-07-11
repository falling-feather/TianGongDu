#pragma once

#include <axmol.h>

#include <tgd/presentation/presentation_lifecycle.hpp>
#include <tgd/runtime/runtime_facade.hpp>

#include <cstdint>
#include <string_view>

class AppDelegate final : private ax::Application {
  public:
    AppDelegate();
    ~AppDelegate() override;

    void initGfxContextAttrs() override;
    bool applicationDidFinishLaunching() override;
    void applicationDidEnterBackground() override;
    void applicationWillEnterForeground() override;
    void applicationWillQuit() override;

    void webVisibilityChanged(bool hidden) noexcept;
    void webFocusChanged(bool focused) noexcept;
    void webContextLost() noexcept;
    void webContextRestored() noexcept;
    [[nodiscard]] int webPresentationState() const noexcept;

    [[nodiscard]] static AppDelegate* active() noexcept;

  private:
    [[nodiscard]] tgd::presentation::PresentationError synchronizeSuspension() noexcept;
    void pausePresentationOutput() noexcept;
    void resumePresentationOutputIfEligible() noexcept;
    void trace(std::string_view event, std::string_view result) noexcept;

    static AppDelegate* active_;

    tgd::runtime::RuntimeFacade runtime_;
    tgd::presentation::PresentationLifecycle presentation_;
    std::uint64_t traceSequence_{0};
    bool pageHidden_{false};
    bool pageFocused_{true};
    bool suspendedForPage_{false};
    bool graphicsContextLost_{false};
    bool presentationOutputActive_{false};
};
