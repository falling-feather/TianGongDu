#pragma once

#include <tgd/contracts/session_types.hpp>

struct F1RewardClaim final {
    tgd::contracts::StableContentKey source_id{};
    tgd::contracts::StableContentKey reward_id{};
    tgd::contracts::StableContentKey reward_dedup_key{};

    [[nodiscard]] bool valid() const noexcept {
        return source_id != 0 && reward_id != 0 && reward_dedup_key != 0;
    }

    [[nodiscard]] friend constexpr bool operator==(
        const F1RewardClaim&,
        const F1RewardClaim&
    ) noexcept = default;
};

class IF1RewardClaimSink {
  public:
    virtual ~IF1RewardClaimSink() = default;
    virtual void submitF1RewardClaim(const F1RewardClaim& claim) noexcept = 0;
};
