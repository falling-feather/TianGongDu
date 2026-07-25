#pragma once

#include <tgd/content/sandbox_package_provider.hpp>
#include <tgd/contracts/sandbox_definition.hpp>
#include <tgd/contracts/sandbox_pack.hpp>
#include <tgd/contracts/sha256.hpp>
#include <tgd/gameplay/sandbox_session.hpp>
#include <tgd/runtime/collision_world.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace tgd::content {
class SandboxPackageDocument;
}

namespace tgd::integration {

struct SandboxPublishedPackageArtifact final {
    content::SandboxPackagePublicationIdentity identity{};
    std::vector<std::uint8_t> canonical_bytes{};
};

enum class SandboxRuntimePublishDisposition : std::uint8_t {
    published = 1,
    unchanged = 2,
    invalid_artifact = 3,
    stale_generation = 4,
    identity_conflict = 5,
    generation_exhausted = 6,
    decode_failed = 7,
    fingerprint_mismatch = 8,
    blueprint_prepare_failed = 9,
    session_prepare_failed = 10,
    collision_prepare_failed = 11,
    asset_set_prepare_failed = 12,
    allocation_failed = 13,
    invalid = 255,
};

enum class SandboxAssetSetBuildError : std::uint8_t {
    none = 0,
    capacity_exceeded = 1,
    invalid_content_id = 2,
    duplicate_content_id = 3,
    invalid_kind = 4,
    invalid = 255,
};

struct SandboxOwnedStableAsset final {
    contracts::StableContentKey key{};
    std::uint16_t id_byte_count{};
    std::array<char, contracts::sandbox_pack_max_id_bytes> id_bytes{};
    contracts::SandboxAssetKind kind{contracts::SandboxAssetKind::obstacle};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxOwnedStableAsset&,
        const SandboxOwnedStableAsset&
    ) noexcept = default;
};

struct SandboxStaticCollisionRecord final {
    contracts::StableContentKey blocker_key{};
    contracts::StableContentKey region_key{};
    contracts::CollisionShapeId shape_id{};
    std::int32_t min_x{};
    std::int32_t max_x{};
    std::int32_t min_y{};
    std::int32_t max_y{};
    std::int32_t min_height{};
    std::int32_t max_height{};
    std::int16_t floor_layer{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxStaticCollisionRecord&,
        const SandboxStaticCollisionRecord&
    ) noexcept = default;
};

struct SandboxRuntimeSnapshot final {
    bool initialized{};
    std::uint32_t runtime_generation{};
    std::uint32_t package_generation{};
    contracts::Sha256Digest package_checksum{};
    std::uint32_t canonical_byte_count{};
    gameplay::SandboxSessionSnapshot session{};
    std::uint16_t collision_region_count{};
    std::uint16_t collision_record_count{};
    std::uint16_t asset_count{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxRuntimeSnapshot&,
        const SandboxRuntimeSnapshot&
    ) noexcept = default;
};

struct SandboxRuntimePublishResult final {
    SandboxRuntimePublishDisposition disposition{
        SandboxRuntimePublishDisposition::invalid
    };
    SandboxAssetSetBuildError asset_error{SandboxAssetSetBuildError::none};
    runtime::CollisionWorldError collision_error{runtime::CollisionWorldError::none};
    SandboxRuntimeSnapshot snapshot{};
};

struct SandboxRuntimeGenerationAdvance final {
    bool valid{};
    std::uint32_t generation{};
};

[[nodiscard]] constexpr SandboxRuntimeGenerationAdvance
sandbox_next_runtime_generation(std::uint32_t current) noexcept {
    if (current == UINT32_MAX) {
        return {};
    }
    return {true, current + 1U};
}

class SandboxRuntimeCoordinator final {
  public:
    SandboxRuntimeCoordinator() noexcept;
    ~SandboxRuntimeCoordinator();

    SandboxRuntimeCoordinator(const SandboxRuntimeCoordinator&) = delete;
    SandboxRuntimeCoordinator& operator=(const SandboxRuntimeCoordinator&) = delete;
    SandboxRuntimeCoordinator(SandboxRuntimeCoordinator&&) = delete;
    SandboxRuntimeCoordinator& operator=(SandboxRuntimeCoordinator&&) = delete;

    [[nodiscard]] SandboxRuntimePublishResult publish(
        SandboxPublishedPackageArtifact artifact,
        const gameplay::SandboxPlayerRuntimeBinding& player_binding
    ) noexcept;

    [[nodiscard]] SandboxRuntimeSnapshot snapshot() const noexcept;
    [[nodiscard]] const content::SandboxPackageDocument* document() const noexcept;
    [[nodiscard]] const gameplay::SandboxSession* session() const noexcept;
    [[nodiscard]] const runtime::StaticCollisionWorld* collision_world(
        contracts::StableContentKey region_key
    ) const noexcept;
    [[nodiscard]] std::optional<SandboxStaticCollisionRecord> collision_at(
        std::size_t index
    ) const noexcept;
    [[nodiscard]] std::optional<SandboxOwnedStableAsset> asset_at(
        std::size_t index
    ) const noexcept;

  private:
    struct LiveAggregate;
    std::unique_ptr<LiveAggregate> live_{};
};

}  // namespace tgd::integration
