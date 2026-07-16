#pragma once

#include <tgd/contracts/sandbox_pack.hpp>
#include <tgd/gameplay/sandbox_session.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace tgd::content {
class SandboxPackageDocument;
}

namespace tgd::integration {

enum class SandboxSessionBlueprintBuildError : std::uint8_t {
    none = 0,
    capacity_exceeded = 1,
    invalid_owned_content_id = 2,
    id_byte_capacity_exceeded = 3,
    invalid = 255,
};

class SandboxSessionBlueprint;
class SandboxSessionBlueprintBuildResult;

[[nodiscard]] SandboxSessionBlueprintBuildResult build_sandbox_session_blueprint(
    const content::SandboxPackageDocument& document
) noexcept;

[[nodiscard]] gameplay::SandboxSessionBuildResult
initialize_sandbox_session_from_blueprint(
    gameplay::SandboxSession& destination,
    const SandboxSessionBlueprint& blueprint,
    const gameplay::SandboxPlayerRuntimeBinding& player_binding
) noexcept;

class SandboxSessionBlueprint final {
  public:
    static constexpr std::size_t id_byte_capacity =
        contracts::sandbox_pack_max_id_bytes;
    static constexpr std::size_t interaction_capacity =
        contracts::sandbox_interaction_capacity;
    static constexpr std::size_t mechanism_capacity =
        contracts::sandbox_mechanism_capacity;
    static constexpr std::size_t ground_blocker_capacity =
        contracts::sandbox_ground_blocker_capacity;
    static constexpr std::size_t safe_point_capacity =
        contracts::sandbox_safe_point_capacity;

    SandboxSessionBlueprint(const SandboxSessionBlueprint&) = delete;
    SandboxSessionBlueprint& operator=(const SandboxSessionBlueprint&) = delete;
    SandboxSessionBlueprint(SandboxSessionBlueprint&& other) noexcept;
    SandboxSessionBlueprint& operator=(SandboxSessionBlueprint&& other) noexcept;

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxSessionBlueprint&,
        const SandboxSessionBlueprint&
    ) noexcept = default;

  private:
    struct OwnedContentId final {
        contracts::StableContentKey key{};
        std::uint16_t length{};
        std::array<char, id_byte_capacity> bytes{};

        [[nodiscard]] friend constexpr bool operator==(
            const OwnedContentId&,
            const OwnedContentId&
        ) noexcept = default;
    };

    struct PlayerRecord final {
        OwnedContentId id{};
        OwnedContentId region_id{};
        OwnedContentId initial_safe_point_id{};
        contracts::GroundPoseMm pose{};
        std::uint32_t facing_millidegrees{};

        [[nodiscard]] friend constexpr bool operator==(
            const PlayerRecord&,
            const PlayerRecord&
        ) noexcept = default;
    };

    struct SafePointRecord final {
        OwnedContentId id{};
        OwnedContentId region_id{};
        contracts::GroundPoseMm pose{};
        std::uint32_t facing_millidegrees{};

        [[nodiscard]] friend constexpr bool operator==(
            const SafePointRecord&,
            const SafePointRecord&
        ) noexcept = default;
    };

    struct InteractionRecord final {
        OwnedContentId id{};
        contracts::GroundPoseMm pose{};

        [[nodiscard]] friend constexpr bool operator==(
            const InteractionRecord&,
            const InteractionRecord&
        ) noexcept = default;
    };

    struct MechanismRecord final {
        OwnedContentId id{};

        [[nodiscard]] friend constexpr bool operator==(
            const MechanismRecord&,
            const MechanismRecord&
        ) noexcept = default;
    };

    struct GroundBlockerRecord final {
        OwnedContentId id{};

        [[nodiscard]] friend constexpr bool operator==(
            const GroundBlockerRecord&,
            const GroundBlockerRecord&
        ) noexcept = default;
    };

    struct InteractionBindingRecord final {
        OwnedContentId interaction_id{};
        contracts::SandboxInteractionOperation operation{
            contracts::SandboxInteractionOperation::invalid
        };
        std::int32_t range_mm{};
        OwnedContentId target_mechanism_id{};

        [[nodiscard]] friend constexpr bool operator==(
            const InteractionBindingRecord&,
            const InteractionBindingRecord&
        ) noexcept = default;
    };

    struct MechanismBindingRecord final {
        OwnedContentId mechanism_id{};
        contracts::SandboxMechanismActivation activation{
            contracts::SandboxMechanismActivation::invalid
        };
        OwnedContentId target_ground_blocker_id{};

        [[nodiscard]] friend constexpr bool operator==(
            const MechanismBindingRecord&,
            const MechanismBindingRecord&
        ) noexcept = default;
    };

    SandboxSessionBlueprint() = default;

    [[nodiscard]] static SandboxSessionBlueprintBuildError copy_content_id(
        OwnedContentId& destination,
        contracts::ContentId source
    ) noexcept;
    [[nodiscard]] static contracts::ContentId content_id_view(
        const OwnedContentId& source
    ) noexcept;

    PlayerRecord player_{};
    std::array<SafePointRecord, safe_point_capacity> safe_points_{};
    std::array<InteractionRecord, interaction_capacity> interactions_{};
    std::array<MechanismRecord, mechanism_capacity> mechanisms_{};
    std::array<GroundBlockerRecord, ground_blocker_capacity> ground_blockers_{};
    std::array<InteractionBindingRecord, interaction_capacity>
        interaction_bindings_{};
    std::array<MechanismBindingRecord, mechanism_capacity> mechanism_bindings_{};
    std::size_t safe_point_count_{};
    std::size_t interaction_count_{};
    std::size_t mechanism_count_{};
    std::size_t ground_blocker_count_{};
    std::size_t interaction_binding_count_{};
    std::size_t mechanism_binding_count_{};
    bool valid_{};

    friend class SandboxSessionBlueprintBuildResult;
    friend SandboxSessionBlueprintBuildResult build_sandbox_session_blueprint(
        const content::SandboxPackageDocument& document
    ) noexcept;
    friend gameplay::SandboxSessionBuildResult
    initialize_sandbox_session_from_blueprint(
        gameplay::SandboxSession& destination,
        const SandboxSessionBlueprint& blueprint,
        const gameplay::SandboxPlayerRuntimeBinding& player_binding
    ) noexcept;
};

class SandboxSessionBlueprintBuildResult final {
  public:
    SandboxSessionBlueprintBuildResult(const SandboxSessionBlueprintBuildResult&) = delete;
    SandboxSessionBlueprintBuildResult& operator=(
        const SandboxSessionBlueprintBuildResult&
    ) = delete;
    SandboxSessionBlueprintBuildResult(
        SandboxSessionBlueprintBuildResult&& other
    ) noexcept;
    SandboxSessionBlueprintBuildResult& operator=(
        SandboxSessionBlueprintBuildResult&& other
    ) noexcept;

    [[nodiscard]] SandboxSessionBlueprintBuildError error() const noexcept;
    [[nodiscard]] bool succeeded() const noexcept;
    [[nodiscard]] const SandboxSessionBlueprint* blueprint() const noexcept;
    [[nodiscard]] std::optional<SandboxSessionBlueprint> take_blueprint() && noexcept;

  private:
    SandboxSessionBlueprintBuildResult(
        SandboxSessionBlueprintBuildError error,
        std::optional<SandboxSessionBlueprint> blueprint
    ) noexcept;

    SandboxSessionBlueprintBuildError error_{
        SandboxSessionBlueprintBuildError::invalid
    };
    std::optional<SandboxSessionBlueprint> blueprint_{};

    friend SandboxSessionBlueprintBuildResult build_sandbox_session_blueprint(
        const content::SandboxPackageDocument& document
    ) noexcept;
};

}  // namespace tgd::integration
