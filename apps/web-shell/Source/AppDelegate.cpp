#include "AppDelegate.hpp"

#include <tgd/contracts/build_identity.hpp>

#include <emscripten/emscripten.h>

#include <iostream>
#include <string_view>

#if AX_ENABLE_AUDIO
#    include <audio/AudioEngine.h>
#endif

namespace {

constexpr float designWidth = 1280.0F;
constexpr float designHeight = 720.0F;

constexpr std::string_view toString(tgd::runtime::RuntimeLifecycle state) noexcept {
    using tgd::runtime::RuntimeLifecycle;
    switch (state) {
        case RuntimeLifecycle::cold:
            return "cold";
        case RuntimeLifecycle::ready:
            return "ready";
        case RuntimeLifecycle::stopped:
            return "stopped";
    }
    return "unknown";
}

constexpr std::string_view toString(tgd::runtime::RuntimeError error) noexcept {
    using tgd::runtime::RuntimeError;
    switch (error) {
        case RuntimeError::none:
            return "none";
        case RuntimeError::already_initialized:
            return "already_initialized";
        case RuntimeError::not_initialized:
            return "not_initialized";
    }
    return "unknown";
}

constexpr std::string_view toString(tgd::presentation::PresentationState state) noexcept {
    using tgd::presentation::PresentationState;
    switch (state) {
        case PresentationState::stopped:
            return "stopped";
        case PresentationState::running:
            return "running";
        case PresentationState::suspended:
            return "suspended";
        case PresentationState::context_lost:
            return "context_lost";
    }
    return "unknown";
}

constexpr std::string_view toString(tgd::presentation::PresentationError error) noexcept {
    using tgd::presentation::PresentationError;
    switch (error) {
        case PresentationError::none:
            return "none";
        case PresentationError::runtime_not_ready:
            return "runtime_not_ready";
        case PresentationError::invalid_transition:
            return "invalid_transition";
    }
    return "unknown";
}

ax::Scene* createBootstrapScene() {
    auto* scene = ax::Scene::create();
    auto* background = ax::LayerColor::create(ax::Color4B(8, 17, 24, 255), designWidth, designHeight);
    scene->addChild(background);

    auto* horizon = ax::LayerColor::create(ax::Color4B(27, 67, 73, 255), designWidth, 180.0F);
    horizon->setPosition(ax::Vec2(0.0F, 0.0F));
    scene->addChild(horizon);

    auto* bridge = ax::LayerColor::create(ax::Color4B(208, 157, 85, 255), 620.0F, 20.0F);
    bridge->setPosition(ax::Vec2(330.0F, 235.0F));
    scene->addChild(bridge);

    auto* gate = ax::LayerColor::create(ax::Color4B(75, 118, 121, 255), 110.0F, 250.0F);
    gate->setPosition(ax::Vec2(585.0F, 255.0F));
    scene->addChild(gate);
    return scene;
}

}  // namespace

AppDelegate* AppDelegate::active_ = nullptr;

AppDelegate::AppDelegate() {
    active_ = this;
}

AppDelegate::~AppDelegate() {
    if (active_ == this) {
        active_ = nullptr;
    }
}

AppDelegate* AppDelegate::active() noexcept {
    return active_;
}

void AppDelegate::initGfxContextAttrs() {
    GfxContextAttrs attributes = {8, 8, 8, 8, 24, 8, 0};
    ax::RenderView::setGfxContextAttrs(attributes);
}

bool AppDelegate::applicationDidFinishLaunching() {
    const auto runtimeResult = runtime_.initialize();
    trace("runtime.initialize", toString(runtimeResult));
    if (runtimeResult != tgd::runtime::RuntimeError::none) {
        return false;
    }

    const auto presentationResult = presentation_.start(runtime_);
    trace("presentation.start", toString(presentationResult));
    if (presentationResult != tgd::presentation::PresentationError::none) {
        static_cast<void>(runtime_.shutdown());
        return false;
    }

    auto* director = ax::Director::getInstance();
    auto* renderView = director->getRenderView();
    if (renderView == nullptr) {
        renderView = ax::RenderViewImpl::createWithRect(
            "TianGongDu F1 Host",
            ax::Rect(0.0F, 0.0F, designWidth, designHeight)
        );
        director->setRenderView(renderView);
    }

#ifndef NDEBUG
    director->setStatsDisplay(true);
#else
    director->setStatsDisplay(false);
#endif
    director->setAnimationInterval(1.0F / 60.0F);
    renderView->setDesignResolutionSize(designWidth, designHeight, ResolutionPolicy::SHOW_ALL);
    director->runWithScene(createBootstrapScene());
    presentationOutputActive_ = true;
    trace("host.ready", "none");
    return true;
}

void AppDelegate::applicationDidEnterBackground() {
    if (pageHidden_) {
        return;
    }
    pageHidden_ = true;
    trace("page.hidden", toString(synchronizeSuspension()));
    pausePresentationOutput();
}

void AppDelegate::applicationWillEnterForeground() {
    if (!pageHidden_) {
        return;
    }
    pageHidden_ = false;
    trace("page.visible", toString(synchronizeSuspension()));
    resumePresentationOutputIfEligible();
}

void AppDelegate::applicationWillQuit() {
    if (presentation_.state() != tgd::presentation::PresentationState::stopped) {
        trace("presentation.stop", toString(presentation_.stop()));
    }
    if (runtime_.lifecycle() == tgd::runtime::RuntimeLifecycle::ready) {
        trace("runtime.shutdown", toString(runtime_.shutdown()));
    }
}

void AppDelegate::webFocusChanged(bool focused) noexcept {
    pageFocused_ = focused;
    trace(focused ? "page.focus" : "page.blur", toString(synchronizeSuspension()));
    if (focused) {
        resumePresentationOutputIfEligible();
    } else {
        pausePresentationOutput();
    }
}

void AppDelegate::webVisibilityChanged(bool hidden) noexcept {
    if (hidden) {
        applicationDidEnterBackground();
    } else {
        applicationWillEnterForeground();
    }
}

void AppDelegate::webContextLost() noexcept {
    graphicsContextLost_ = true;
    pausePresentationOutput();
    trace("webgl.context_lost", toString(presentation_.context_lost()));
}

void AppDelegate::webContextRestored() noexcept {
    const auto result = presentation_.context_restored();
    graphicsContextLost_ = false;
    trace("webgl.context_restored", toString(result));
    if (result == tgd::presentation::PresentationError::none) {
        resumePresentationOutputIfEligible();
    }
}

int AppDelegate::webPresentationState() const noexcept {
    return static_cast<int>(presentation_.state());
}

tgd::presentation::PresentationError AppDelegate::synchronizeSuspension() noexcept {
    const bool shouldSuspend = pageHidden_ || !pageFocused_;
    if (shouldSuspend == suspendedForPage_) {
        return tgd::presentation::PresentationError::none;
    }

    const auto result = shouldSuspend ? presentation_.suspend() : presentation_.resume();
    if (result == tgd::presentation::PresentationError::none) {
        suspendedForPage_ = shouldSuspend;
    }
    return result;
}

void AppDelegate::pausePresentationOutput() noexcept {
    if (!presentationOutputActive_) {
        return;
    }
    ax::Director::getInstance()->stopAnimation();
#if AX_ENABLE_AUDIO
    ax::AudioEngine::pauseAll();
#endif
    presentationOutputActive_ = false;
}

void AppDelegate::resumePresentationOutputIfEligible() noexcept {
    if (presentationOutputActive_ || pageHidden_ || !pageFocused_ || graphicsContextLost_) {
        return;
    }
    ax::Director::getInstance()->startAnimation();
#if AX_ENABLE_AUDIO
    ax::AudioEngine::resumeAll();
#endif
    presentationOutputActive_ = true;
}

void AppDelegate::trace(std::string_view event, std::string_view result) noexcept {
    const auto identity = tgd::contracts::current_build_identity();
    std::cout << "[tgd.lifecycle] {\"sequence\":" << ++traceSequence_ << ",\"event\":\"" << event
              << "\",\"result\":\"" << result << "\",\"runtime\":\"" << toString(runtime_.lifecycle())
              << "\",\"presentation\":\"" << toString(presentation_.state()) << "\",\"hidden\":"
              << (pageHidden_ ? "true" : "false") << ",\"focused\":" << (pageFocused_ ? "true" : "false")
              << ",\"version\":\"" << identity.semantic_version << "\",\"commit\":\"" << identity.git_commit
              << "\",\"channel\":\"" << identity.channel << "\"}" << std::endl;
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void tgd_web_visibility_changed(int hidden) {
    if (auto* app = AppDelegate::active(); app != nullptr) {
        app->webVisibilityChanged(hidden != 0);
    }
}

EMSCRIPTEN_KEEPALIVE void tgd_web_focus_changed(int focused) {
    if (auto* app = AppDelegate::active(); app != nullptr) {
        app->webFocusChanged(focused != 0);
    }
}

EMSCRIPTEN_KEEPALIVE void tgd_web_context_lost() {
    if (auto* app = AppDelegate::active(); app != nullptr) {
        app->webContextLost();
    }
}

EMSCRIPTEN_KEEPALIVE void tgd_web_context_restored() {
    if (auto* app = AppDelegate::active(); app != nullptr) {
        app->webContextRestored();
    }
}

EMSCRIPTEN_KEEPALIVE int tgd_web_presentation_state() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webPresentationState();
    }
    return -1;
}

}  // extern "C"
