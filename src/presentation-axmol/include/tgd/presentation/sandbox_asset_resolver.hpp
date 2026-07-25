#pragma once

#include <tgd/contracts/sandbox_definition.hpp>
#include <tgd/contracts/sandbox_pack.hpp>
#include <tgd/contracts/sha256.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tgd::presentation {

inline constexpr std::size_t sandbox_runtime_asset_slot_count = 12;
inline constexpr std::size_t sandbox_runtime_artifact_count = 24;
inline constexpr std::size_t sandbox_runtime_anchor_capacity = 8;
inline constexpr std::size_t sandbox_runtime_string_capacity = 256;

enum class SandboxAssetQuality : std::uint8_t {
  standard = 1,
  low = 2,
  invalid = 255,
};

enum class SandboxAssetMaturity : std::uint8_t {
  blockout = 1,
  invalid = 255,
};

enum class SandboxAssetChannel : std::uint8_t {
  internal_preview = 1,
  invalid = 255,
};

enum class SandboxAssetLicense : std::uint8_t {
  review_recorded_not_release_cleared = 1,
  invalid = 255,
};

enum class SandboxAssetResolveError : std::uint8_t {
  none = 0,
  invalid_quality = 1,
  invalid_registry = 2,
  capacity_exceeded = 3,
  missing_asset = 4,
  duplicate_asset = 5,
  stable_key_mismatch = 6,
  wrong_kind = 7,
  missing_artifact = 8,
  artifact_hash_mismatch = 9,
  artifact_import_failed = 10,
  budget_exceeded = 11,
  channel_blocked = 12,
  license_blocked = 13,
  maturity_blocked = 14,
  generation_exhausted = 15,
  stale_identity = 16,
  foreign_resolver = 17,
  invalid_prepared_update = 18,
  allocation_failed = 19,
  invalid = 255,
};

enum class SandboxAssetCommitDisposition : std::uint8_t {
  committed = 1,
  stale_identity = 2,
  foreign_resolver = 3,
  invalid_prepared_update = 4,
  invalid = 255,
};

struct SandboxAssetGenerationAdvance final {
  bool valid{};
  std::uint32_t generation{};
};

[[nodiscard]] constexpr SandboxAssetGenerationAdvance
sandbox_next_asset_generation(std::uint32_t current) noexcept {
  return current == UINT32_MAX
             ? SandboxAssetGenerationAdvance{}
             : SandboxAssetGenerationAdvance{true, current + 1U};
}

enum class SandboxAssetMetric : std::uint16_t {
  width = 1U << 0U,
  depth = 1U << 1U,
  height = 1U << 2U,
  body_height = 1U << 3U,
  body_root_height = 1U << 4U,
  ground_ring_diameter = 1U << 5U,
  forward = 1U << 6U,
  lateral = 1U << 7U,
  sweep = 1U << 8U,
};

enum class SandboxAssetMetricShape : std::uint8_t {
  visual_bounds = 1,
  nominal_extent = 2,
  invalid = 255,
};

enum class SandboxAssetFormat : std::uint8_t {
  png = 1,
  invalid = 255,
};

enum class SandboxAssetPixelFormat : std::uint8_t {
  rgba8 = 1,
  invalid = 255,
};

enum class SandboxAssetColorSpace : std::uint8_t {
  srgb = 1,
  invalid = 255,
};

enum class SandboxAssetAlphaMode : std::uint8_t {
  straight_source_premultiply_on_upload = 1,
  invalid = 255,
};

enum class SandboxAssetFilter : std::uint8_t {
  linear = 1,
  invalid = 255,
};

enum class SandboxAssetWrap : std::uint8_t {
  clamp = 1,
  invalid = 255,
};

struct SandboxAssetPresentationMetrics final {
  SandboxAssetMetricShape shape{SandboxAssetMetricShape::invalid};
  std::uint16_t present{};
  std::int32_t width_mm{};
  std::int32_t depth_mm{};
  std::int32_t height_mm{};
  std::int32_t body_height_mm{};
  std::int32_t body_root_height_mm{};
  std::int32_t ground_ring_diameter_mm{};
  std::int32_t forward_mm{};
  std::int32_t lateral_mm{};
  std::int32_t sweep_mm{};

  [[nodiscard]] friend constexpr bool
  operator==(const SandboxAssetPresentationMetrics &,
             const SandboxAssetPresentationMetrics &) noexcept = default;
};

struct SandboxAssetAnchorView final {
  std::string_view name{};
  std::int32_t x_mm{};
  std::int32_t y_mm{};
  std::int32_t height_mm{};
  std::string_view role{};
};

struct SandboxAssetArtifactView final {
  std::string_view artifact_id{};
  SandboxAssetQuality quality{SandboxAssetQuality::invalid};
  SandboxAssetFormat format{SandboxAssetFormat::invalid};
  SandboxAssetPixelFormat pixel_format{SandboxAssetPixelFormat::invalid};
  SandboxAssetColorSpace color_space{SandboxAssetColorSpace::invalid};
  SandboxAssetAlphaMode alpha_mode{SandboxAssetAlphaMode::invalid};
  SandboxAssetFilter filter{SandboxAssetFilter::invalid};
  SandboxAssetWrap wrap{SandboxAssetWrap::invalid};
  bool mipmaps{};
  contracts::Sha256Digest sha256{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t root_anchor_x{};
  std::uint32_t root_anchor_y{};
  std::uint32_t root_anchor_u_millionths{};
  std::uint32_t root_anchor_v_millionths{};
  std::uint32_t declared_file_bytes{};
  std::uint32_t decoded_bytes{};
  std::span<const std::uint8_t> canonical_bytes{};
};

struct SandboxAssetRegistryEntryView final {
  std::string_view stable_id{};
  contracts::StableContentKey stable_key{};
  contracts::SandboxAssetKind kind{contracts::SandboxAssetKind::obstacle};
  SandboxAssetPresentationMetrics metrics{};
  std::span<const SandboxAssetAnchorView> anchors{};
  SandboxAssetArtifactView standard{};
  SandboxAssetArtifactView low{};
};

struct SandboxAssetRegistryView final {
  contracts::Sha256Digest fingerprint{};
  SandboxAssetMaturity maturity{SandboxAssetMaturity::invalid};
  SandboxAssetChannel channel{SandboxAssetChannel::invalid};
  SandboxAssetLicense license{SandboxAssetLicense::invalid};
  bool preview_ready{};
  bool release_allowed{};
  std::uint32_t standard_transfer_limit{};
  std::uint32_t low_transfer_limit{};
  std::uint32_t standard_decoded_limit{};
  std::uint32_t low_decoded_limit{};
  std::span<const SandboxAssetRegistryEntryView> entries{};
};

// Implemented by a deterministic build-generated translation unit. It contains
// no runtime filesystem path and is identical for Native and Web builds.
[[nodiscard]] SandboxAssetRegistryView system_sandbox_asset_registry() noexcept;

struct SandboxPresentationAssetRequirement final {
  contracts::StableContentKey key{};
  std::uint16_t id_byte_count{};
  std::array<char, contracts::sandbox_pack_max_id_bytes> id_bytes{};
  contracts::SandboxAssetKind kind{contracts::SandboxAssetKind::obstacle};
};

struct SandboxAssetSourceIdentity final {
  std::uint32_t package_generation{};
  contracts::Sha256Digest package_checksum{};
  std::uint32_t runtime_generation{};

  [[nodiscard]] friend constexpr bool
  operator==(const SandboxAssetSourceIdentity &,
             const SandboxAssetSourceIdentity &) noexcept = default;
};

struct SandboxResolvedAssetSetIdentity final {
  std::uint32_t generation{};
  contracts::Sha256Digest registry_fingerprint{};
  SandboxAssetSourceIdentity source{};
  SandboxAssetQuality quality{SandboxAssetQuality::invalid};

  [[nodiscard]] friend constexpr bool
  operator==(const SandboxResolvedAssetSetIdentity &,
             const SandboxResolvedAssetSetIdentity &) noexcept = default;
};

class SandboxResolvedAsset final {
public:
  [[nodiscard]] std::string_view stable_id() const noexcept;
  [[nodiscard]] contracts::StableContentKey stable_key() const noexcept;
  [[nodiscard]] contracts::SandboxAssetKind kind() const noexcept;
  [[nodiscard]] std::string_view artifact_id() const noexcept;
  [[nodiscard]] SandboxAssetQuality quality() const noexcept;
  [[nodiscard]] const contracts::Sha256Digest &sha256() const noexcept;
  [[nodiscard]] std::uint32_t width() const noexcept;
  [[nodiscard]] std::uint32_t height() const noexcept;
  [[nodiscard]] std::uint32_t root_anchor_x() const noexcept;
  [[nodiscard]] std::uint32_t root_anchor_y() const noexcept;
  [[nodiscard]] SandboxAssetFormat format() const noexcept;
  [[nodiscard]] SandboxAssetPixelFormat pixel_format() const noexcept;
  [[nodiscard]] SandboxAssetColorSpace color_space() const noexcept;
  [[nodiscard]] SandboxAssetAlphaMode alpha_mode() const noexcept;
  [[nodiscard]] SandboxAssetFilter filter() const noexcept;
  [[nodiscard]] SandboxAssetWrap wrap() const noexcept;
  [[nodiscard]] const SandboxAssetPresentationMetrics &metrics() const noexcept;
  [[nodiscard]] std::span<const SandboxAssetAnchorView>
  anchors() const noexcept;
  [[nodiscard]] std::span<const std::uint8_t> canonical_bytes() const noexcept;

private:
  friend class SandboxAssetResolver;
  struct OwnedAnchor final {
    std::string name{};
    std::int32_t x_mm{};
    std::int32_t y_mm{};
    std::int32_t height_mm{};
    std::string role{};
  };

  std::string stable_id_{};
  contracts::StableContentKey stable_key_{};
  contracts::SandboxAssetKind kind_{contracts::SandboxAssetKind::obstacle};
  std::string artifact_id_{};
  SandboxAssetQuality quality_{SandboxAssetQuality::invalid};
  contracts::Sha256Digest sha256_{};
  std::uint32_t width_{};
  std::uint32_t height_{};
  std::uint32_t root_anchor_x_{};
  std::uint32_t root_anchor_y_{};
  SandboxAssetFormat format_{SandboxAssetFormat::invalid};
  SandboxAssetPixelFormat pixel_format_{SandboxAssetPixelFormat::invalid};
  SandboxAssetColorSpace color_space_{SandboxAssetColorSpace::invalid};
  SandboxAssetAlphaMode alpha_mode_{SandboxAssetAlphaMode::invalid};
  SandboxAssetFilter filter_{SandboxAssetFilter::invalid};
  SandboxAssetWrap wrap_{SandboxAssetWrap::invalid};
  SandboxAssetPresentationMetrics metrics_{};
  std::vector<OwnedAnchor> owned_anchors_{};
  std::vector<SandboxAssetAnchorView> anchor_views_{};
  std::vector<std::uint8_t> canonical_bytes_{};
};

class SandboxResolvedAssetSet final {
public:
  [[nodiscard]] const SandboxResolvedAssetSetIdentity &
  identity() const noexcept;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] const SandboxResolvedAsset *
  asset_at(std::size_t index) const noexcept;
  [[nodiscard]] const SandboxResolvedAsset *
  find(std::string_view stable_id, contracts::StableContentKey stable_key,
       contracts::SandboxAssetKind kind) const noexcept;

private:
  friend class SandboxAssetResolver;
  SandboxResolvedAssetSetIdentity identity_{};
  std::vector<SandboxResolvedAsset> assets_{};
};

class SandboxAssetPreparedUpdate final {
public:
  SandboxAssetPreparedUpdate() noexcept;
  ~SandboxAssetPreparedUpdate();
  SandboxAssetPreparedUpdate(SandboxAssetPreparedUpdate &&) noexcept;
  SandboxAssetPreparedUpdate &operator=(SandboxAssetPreparedUpdate &&) noexcept;
  SandboxAssetPreparedUpdate(const SandboxAssetPreparedUpdate &) = delete;
  SandboxAssetPreparedUpdate &
  operator=(const SandboxAssetPreparedUpdate &) = delete;

  [[nodiscard]] explicit operator bool() const noexcept;
  [[nodiscard]] const SandboxResolvedAssetSet *candidate() const noexcept;

private:
  friend class SandboxAssetResolver;
  struct State;
  explicit SandboxAssetPreparedUpdate(std::unique_ptr<State> state) noexcept;
  std::unique_ptr<State> state_{};
};

struct SandboxAssetPrepareResult final {
  SandboxAssetResolveError error{SandboxAssetResolveError::invalid};
  SandboxAssetPreparedUpdate prepared{};
};

struct SandboxAssetCommitResult final {
  SandboxAssetCommitDisposition disposition{
      SandboxAssetCommitDisposition::invalid};
  SandboxResolvedAssetSetIdentity identity{};
};

class SandboxAssetResolver final {
public:
  SandboxAssetResolver() noexcept;
  ~SandboxAssetResolver();
  SandboxAssetResolver(const SandboxAssetResolver &) = delete;
  SandboxAssetResolver &operator=(const SandboxAssetResolver &) = delete;
  SandboxAssetResolver(SandboxAssetResolver &&) = delete;
  SandboxAssetResolver &operator=(SandboxAssetResolver &&) = delete;

  [[nodiscard]] SandboxAssetPrepareResult
  prepare(SandboxAssetRegistryView registry,
          std::span<const SandboxPresentationAssetRequirement> requirements,
          SandboxAssetQuality quality,
          SandboxAssetSourceIdentity source) noexcept;
  [[nodiscard]] SandboxAssetCommitResult
  commit(SandboxAssetPreparedUpdate &&prepared) noexcept;

  [[nodiscard]] const SandboxResolvedAssetSet *live_set() const noexcept;
  [[nodiscard]] SandboxResolvedAssetSetIdentity identity() const noexcept;

private:
  struct Lifetime;
  std::shared_ptr<const Lifetime> lifetime_{};
  std::unique_ptr<SandboxResolvedAssetSet> live_{};
};

} // namespace tgd::presentation
