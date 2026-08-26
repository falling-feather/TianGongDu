#pragma once

#include "F1QuestUiProjection.hpp"
#include "F1RewardClaim.hpp"

#include <axmol.h>

#include <tgd/platform/web/web_platform_bridge.hpp>
#include <tgd/presentation/presentation_lifecycle.hpp>
#include <tgd/runtime/profile_progress_coordinator.hpp>
#include <tgd/runtime/profile_storage_coordinator.hpp>
#include <tgd/runtime/runtime_facade.hpp>

#include <cstdint>
#include <span>
#include <string_view>

class F1GrayboxLayer;

class AppDelegate final :
    private ax::Application,
    private IF1RewardClaimSink,
    private IF1QuestUiProjectionSink {
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
    [[nodiscard]] std::int32_t webF1QaPlayerHealth() const noexcept;
    [[nodiscard]] int webF1QaPlayerActive() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaActiveHostiles() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaRetryCount() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaQuestBeatIndex() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaQuestCompletedObjectives() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaQuestRequiredObjectives() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaQuestSelectedChoices() const noexcept;
    [[nodiscard]] int webF1QaQuestResolved() const noexcept;
    [[nodiscard]] int webF1QaResolutionRewardReady() const noexcept;
    [[nodiscard]] int webF1QaRewardClaimCommitted() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaProfileOperationCount() const noexcept;
    [[nodiscard]] int webF1QaReplayRewardClaim() noexcept;
    [[nodiscard]] int webF1QaSubmitInvalidRewardClaim() noexcept;
    [[nodiscard]] std::int32_t webF1QaSafePointPoseX() const noexcept;
    [[nodiscard]] std::int32_t webF1QaSafePointPoseY() const noexcept;
    [[nodiscard]] std::int32_t webF1QaPlayerPoseX() const noexcept;
    [[nodiscard]] std::int32_t webF1QaPlayerPoseY() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaEligiblePlayTicks() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaIdleTicks() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaFailureRetryTicks() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaBeatTargetsMet() const noexcept;
    [[nodiscard]] int webF1QaPlayableTargetMet() const noexcept;
    [[nodiscard]] std::uint32_t webF1QaIncomingAttackTicks() const noexcept;
    [[nodiscard]] int webF1QaPlayerBusy() const noexcept;
    [[nodiscard]] std::int32_t webBoot(std::span<const std::uint8_t> message) noexcept;
    [[nodiscard]] std::int32_t webSubmitUiCommand(std::span<const std::uint8_t> message) noexcept;
    [[nodiscard]] std::int32_t
    webSubmitQuestUiSelectionIntent(std::span<const std::uint8_t> message) noexcept;
    [[nodiscard]] std::uint32_t webPeekPlatformRequestSize() const noexcept;
    [[nodiscard]] std::int32_t webPollPlatformRequest(std::span<std::uint8_t> output) noexcept;
    [[nodiscard]] std::int32_t
    webCompleteAsyncRequest(std::span<const std::uint8_t> message) noexcept;
    [[nodiscard]] std::uint32_t webPeekUiEventSize() const noexcept;
    [[nodiscard]] std::int32_t webPollUiEvent(std::span<std::uint8_t> output) noexcept;
    void webRequestShutdown() noexcept;

    [[nodiscard]] static AppDelegate* active() noexcept;

  private:
    [[nodiscard]] tgd::presentation::PresentationError synchronizeSuspension() noexcept;
    void pausePresentationOutput() noexcept;
    void resumePresentationOutputIfEligible() noexcept;
    void publishProfileUi() noexcept;
    void synchronizeProfileProgress() noexcept;
    void tryBeginQueuedRewardClaim() noexcept;
    void notifyObservedRewardClaimIfCommitted() noexcept;
    void submitF1RewardClaim(const F1RewardClaim& claim) noexcept override;
    [[nodiscard]] bool submitF1QuestUiProjection(
        const tgd::contracts::QuestUiProjectionSnapshot& projection
    ) noexcept override;
    void trace(std::string_view event, std::string_view result) noexcept;

    static AppDelegate* active_;

    tgd::runtime::RuntimeFacade runtime_;
    tgd::presentation::PresentationLifecycle presentation_;
    tgd::platform::web::WebPlatformBridge webPlatform_;
    tgd::runtime::ProfileStorageCoordinator profileStorage_;
    tgd::runtime::ProfileProgressCoordinator profileProgress_;
    tgd::runtime::ProfileProgressCoordinatorError profileProgressError_{
        tgd::runtime::ProfileProgressCoordinatorError::none
    };
    tgd::platform::web::WebBootConfig webBootConfig_{};
    F1RewardClaim queuedRewardClaim_{};
    F1RewardClaim observedRewardClaim_{};
    F1GrayboxLayer* grayboxLayer_{};
    std::uint64_t traceSequence_{0};
    bool profileBooted_{false};
    bool profileProgressReady_{false};
    bool hasQueuedRewardClaim_{false};
    bool hasObservedRewardClaim_{false};
    bool pageHidden_{false};
    bool pageFocused_{true};
    bool suspendedForPage_{false};
    bool graphicsContextLost_{false};
    bool presentationOutputActive_{false};
};
