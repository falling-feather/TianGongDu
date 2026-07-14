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

constexpr std::string_view toString(
    tgd::runtime::ProfileProgressCoordinatorError error
) noexcept {
    using tgd::runtime::ProfileProgressCoordinatorError;
    switch (error) {
        case ProfileProgressCoordinatorError::none:
            return "none";
        case ProfileProgressCoordinatorError::invalid_config:
            return "invalid_config";
        case ProfileProgressCoordinatorError::invalid_state:
            return "invalid_state";
        case ProfileProgressCoordinatorError::invalid_snapshot:
            return "invalid_snapshot";
        case ProfileProgressCoordinatorError::invalid_progress:
            return "invalid_progress";
        case ProfileProgressCoordinatorError::invalid_claim:
            return "invalid_claim";
        case ProfileProgressCoordinatorError::allocation_failed:
            return "allocation_failed";
        case ProfileProgressCoordinatorError::revision_overflow:
            return "revision_overflow";
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

constexpr tgd::platform::web::WebAbiError toWebError(
    tgd::runtime::ProfileProgressCoordinatorError error
) noexcept {
    using tgd::platform::web::WebAbiError;
    using tgd::runtime::ProfileProgressCoordinatorError;
    switch (error) {
        case ProfileProgressCoordinatorError::none:
            return WebAbiError::none;
        case ProfileProgressCoordinatorError::invalid_config:
        case ProfileProgressCoordinatorError::invalid_state:
        case ProfileProgressCoordinatorError::invalid_snapshot:
        case ProfileProgressCoordinatorError::invalid_progress:
        case ProfileProgressCoordinatorError::invalid_claim:
        case ProfileProgressCoordinatorError::revision_overflow:
            return WebAbiError::invalid_message;
        case ProfileProgressCoordinatorError::allocation_failed:
            return WebAbiError::internal;
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
    auto* scene = createF1GrayboxScene(&grayboxLayer_, this);
    if (scene == nullptr) {
        static_cast<void>(presentation_.stop());
        static_cast<void>(runtime_.shutdown());
        return false;
    }
    grayboxLayer_->setQuestUiProjectionSink(this);
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

std::int32_t AppDelegate::webF1QaPlayerHealth() const noexcept {
    return grayboxLayer_ == nullptr ? -1 : grayboxLayer_->qaPlayerHealth();
}

int AppDelegate::webF1QaPlayerActive() const noexcept {
    return grayboxLayer_ != nullptr && grayboxLayer_->qaPlayerActive() ? 1 : 0;
}

std::uint32_t AppDelegate::webF1QaActiveHostiles() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaActiveHostiles();
}

std::uint32_t AppDelegate::webF1QaRetryCount() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaRetryCount();
}

std::uint32_t AppDelegate::webF1QaQuestBeatIndex() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaQuestBeatIndex();
}

std::uint32_t AppDelegate::webF1QaQuestCompletedObjectives() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaQuestCompletedObjectives();
}

std::uint32_t AppDelegate::webF1QaQuestRequiredObjectives() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaQuestRequiredObjectives();
}

std::uint32_t AppDelegate::webF1QaQuestSelectedChoices() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaQuestSelectedChoices();
}

int AppDelegate::webF1QaQuestResolved() const noexcept {
    return grayboxLayer_ != nullptr && grayboxLayer_->qaQuestResolved() ? 1 : 0;
}

int AppDelegate::webF1QaResolutionRewardReady() const noexcept {
    return grayboxLayer_ != nullptr && grayboxLayer_->qaResolutionRewardReady() ? 1 : 0;
}

int AppDelegate::webF1QaRewardClaimCommitted() const noexcept {
    return grayboxLayer_ != nullptr && grayboxLayer_->qaResolutionRewardCommitted() ? 1 : 0;
}

std::uint32_t AppDelegate::webF1QaProfileOperationCount() const noexcept {
    if (!profileProgressReady_) {
        return 0;
    }
    return profileProgress_.committed_operation_count() >
                   std::numeric_limits<std::uint32_t>::max()
               ? std::numeric_limits<std::uint32_t>::max()
               : static_cast<std::uint32_t>(profileProgress_.committed_operation_count());
}

int AppDelegate::webF1QaReplayRewardClaim() noexcept {
    if (!observedRewardClaim_.valid()) {
        return 0;
    }
    submitF1RewardClaim(observedRewardClaim_);
    return 1;
}

int AppDelegate::webF1QaSubmitInvalidRewardClaim() noexcept {
    submitF1RewardClaim({});
    return 1;
}

std::int32_t AppDelegate::webF1QaSafePointPoseX() const noexcept {
    return grayboxLayer_ == nullptr ? 0 : grayboxLayer_->qaSafePointPoseX();
}

std::int32_t AppDelegate::webF1QaSafePointPoseY() const noexcept {
    return grayboxLayer_ == nullptr ? 0 : grayboxLayer_->qaSafePointPoseY();
}

std::int32_t AppDelegate::webF1QaPlayerPoseX() const noexcept {
    return grayboxLayer_ == nullptr ? 0 : grayboxLayer_->qaPlayerPoseX();
}

std::int32_t AppDelegate::webF1QaPlayerPoseY() const noexcept {
    return grayboxLayer_ == nullptr ? 0 : grayboxLayer_->qaPlayerPoseY();
}

std::uint32_t AppDelegate::webF1QaEligiblePlayTicks() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaEligiblePlayTicks();
}

std::uint32_t AppDelegate::webF1QaIdleTicks() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaIdleTicks();
}

std::uint32_t AppDelegate::webF1QaFailureRetryTicks() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaFailureRetryTicks();
}

std::uint32_t AppDelegate::webF1QaBeatTargetsMet() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaBeatTargetsMet();
}

int AppDelegate::webF1QaPlayableTargetMet() const noexcept {
    return grayboxLayer_ != nullptr && grayboxLayer_->qaPlayableTargetMet() ? 1 : 0;
}

std::uint32_t AppDelegate::webF1QaIncomingAttackTicks() const noexcept {
    return grayboxLayer_ == nullptr ? 0U : grayboxLayer_->qaIncomingAttackTicks();
}

int AppDelegate::webF1QaPlayerBusy() const noexcept {
    return grayboxLayer_ != nullptr && grayboxLayer_->qaPlayerBusy() ? 1 : 0;
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
    profileProgress_ = {};
    profileProgressError_ = tgd::runtime::ProfileProgressCoordinatorError::none;
    profileProgressReady_ = false;
    queuedRewardClaim_ = {};
    observedRewardClaim_ = {};
    hasQueuedRewardClaim_ = false;
    hasObservedRewardClaim_ = false;
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

    synchronizeProfileProgress();
    if (!profileProgressReady_) {
        return static_cast<std::int32_t>(
            profileProgressError_ == tgd::runtime::ProfileProgressCoordinatorError::none
                ? WebAbiError::invalid_message
                : toWebError(profileProgressError_)
        );
    }
    const auto prepared =
        profileProgress_.prepare_checkpoint(command.command_id, command.checkpoint_kind);
    profileProgressError_ = prepared.error;
    if (prepared.error != tgd::runtime::ProfileProgressCoordinatorError::none ||
        prepared.disposition != tgd::runtime::ProfileProgressPrepareDisposition::prepared) {
        trace("profile.progress.checkpoint", toString(prepared.error));
        return static_cast<std::int32_t>(toWebError(prepared.error));
    }
    const auto saved = profileStorage_.begin_save(
        profileProgress_.pending_snapshot(),
        tgd::runtime::StorageDurability::strict_if_supported,
        profileProgress_.pending_new_operations()
    );
    if (saved != tgd::runtime::ProfileStorageError::none &&
        !profileStorage_.has_pending_save()) {
        static_cast<void>(profileProgress_.discard_pending());
    }
    trace("profile.save.begin", toString(saved));
    publishProfileUi();
    return static_cast<std::int32_t>(toWebError(saved));
}

std::int32_t AppDelegate::webSubmitQuestUiSelectionIntent(
    std::span<const std::uint8_t> message
) noexcept {
    using tgd::platform::web::WebAbiError;
    if (!profileBooted_ || grayboxLayer_ == nullptr) {
        return static_cast<std::int32_t>(WebAbiError::invalid_message);
    }
    tgd::platform::web::WebQuestUiSelectionIntent selection;
    const auto decoded =
        tgd::platform::web::WebPlatformBridge::decode_quest_ui_selection_intent(
            message,
            selection
        );
    if (decoded != WebAbiError::none) {
        return static_cast<std::int32_t>(decoded);
    }
    if (selection.session_generation != webBootConfig_.session_generation) {
        return static_cast<std::int32_t>(WebAbiError::stale_generation);
    }
    const auto submitted =
        grayboxLayer_->submitQuestUiSelectionIntent(selection.intent);
    if (submitted != tgd::gameplay::QuestUiSelectionIntentError::none) {
        return static_cast<std::int32_t>(WebAbiError::invalid_message);
    }
    const auto closed = webPlatform_.publish_quest_ui_close(
        webBootConfig_.session_generation,
        {
            selection.intent.projection_sequence,
            selection.intent.projection_checksum,
            TGD_WEB_QUEST_UI_CLOSE_SELECTION_COMMITTED,
        }
    );
    if (closed != WebAbiError::none) {
        trace("quest_ui.close", "invalid_acknowledgement");
        return static_cast<std::int32_t>(WebAbiError::internal);
    }
    return static_cast<std::int32_t>(WebAbiError::none);
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
        synchronizeProfileProgress();
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

bool AppDelegate::submitF1QuestUiProjection(
    const tgd::contracts::QuestUiProjectionSnapshot& projection
) noexcept {
    if (!profileBooted_) {
        return false;
    }
    const auto published =
        webPlatform_.publish_quest_ui(webBootConfig_.session_generation, projection);
    if (published != tgd::platform::web::WebAbiError::none) {
        trace("quest_ui.publish", "invalid_projection");
        return false;
    }
    return true;
}

void AppDelegate::synchronizeProfileProgress() noexcept {
    if (!profileBooted_ || profileStorage_.state() != tgd::runtime::ProfileStorageState::ready) {
        return;
    }
    if (!profileProgressReady_) {
        if (profileStorage_.has_snapshot()) {
            const auto decoded =
                tgd::contracts::decode_save_envelope(profileStorage_.current_snapshot_bytes());
            profileProgressError_ =
                decoded.error == tgd::contracts::SaveEnvelopeError::none
                    ? profileProgress_.restore(decoded.envelope)
                    : tgd::runtime::ProfileProgressCoordinatorError::invalid_snapshot;
        } else {
            profileProgressError_ = profileProgress_.initialize(
                webBootConfig_.profile_id,
                webBootConfig_.package_set_id
            );
        }
        profileProgressReady_ =
            profileProgressError_ == tgd::runtime::ProfileProgressCoordinatorError::none;
        trace("profile.progress.restore", toString(profileProgressError_));
        if (!profileProgressReady_) {
            return;
        }
    }

    if (profileProgress_.has_pending() && profileStorage_.has_snapshot() &&
        profileStorage_.current_head().snapshot_id ==
            profileProgress_.pending_snapshot().snapshot_id) {
        profileProgressError_ =
            profileProgress_.accept_commit(profileStorage_.current_head().snapshot_id);
        trace("profile.progress.commit", toString(profileProgressError_));
    }
    notifyObservedRewardClaimIfCommitted();
    tryBeginQueuedRewardClaim();
}

void AppDelegate::tryBeginQueuedRewardClaim() noexcept {
    if (!hasQueuedRewardClaim_ || !profileProgressReady_ ||
        profileStorage_.state() != tgd::runtime::ProfileStorageState::ready) {
        return;
    }
    const auto prepared = profileProgress_.prepare_reward_claim(
        queuedRewardClaim_.source_id,
        queuedRewardClaim_.reward_id,
        queuedRewardClaim_.reward_dedup_key
    );
    profileProgressError_ = prepared.error;
    trace("profile.reward.prepare", toString(prepared.error));
    if (prepared.error != tgd::runtime::ProfileProgressCoordinatorError::none) {
        if (prepared.error == tgd::runtime::ProfileProgressCoordinatorError::invalid_claim) {
            hasQueuedRewardClaim_ = false;
        }
        return;
    }
    if (prepared.disposition ==
        tgd::runtime::ProfileProgressPrepareDisposition::already_committed) {
        hasQueuedRewardClaim_ = false;
        notifyObservedRewardClaimIfCommitted();
        return;
    }
    if (prepared.disposition ==
        tgd::runtime::ProfileProgressPrepareDisposition::already_pending) {
        hasQueuedRewardClaim_ = false;
        return;
    }
    if (prepared.disposition != tgd::runtime::ProfileProgressPrepareDisposition::prepared) {
        return;
    }
    const auto saved = profileStorage_.begin_save(
        profileProgress_.pending_snapshot(),
        tgd::runtime::StorageDurability::strict_if_supported,
        profileProgress_.pending_new_operations()
    );
    trace("profile.reward.save", toString(saved));
    if (saved == tgd::runtime::ProfileStorageError::none ||
        profileStorage_.has_pending_save()) {
        hasQueuedRewardClaim_ = false;
        publishProfileUi();
        return;
    }
    static_cast<void>(profileProgress_.discard_pending());
}

void AppDelegate::notifyObservedRewardClaimIfCommitted() noexcept {
    if (!hasObservedRewardClaim_ || !profileProgressReady_ ||
        !profileProgress_.has_reward_claim(observedRewardClaim_.reward_dedup_key)) {
        return;
    }
    if (grayboxLayer_ != nullptr) {
        grayboxLayer_->notifyRewardClaimCommitted(observedRewardClaim_.reward_dedup_key);
    }
    hasObservedRewardClaim_ = false;
}

void AppDelegate::submitF1RewardClaim(const F1RewardClaim& claim) noexcept {
    if (!claim.valid()) {
        trace("profile.reward.claim", "invalid_claim");
        return;
    }
    if (hasQueuedRewardClaim_ && queuedRewardClaim_ != claim) {
        trace("profile.reward.claim", "conflicting_claim");
        return;
    }
    if (hasObservedRewardClaim_ && observedRewardClaim_ != claim) {
        trace("profile.reward.claim", "conflicting_claim");
        return;
    }
    observedRewardClaim_ = claim;
    hasObservedRewardClaim_ = true;
    queuedRewardClaim_ = claim;
    hasQueuedRewardClaim_ = true;
    trace("profile.reward.claim", "queued");
    synchronizeProfileProgress();
    tryBeginQueuedRewardClaim();
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

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_submit_quest_ui_selection_intent(
    const std::uint8_t* bytes,
    std::uint32_t length
) {
    if ((bytes == nullptr && length != 0U) || length > TGD_WEB_ABI_MAX_MESSAGE_BYTES) {
        return TGD_WEB_ERROR_INVALID_MESSAGE;
    }
    if (auto* app = AppDelegate::active(); app != nullptr) {
        return app->webSubmitQuestUiSelectionIntent({bytes, length});
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

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_f1_qa_player_health() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaPlayerHealth();
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE int tgd_web_f1_qa_player_active() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaPlayerActive();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_active_hostiles() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaActiveHostiles();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_retry_count() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaRetryCount();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_quest_beat_index() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaQuestBeatIndex();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_quest_completed_objectives() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaQuestCompletedObjectives();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_quest_required_objectives() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaQuestRequiredObjectives();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_quest_selected_choices() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaQuestSelectedChoices();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE int tgd_web_f1_qa_quest_resolved() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaQuestResolved();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE int tgd_web_f1_qa_resolution_reward_ready() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaResolutionRewardReady();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE int tgd_web_f1_qa_reward_claim_committed() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaRewardClaimCommitted();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_profile_operation_count() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaProfileOperationCount();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE int tgd_web_f1_qa_replay_reward_claim() {
    if (auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaReplayRewardClaim();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE int tgd_web_f1_qa_submit_invalid_reward_claim() {
    if (auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaSubmitInvalidRewardClaim();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_f1_qa_safe_point_pose_x() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaSafePointPoseX();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_f1_qa_safe_point_pose_y() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaSafePointPoseY();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_f1_qa_player_pose_x() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaPlayerPoseX();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::int32_t tgd_web_f1_qa_player_pose_y() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaPlayerPoseY();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_eligible_play_ticks() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaEligiblePlayTicks();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_idle_ticks() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaIdleTicks();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_failure_retry_ticks() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaFailureRetryTicks();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_beat_targets_met() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaBeatTargetsMet();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE int tgd_web_f1_qa_playable_target_met() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaPlayableTargetMet();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE std::uint32_t tgd_web_f1_qa_incoming_attack_ticks() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaIncomingAttackTicks();
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE int tgd_web_f1_qa_player_busy() {
    if (const auto* app = AppDelegate::active(); app != nullptr) {
        return app->webF1QaPlayerBusy();
    }
    return 0;
}

}  // extern "C"
