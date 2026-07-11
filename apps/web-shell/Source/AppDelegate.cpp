#include "AppDelegate.hpp"
#include "F1GrayboxLayer.hpp"

#include <tgd/contracts/build_identity.hpp>
#include <tgd/contracts/tgd_web_abi.h>

#include <emscripten/emscripten.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <new>
#include <span>
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

constexpr std::string_view toString(tgd::runtime::ProfileStorageError error) noexcept {
    using tgd::runtime::ProfileStorageError;
    switch (error) {
        case ProfileStorageError::none:
            return "none";
        case ProfileStorageError::invalid_config:
            return "invalid_config";
        case ProfileStorageError::invalid_state:
            return "invalid_state";
        case ProfileStorageError::invalid_snapshot:
            return "invalid_snapshot";
        case ProfileStorageError::allocation_failed:
            return "allocation_failed";
        case ProfileStorageError::backpressure:
            return "backpressure";
        case ProfileStorageError::storage_unavailable:
            return "storage_unavailable";
        case ProfileStorageError::storage_conflict:
            return "storage_conflict";
        case ProfileStorageError::storage_quota:
            return "storage_quota";
        case ProfileStorageError::storage_corrupt:
            return "storage_corrupt";
        case ProfileStorageError::cancelled:
            return "cancelled";
        case ProfileStorageError::timeout:
            return "timeout";
        case ProfileStorageError::protocol_violation:
            return "protocol_violation";
        case ProfileStorageError::internal:
            return "internal";
    }
    return "unknown";
}

constexpr tgd::platform::web::WebAbiError
toWebError(tgd::runtime::ProfileStorageError error) noexcept {
    using tgd::platform::web::WebAbiError;
    using tgd::runtime::ProfileStorageError;
    switch (error) {
        case ProfileStorageError::none:
            return WebAbiError::none;
        case ProfileStorageError::invalid_config:
        case ProfileStorageError::invalid_state:
        case ProfileStorageError::invalid_snapshot:
        case ProfileStorageError::protocol_violation:
            return WebAbiError::invalid_message;
        case ProfileStorageError::allocation_failed:
        case ProfileStorageError::internal:
            return WebAbiError::internal;
        case ProfileStorageError::backpressure:
            return WebAbiError::backpressure;
        case ProfileStorageError::storage_unavailable:
            return WebAbiError::storage_unavailable;
        case ProfileStorageError::storage_conflict:
            return WebAbiError::storage_conflict;
        case ProfileStorageError::storage_quota:
            return WebAbiError::storage_quota;
        case ProfileStorageError::storage_corrupt:
            return WebAbiError::storage_corrupt;
        case ProfileStorageError::cancelled:
            return WebAbiError::cancelled;
        case ProfileStorageError::timeout:
            return WebAbiError::timeout;
    }
    return WebAbiError::internal;
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
    auto* scene = createF1GrayboxScene(&grayboxLayer_);
    if (scene == nullptr) {
        static_cast<void>(presentation_.stop());
        static_cast<void>(runtime_.shutdown());
        return false;
    }
    director->runWithScene(scene);
    presentationOutputActive_ = true;
    trace("host.ready", "none");
    return true;
}

void AppDelegate::applicationDidEnterBackground() {
    if (pageHidden_) {
        return;
    }
    pageHidden_ = true;
    if (grayboxLayer_ != nullptr) {
        grayboxLayer_->clearInput(tgd::contracts::InputClearReason::visibility_hidden);
    }
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
    if (grayboxLayer_ != nullptr) {
        grayboxLayer_->shutdown();
        grayboxLayer_ = nullptr;
    }
    if (presentation_.state() != tgd::presentation::PresentationState::stopped) {
        trace("presentation.stop", toString(presentation_.stop()));
    }
    if (runtime_.lifecycle() == tgd::runtime::RuntimeLifecycle::ready) {
        trace("runtime.shutdown", toString(runtime_.shutdown()));
    }
}

void AppDelegate::webFocusChanged(bool focused) noexcept {
    pageFocused_ = focused;
    if (!focused && grayboxLayer_ != nullptr) {
        grayboxLayer_->clearInput(tgd::contracts::InputClearReason::blur);
    }
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
    if (grayboxLayer_ != nullptr) {
        grayboxLayer_->clearInput(tgd::contracts::InputClearReason::device_disconnected);
    }
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

std::int32_t AppDelegate::webBoot(std::span<const std::uint8_t> message) noexcept {
    using tgd::platform::web::WebAbiError;

    if (profileBooted_) {
        return static_cast<std::int32_t>(WebAbiError::invalid_message);
    }
    tgd::platform::web::WebBootConfig config;
    const auto decoded = tgd::platform::web::WebPlatformBridge::decode_boot_config(message, config);
    if (decoded != WebAbiError::none) {
        return static_cast<std::int32_t>(decoded);
    }

    webPlatform_.reset();
    const auto initialized =
        profileStorage_.initialize(webPlatform_, {
                                                     config.profile_id,
                                                     config.package_set_id,
                                                     config.request_id_seed,
                                                     config.session_generation,
                                                     tgd::runtime::StorageChannel::prototype_f1,
                                                 });
    if (initialized != tgd::runtime::ProfileStorageError::none) {
        trace("profile.initialize", toString(initialized));
        return static_cast<std::int32_t>(toWebError(initialized));
    }

    webBootConfig_ = config;
    profileBooted_ = true;
    const auto restore = profileStorage_.begin_restore();
    trace("profile.restore.begin", toString(restore));
    publishProfileUi();
    return static_cast<std::int32_t>(toWebError(restore));
}

std::int32_t AppDelegate::webSubmitUiCommand(std::span<const std::uint8_t> message) noexcept {
    using tgd::platform::web::WebAbiError;

    if (!profileBooted_) {
        return static_cast<std::int32_t>(WebAbiError::invalid_message);
    }
    tgd::platform::web::WebUiCommand command;
    const auto decoded = tgd::platform::web::WebPlatformBridge::decode_ui_command(message, command);
    if (decoded != WebAbiError::none) {
        return static_cast<std::int32_t>(decoded);
    }
    if (command.session_generation != webBootConfig_.session_generation) {
        return static_cast<std::int32_t>(WebAbiError::stale_generation);
    }
    if (command.type == tgd::platform::web::WebUiCommandType::retry_pending_save) {
        const auto retried = profileStorage_.retry_pending_save();
        trace("profile.save.retry", toString(retried));
        publishProfileUi();
        return static_cast<std::int32_t>(toWebError(retried));
    }

    const auto current_sequence =
        profileStorage_.has_snapshot() ? profileStorage_.current_head().logical_sequence : 0;
    if (current_sequence == std::numeric_limits<std::uint64_t>::max()) {
        return static_cast<std::int32_t>(WebAbiError::invalid_message);
    }

    tgd::contracts::SaveEnvelopeV1 snapshot;
    snapshot.profile_id = webBootConfig_.profile_id;
    snapshot.snapshot_id = command.command_id;
    snapshot.parent_snapshot_id = profileStorage_.has_snapshot()
                                      ? profileStorage_.current_head().snapshot_id
                                      : tgd::contracts::StableId128{};
    snapshot.package_set_id = webBootConfig_.package_set_id;
    snapshot.created_logical_sequence = current_sequence + 1;
    snapshot.checkpoint_kind = command.checkpoint_kind;
    try {
        constexpr std::string_view payload = "tgd.f1.guest.profile.checkpoint.v1";
        snapshot.payload.assign(payload.begin(), payload.end());
        for (std::size_t index = 0; index < sizeof(snapshot.created_logical_sequence); ++index) {
            snapshot.payload.push_back(static_cast<std::uint8_t>(
                snapshot.created_logical_sequence >> static_cast<unsigned>(index * 8U)));
        }
    } catch (const std::bad_alloc&) {
        return static_cast<std::int32_t>(WebAbiError::internal);
    }

    const auto saved =
        profileStorage_.begin_save(snapshot, tgd::runtime::StorageDurability::strict_if_supported);
    trace("profile.save.begin", toString(saved));
    publishProfileUi();
    return static_cast<std::int32_t>(toWebError(saved));
}

std::uint32_t AppDelegate::webPeekPlatformRequestSize() const noexcept {
    return webPlatform_.peek_platform_request_size();
}

std::int32_t AppDelegate::webPollPlatformRequest(std::span<std::uint8_t> output) noexcept {
    return webPlatform_.poll_platform_request(output);
}

std::int32_t AppDelegate::webCompleteAsyncRequest(std::span<const std::uint8_t> message) noexcept {
    const auto accepted = webPlatform_.accept_async_completion(message);
    if (accepted != tgd::platform::web::WebAbiError::none) {
        return static_cast<std::int32_t>(accepted);
    }

    const auto pumped = profileStorage_.pump();
    if (pumped.completion_consumed) {
        trace("profile.storage.complete", toString(pumped.error));
        publishProfileUi();
    }
    return static_cast<std::int32_t>(tgd::platform::web::WebAbiError::none);
}

std::uint32_t AppDelegate::webPeekUiEventSize() const noexcept {
    return webPlatform_.peek_ui_event_size();
}

std::int32_t AppDelegate::webPollUiEvent(std::span<std::uint8_t> output) noexcept {
    return webPlatform_.poll_ui_event(output);
}

void AppDelegate::webRequestShutdown() noexcept { applicationWillQuit(); }

void AppDelegate::publishProfileUi() noexcept {
    tgd::platform::web::WebProfileUiEvent event;
    event.state = static_cast<std::uint16_t>(profileStorage_.state());
    event.error = static_cast<std::uint16_t>(profileStorage_.last_error());
    event.has_snapshot = profileStorage_.has_snapshot();
    event.has_pending_save = profileStorage_.has_pending_save();
    event.committed_save_count = profileStorage_.committed_save_count();
    if (event.has_snapshot) {
        event.logical_sequence = profileStorage_.current_head().logical_sequence;
        event.snapshot_id = profileStorage_.current_head().snapshot_id;
    }
    webPlatform_.publish_profile_ui(webBootConfig_.session_generation, event);
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

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_abi_version() {
    return (static_cast<std::uint32_t>(TGD_WEB_ABI_MAJOR) << 16U) |
           static_cast<std::uint32_t>(TGD_WEB_ABI_MINOR);
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_boot(const std::uint8_t* bytes, std::uint32_t length) {
    if ((bytes == nullptr && length != 0U) || length > TGD_WEB_ABI_MAX_MESSAGE_BYTES) {
        return TGD_WEB_ERROR_INVALID_MESSAGE;
    }
    if (auto* app = AppDelegate::active(); app != nullptr) {
        return app->webBoot({bytes, length});
    }
    return TGD_WEB_ERROR_INTERNAL;
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_submit_platform_event(const std::uint8_t* bytes,
                                                                std::uint32_t length) {
    if ((bytes == nullptr && length != 0U) || length > TGD_WEB_ABI_MAX_MESSAGE_BYTES) {
        return TGD_WEB_ERROR_INVALID_MESSAGE;
    }
    return TGD_WEB_ERROR_UNKNOWN_MESSAGE_TYPE;
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_submit_ui_command(const std::uint8_t* bytes,
                                                            std::uint32_t length) {
    if ((bytes == nullptr && length != 0U) || length > TGD_WEB_ABI_MAX_MESSAGE_BYTES) {
        return TGD_WEB_ERROR_INVALID_MESSAGE;
    }
    if (auto* app = AppDelegate::active(); app != nullptr) {
        return app->webSubmitUiCommand({bytes, length});
    }
    return TGD_WEB_ERROR_INTERNAL;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_peek_platform_request_size() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webPeekPlatformRequestSize();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_poll_platform_request(std::uint8_t* output,
                                                                std::uint32_t capacity) {
    if (output == nullptr && capacity != 0U) {
        return TGD_WEB_ERROR_INVALID_MESSAGE;
    }
    if (auto* app = AppDelegate::active(); app != nullptr) {
        return app->webPollPlatformRequest({output, capacity});
    }
    return TGD_WEB_ERROR_INTERNAL;
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_complete_async_request(const std::uint8_t* bytes,
                                                                 std::uint32_t length) {
    if ((bytes == nullptr && length != 0U) || length > TGD_WEB_ABI_MAX_MESSAGE_BYTES) {
        return TGD_WEB_ERROR_INVALID_MESSAGE;
    }
    if (auto* app = AppDelegate::active(); app != nullptr) {
        return app->webCompleteAsyncRequest({bytes, length});
    }
    return TGD_WEB_ERROR_INTERNAL;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_peek_ui_event_size() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webPeekUiEventSize();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_poll_ui_event(std::uint8_t* output,
                                                        std::uint32_t capacity) {
    if (output == nullptr && capacity != 0U) {
        return TGD_WEB_ERROR_INVALID_MESSAGE;
    }
    if (auto* app = AppDelegate::active(); app != nullptr) {
        return app->webPollUiEvent({output, capacity});
    }
    return TGD_WEB_ERROR_INTERNAL;
}

EMSCRIPTEN_KEEPALIVE void tgd_web_request_shutdown() {
    if (auto* app = AppDelegate::active(); app != nullptr) {
        app->webRequestShutdown();
    }
}

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
