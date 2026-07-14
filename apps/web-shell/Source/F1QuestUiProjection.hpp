#pragma once

#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/quest_ui.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

class IF1QuestUiProjectionSink {
  public:
    virtual ~IF1QuestUiProjectionSink() = default;

    [[nodiscard]] virtual bool submitF1QuestUiProjection(
        const tgd::contracts::QuestUiProjectionSnapshot& projection
    ) noexcept = 0;
};

enum class F1QuestUiChoiceMode : std::uint8_t {
    none = 0,
    external,
    native,
};

enum class F1QuestUiChoiceError : std::uint8_t {
    none = 0,
    invalid_projection,
    already_pending,
    unavailable_in_mode,
    option_out_of_range,
    missing_presentation,
    stale_intent,
};

struct F1QuestUiChoiceIntentResult final {
    F1QuestUiChoiceError error{F1QuestUiChoiceError::none};
    tgd::contracts::QuestUiSelectionIntent intent{};
};

[[nodiscard]] constexpr std::string_view f1QuestUiNativeChoiceLabel(
    tgd::contracts::StableContentKey selection
) noexcept {
    using tgd::contracts::stable_content_key;
    if (selection == stable_content_key("f1_choice_arrival_high_water_tags")) {
        return "High-water marks";
    }
    if (selection == stable_content_key("f1_choice_arrival_drowned_manifest")) {
        return "Drowned manifest";
    }
    if (selection == stable_content_key("f1_choice_arrival_follow_bell")) {
        return "Follow the bell";
    }
    if (selection == stable_content_key("f1_choice_mooring_cross_belay")) {
        return "Cross-belay";
    }
    if (selection == stable_content_key("f1_choice_mooring_quick_hitch")) {
        return "Quick hitch";
    }
    if (selection == stable_content_key("f1_choice_training_windward_lane")) {
        return "Windward lane";
    }
    if (selection == stable_content_key("f1_choice_training_leeward_lane")) {
        return "Leeward lane";
    }
    return {};
}

class F1QuestUiChoiceState final {
  public:
    [[nodiscard]] F1QuestUiChoiceError begin(
        const tgd::contracts::QuestUiProjectionSnapshot& projection,
        bool external_consumer_accepted
    ) noexcept {
        using tgd::contracts::QuestUiProjectionSource;
        using tgd::contracts::QuestUiSurface;
        if (mode_ != F1QuestUiChoiceMode::none) {
            return F1QuestUiChoiceError::already_pending;
        }
        if (projection.sequence == 0 || projection.checksum == 0 ||
            projection.objective == 0 ||
            projection.source != QuestUiProjectionSource::choice_available ||
            projection.surface != QuestUiSurface::choice ||
            projection.choice_option_count == 0 ||
            projection.choice_option_count > projection.choice_options.size()) {
            return F1QuestUiChoiceError::invalid_projection;
        }
        for (std::size_t index = 0; index < projection.choice_option_count; ++index) {
            const auto& option = projection.choice_options[index];
            if (option.interaction == 0 || option.selection == 0) {
                return F1QuestUiChoiceError::invalid_projection;
            }
            labels_[index] = f1QuestUiNativeChoiceLabel(option.selection);
            if (!external_consumer_accepted && labels_[index].empty()) {
                labels_ = {};
                return F1QuestUiChoiceError::missing_presentation;
            }
        }
        projection_ = projection;
        mode_ = external_consumer_accepted ? F1QuestUiChoiceMode::external
                                           : F1QuestUiChoiceMode::native;
        return F1QuestUiChoiceError::none;
    }

    [[nodiscard]] constexpr bool pending() const noexcept {
        return mode_ != F1QuestUiChoiceMode::none;
    }

    [[nodiscard]] constexpr bool native_pending() const noexcept {
        return mode_ == F1QuestUiChoiceMode::native;
    }

    [[nodiscard]] constexpr F1QuestUiChoiceMode mode() const noexcept {
        return mode_;
    }

    [[nodiscard]] constexpr std::size_t option_count() const noexcept {
        return projection_.choice_option_count;
    }

    [[nodiscard]] constexpr std::string_view option_label(
        std::size_t index
    ) const noexcept {
        return native_pending() && index < option_count() ? labels_[index]
                                                          : std::string_view{};
    }

    [[nodiscard]] F1QuestUiChoiceIntentResult native_intent(
        std::size_t index
    ) const noexcept {
        if (!native_pending()) {
            return {F1QuestUiChoiceError::unavailable_in_mode, {}};
        }
        if (index >= option_count()) {
            return {F1QuestUiChoiceError::option_out_of_range, {}};
        }
        const auto& option = projection_.choice_options[index];
        return {
            F1QuestUiChoiceError::none,
            {
                projection_.sequence,
                projection_.checksum,
                projection_.objective,
                option.interaction,
                option.selection,
            },
        };
    }

    [[nodiscard]] bool matches(
        const tgd::contracts::QuestUiSelectionIntent& intent
    ) const noexcept {
        if (!pending() || intent.projection_sequence != projection_.sequence ||
            intent.projection_checksum != projection_.checksum ||
            intent.objective != projection_.objective) {
            return false;
        }
        for (std::size_t index = 0; index < option_count(); ++index) {
            const auto& option = projection_.choice_options[index];
            if (intent.interaction == option.interaction &&
                intent.selection == option.selection) {
                return true;
            }
        }
        return false;
    }

    void finish() noexcept {
        mode_ = F1QuestUiChoiceMode::none;
        projection_ = {};
        labels_ = {};
    }

  private:
    F1QuestUiChoiceMode mode_{F1QuestUiChoiceMode::none};
    tgd::contracts::QuestUiProjectionSnapshot projection_{};
    std::array<std::string_view, tgd::contracts::quest_ui_choice_option_capacity> labels_{};
};
