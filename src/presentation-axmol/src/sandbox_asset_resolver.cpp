#include <tgd/presentation/sandbox_asset_resolver.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace tgd::presentation {
namespace {

using Error = SandboxAssetResolveError;

[[nodiscard]] bool
nonzero_digest(const contracts::Sha256Digest &digest) noexcept {
  return std::any_of(digest.begin(), digest.end(),
                     [](std::uint8_t byte) noexcept { return byte != 0; });
}

[[nodiscard]] bool valid_quality(SandboxAssetQuality quality) noexcept {
  return quality == SandboxAssetQuality::standard ||
         quality == SandboxAssetQuality::low;
}

[[nodiscard]] bool valid_kind(contracts::SandboxAssetKind kind) noexcept {
  using Kind = contracts::SandboxAssetKind;
  switch (kind) {
  case Kind::player:
  case Kind::actor:
  case Kind::obstacle:
  case Kind::interaction:
  case Kind::mechanism:
  case Kind::safe_point:
  case Kind::effect:
    return true;
  }
  return false;
}

[[nodiscard]] bool valid_string(std::string_view value) noexcept {
  return !value.empty() && value.size() <= sandbox_runtime_string_capacity;
}

[[nodiscard]] std::uint32_t read_big_u32(std::span<const std::uint8_t> bytes,
                                         std::size_t offset) noexcept {
  return static_cast<std::uint32_t>(bytes[offset]) << 24U |
         static_cast<std::uint32_t>(bytes[offset + 1U]) << 16U |
         static_cast<std::uint32_t>(bytes[offset + 2U]) << 8U |
         static_cast<std::uint32_t>(bytes[offset + 3U]);
}

[[nodiscard]] std::uint32_t
png_crc(std::span<const std::uint8_t> bytes) noexcept {
  auto crc = 0xffffffffU;
  for (const auto byte : bytes) {
    crc ^= byte;
    for (unsigned bit = 0; bit < 8U; ++bit) {
      const auto mask = static_cast<std::uint32_t>(0U - (crc & 1U));
      crc = (crc >> 1U) ^ (0xedb88320U & mask);
    }
  }
  return crc ^ 0xffffffffU;
}

[[nodiscard]] bool valid_png(std::span<const std::uint8_t> bytes,
                             std::uint32_t width,
                             std::uint32_t height) noexcept {
  constexpr std::array<std::uint8_t, 8> signature{0x89U, 0x50U, 0x4eU, 0x47U,
                                                  0x0dU, 0x0aU, 0x1aU, 0x0aU};
  if (bytes.size() < 57U ||
      !std::equal(signature.begin(), signature.end(), bytes.begin())) {
    return false;
  }

  std::size_t offset = signature.size();
  bool seen_ihdr{};
  bool seen_srgb{};
  bool seen_idat{};
  bool seen_iend{};
  while (offset < bytes.size()) {
    if (bytes.size() - offset < 12U) {
      return false;
    }
    const auto chunk_size = read_big_u32(bytes, offset);
    const auto remaining = bytes.size() - offset - 12U;
    if (chunk_size > remaining) {
      return false;
    }
    const auto type_offset = offset + 4U;
    const auto data_offset = type_offset + 4U;
    const auto crc_offset = data_offset + chunk_size;
    const auto type_is = [&](char a, char b, char c, char d) noexcept {
      return bytes[type_offset] == static_cast<std::uint8_t>(a) &&
             bytes[type_offset + 1U] == static_cast<std::uint8_t>(b) &&
             bytes[type_offset + 2U] == static_cast<std::uint8_t>(c) &&
             bytes[type_offset + 3U] == static_cast<std::uint8_t>(d);
    };
    if (png_crc(bytes.subspan(type_offset, 4U + chunk_size)) !=
        read_big_u32(bytes, crc_offset)) {
      return false;
    }

    if (type_is('I', 'H', 'D', 'R')) {
      if (seen_ihdr || offset != signature.size() || chunk_size != 13U ||
          read_big_u32(bytes, data_offset) != width ||
          read_big_u32(bytes, data_offset + 4U) != height ||
          bytes[data_offset + 8U] != 8U || bytes[data_offset + 9U] != 6U ||
          bytes[data_offset + 10U] != 0U || bytes[data_offset + 11U] != 0U ||
          bytes[data_offset + 12U] != 0U) {
        return false;
      }
      seen_ihdr = true;
    } else if (type_is('s', 'R', 'G', 'B')) {
      if (!seen_ihdr || seen_srgb || seen_idat || chunk_size != 1U ||
          bytes[data_offset] != 0U) {
        return false;
      }
      seen_srgb = true;
    } else if (type_is('I', 'D', 'A', 'T')) {
      if (!seen_ihdr || !seen_srgb || seen_iend || chunk_size == 0U) {
        return false;
      }
      seen_idat = true;
    } else if (type_is('I', 'E', 'N', 'D')) {
      if (!seen_idat || seen_iend || chunk_size != 0U ||
          crc_offset + 4U != bytes.size()) {
        return false;
      }
      seen_iend = true;
    } else {
      // ART-003 canonical PNG is exactly IHDR, sRGB, IDAT+, IEND.
      return false;
    }
    offset = crc_offset + 4U;
  }
  return seen_ihdr && seen_srgb && seen_idat && seen_iend;
}

[[nodiscard]] const SandboxAssetArtifactView &
artifact_for(const SandboxAssetRegistryEntryView &entry,
             SandboxAssetQuality quality) noexcept {
  return quality == SandboxAssetQuality::standard ? entry.standard : entry.low;
}

[[nodiscard]] std::string_view requirement_id(
    const SandboxPresentationAssetRequirement &requirement) noexcept {
  if (requirement.id_byte_count == 0 ||
      requirement.id_byte_count > requirement.id_bytes.size()) {
    return {};
  }
  return {requirement.id_bytes.data(), requirement.id_byte_count};
}

[[nodiscard]] bool
valid_source_identity(const SandboxAssetSourceIdentity &source) noexcept {
  return source.package_generation != 0 && source.runtime_generation != 0 &&
         nonzero_digest(source.package_checksum);
}

[[nodiscard]] bool
valid_metrics(const SandboxAssetRegistryEntryView &entry) noexcept {
  constexpr auto known_bits = static_cast<std::uint16_t>(
      static_cast<std::uint16_t>(SandboxAssetMetric::width) |
      static_cast<std::uint16_t>(SandboxAssetMetric::depth) |
      static_cast<std::uint16_t>(SandboxAssetMetric::height) |
      static_cast<std::uint16_t>(SandboxAssetMetric::body_height) |
      static_cast<std::uint16_t>(SandboxAssetMetric::body_root_height) |
      static_cast<std::uint16_t>(SandboxAssetMetric::ground_ring_diameter) |
      static_cast<std::uint16_t>(SandboxAssetMetric::forward) |
      static_cast<std::uint16_t>(SandboxAssetMetric::lateral) |
      static_cast<std::uint16_t>(SandboxAssetMetric::sweep));
  const auto &metrics = entry.metrics;
  const auto expected_shape = entry.kind == contracts::SandboxAssetKind::effect
                                  ? SandboxAssetMetricShape::nominal_extent
                                  : SandboxAssetMetricShape::visual_bounds;
  if (metrics.shape != expected_shape || metrics.present == 0 ||
      (metrics.present & static_cast<std::uint16_t>(~known_bits)) != 0) {
    return false;
  }
  const auto field_valid = [&](SandboxAssetMetric metric, std::int32_t value) {
    const auto present =
        (metrics.present & static_cast<std::uint16_t>(metric)) != 0;
    if (!present) {
      return value == 0;
    }
    // Flat telegraph extents explicitly author height=0. Every other
    // present physical dimension must be positive.
    return value > 0 ||
           (metric == SandboxAssetMetric::height &&
            metrics.shape == SandboxAssetMetricShape::nominal_extent &&
            value == 0);
  };
  return field_valid(SandboxAssetMetric::width, metrics.width_mm) &&
         field_valid(SandboxAssetMetric::depth, metrics.depth_mm) &&
         field_valid(SandboxAssetMetric::height, metrics.height_mm) &&
         field_valid(SandboxAssetMetric::body_height, metrics.body_height_mm) &&
         field_valid(SandboxAssetMetric::body_root_height,
                     metrics.body_root_height_mm) &&
         field_valid(SandboxAssetMetric::ground_ring_diameter,
                     metrics.ground_ring_diameter_mm) &&
         field_valid(SandboxAssetMetric::forward, metrics.forward_mm) &&
         field_valid(SandboxAssetMetric::lateral, metrics.lateral_mm) &&
         field_valid(SandboxAssetMetric::sweep, metrics.sweep_mm);
}

[[nodiscard]] Error
validate_registry_header(SandboxAssetRegistryView registry,
                         SandboxAssetQuality quality) noexcept {
  if (!valid_quality(quality)) {
    return Error::invalid_quality;
  }
  if (registry.entries.size() != sandbox_runtime_asset_slot_count ||
      !nonzero_digest(registry.fingerprint) ||
      registry.standard_transfer_limit == 0 ||
      registry.low_transfer_limit == 0 ||
      registry.standard_decoded_limit == 0 || registry.low_decoded_limit == 0) {
    return Error::invalid_registry;
  }
  if (registry.channel != SandboxAssetChannel::internal_preview) {
    return Error::channel_blocked;
  }
  if (registry.license !=
      SandboxAssetLicense::review_recorded_not_release_cleared) {
    return Error::license_blocked;
  }
  if (registry.maturity != SandboxAssetMaturity::blockout ||
      registry.preview_ready || registry.release_allowed) {
    return Error::maturity_blocked;
  }
  return Error::none;
}

[[nodiscard]] Error
validate_artifact(const SandboxAssetArtifactView &artifact,
                  SandboxAssetQuality expected_quality) noexcept {
  constexpr std::uint64_t anchor_denominator = 1'000'000ULL;
  if (artifact.quality != expected_quality ||
      !valid_string(artifact.artifact_id) ||
      artifact.format != SandboxAssetFormat::png ||
      artifact.pixel_format != SandboxAssetPixelFormat::rgba8 ||
      artifact.color_space != SandboxAssetColorSpace::srgb ||
      artifact.alpha_mode !=
          SandboxAssetAlphaMode::straight_source_premultiply_on_upload ||
      artifact.filter != SandboxAssetFilter::linear ||
      artifact.wrap != SandboxAssetWrap::clamp || artifact.mipmaps ||
      !nonzero_digest(artifact.sha256) || artifact.canonical_bytes.empty() ||
      artifact.canonical_bytes.size() != artifact.declared_file_bytes ||
      artifact.width == 0 || artifact.height == 0 ||
      artifact.root_anchor_x >= artifact.width ||
      artifact.root_anchor_y >= artifact.height ||
      artifact.root_anchor_u_millionths != 500'000U ||
      artifact.root_anchor_v_millionths != 125'000U ||
      (static_cast<std::uint64_t>(artifact.width) *
       artifact.root_anchor_u_millionths) %
              anchor_denominator !=
          0 ||
      (static_cast<std::uint64_t>(artifact.height) *
       artifact.root_anchor_v_millionths) %
              anchor_denominator !=
          0 ||
      static_cast<std::uint64_t>(artifact.root_anchor_x) !=
          static_cast<std::uint64_t>(artifact.width) *
              artifact.root_anchor_u_millionths / anchor_denominator ||
      static_cast<std::uint64_t>(artifact.root_anchor_y) !=
          static_cast<std::uint64_t>(artifact.height) *
              artifact.root_anchor_v_millionths / anchor_denominator ||
      artifact.decoded_bytes !=
          static_cast<std::uint64_t>(artifact.width) *
              static_cast<std::uint64_t>(artifact.height) * 4ULL) {
    return Error::missing_artifact;
  }
  if (contracts::sha256(artifact.canonical_bytes) != artifact.sha256) {
    return Error::artifact_hash_mismatch;
  }
  if (!valid_png(artifact.canonical_bytes, artifact.width, artifact.height)) {
    return Error::artifact_import_failed;
  }
  return Error::none;
}

[[nodiscard]] Error
validate_entry(const SandboxAssetRegistryEntryView &entry) noexcept {
  if (!valid_string(entry.stable_id) || entry.stable_key == 0 ||
      contracts::stable_content_key(entry.stable_id) != entry.stable_key ||
      !valid_kind(entry.kind) || !valid_metrics(entry) ||
      entry.anchors.empty() ||
      entry.anchors.size() > sandbox_runtime_anchor_capacity) {
    return Error::invalid_registry;
  }
  for (std::size_t index = 0; index < entry.anchors.size(); ++index) {
    const auto &anchor = entry.anchors[index];
    if (!valid_string(anchor.name) || !valid_string(anchor.role)) {
      return Error::invalid_registry;
    }
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (entry.anchors[previous].name == anchor.name) {
        return Error::invalid_registry;
      }
    }
  }
  const auto standard =
      validate_artifact(entry.standard, SandboxAssetQuality::standard);
  if (standard != Error::none) {
    return standard;
  }
  return validate_artifact(entry.low, SandboxAssetQuality::low);
}

} // namespace

struct SandboxAssetResolver::Lifetime final {};

struct SandboxAssetPreparedUpdate::State final {
  std::shared_ptr<const void> origin{};
  std::uint32_t expected_generation{};
  std::unique_ptr<SandboxResolvedAssetSet> candidate{};
};

std::string_view SandboxResolvedAsset::stable_id() const noexcept {
  return stable_id_;
}

contracts::StableContentKey SandboxResolvedAsset::stable_key() const noexcept {
  return stable_key_;
}

contracts::SandboxAssetKind SandboxResolvedAsset::kind() const noexcept {
  return kind_;
}

std::string_view SandboxResolvedAsset::artifact_id() const noexcept {
  return artifact_id_;
}

SandboxAssetQuality SandboxResolvedAsset::quality() const noexcept {
  return quality_;
}

const contracts::Sha256Digest &SandboxResolvedAsset::sha256() const noexcept {
  return sha256_;
}

std::uint32_t SandboxResolvedAsset::width() const noexcept { return width_; }

std::uint32_t SandboxResolvedAsset::height() const noexcept { return height_; }

std::uint32_t SandboxResolvedAsset::root_anchor_x() const noexcept {
  return root_anchor_x_;
}

std::uint32_t SandboxResolvedAsset::root_anchor_y() const noexcept {
  return root_anchor_y_;
}

SandboxAssetFormat SandboxResolvedAsset::format() const noexcept {
  return format_;
}

SandboxAssetPixelFormat SandboxResolvedAsset::pixel_format() const noexcept {
  return pixel_format_;
}

SandboxAssetColorSpace SandboxResolvedAsset::color_space() const noexcept {
  return color_space_;
}

SandboxAssetAlphaMode SandboxResolvedAsset::alpha_mode() const noexcept {
  return alpha_mode_;
}

SandboxAssetFilter SandboxResolvedAsset::filter() const noexcept {
  return filter_;
}

SandboxAssetWrap SandboxResolvedAsset::wrap() const noexcept { return wrap_; }

const SandboxAssetPresentationMetrics &
SandboxResolvedAsset::metrics() const noexcept {
  return metrics_;
}

std::span<const SandboxAssetAnchorView>
SandboxResolvedAsset::anchors() const noexcept {
  return anchor_views_;
}

std::span<const std::uint8_t>
SandboxResolvedAsset::canonical_bytes() const noexcept {
  return canonical_bytes_;
}

const SandboxResolvedAssetSetIdentity &
SandboxResolvedAssetSet::identity() const noexcept {
  return identity_;
}

std::size_t SandboxResolvedAssetSet::size() const noexcept {
  return assets_.size();
}

const SandboxResolvedAsset *
SandboxResolvedAssetSet::asset_at(std::size_t index) const noexcept {
  return index < assets_.size() ? &assets_[index] : nullptr;
}

const SandboxResolvedAsset *
SandboxResolvedAssetSet::find(std::string_view stable_id,
                              contracts::StableContentKey stable_key,
                              contracts::SandboxAssetKind kind) const noexcept {
  const auto found = std::lower_bound(
      assets_.begin(), assets_.end(), stable_id,
      [](const SandboxResolvedAsset &asset, std::string_view id) noexcept {
        return asset.stable_id() < id;
      });
  return found != assets_.end() && found->stable_id() == stable_id &&
                 found->stable_key() == stable_key && found->kind() == kind
             ? &*found
             : nullptr;
}

SandboxAssetPreparedUpdate::SandboxAssetPreparedUpdate() noexcept = default;
SandboxAssetPreparedUpdate::~SandboxAssetPreparedUpdate() = default;
SandboxAssetPreparedUpdate::SandboxAssetPreparedUpdate(
    SandboxAssetPreparedUpdate &&) noexcept = default;
SandboxAssetPreparedUpdate &SandboxAssetPreparedUpdate::operator=(
    SandboxAssetPreparedUpdate &&) noexcept = default;

SandboxAssetPreparedUpdate::SandboxAssetPreparedUpdate(
    std::unique_ptr<State> state) noexcept
    : state_(std::move(state)) {}

SandboxAssetPreparedUpdate::operator bool() const noexcept {
  return state_ != nullptr && state_->candidate != nullptr;
}

const SandboxResolvedAssetSet *
SandboxAssetPreparedUpdate::candidate() const noexcept {
  return state_ == nullptr ? nullptr : state_->candidate.get();
}

SandboxAssetResolver::SandboxAssetResolver() noexcept {
  try {
    lifetime_ = std::make_shared<Lifetime>();
  } catch (const std::bad_alloc &) {
    lifetime_.reset();
  }
}

SandboxAssetResolver::~SandboxAssetResolver() = default;

SandboxAssetPrepareResult SandboxAssetResolver::prepare(
    SandboxAssetRegistryView registry,
    std::span<const SandboxPresentationAssetRequirement> requirements,
    SandboxAssetQuality quality, SandboxAssetSourceIdentity source) noexcept {
  SandboxAssetPrepareResult result{};
  result.error = Error::invalid_registry;

  if (lifetime_ == nullptr) {
    result.error = Error::allocation_failed;
    return result;
  }
  const auto header_error = validate_registry_header(registry, quality);
  if (header_error != Error::none) {
    result.error = header_error;
    return result;
  }
  if (requirements.size() != sandbox_runtime_asset_slot_count) {
    result.error = requirements.size() < sandbox_runtime_asset_slot_count
                       ? Error::missing_asset
                       : Error::capacity_exceeded;
    return result;
  }
  if (!valid_source_identity(source)) {
    result.error = Error::stale_identity;
    return result;
  }
  if (live_ != nullptr) {
    const auto &current_source = live_->identity_.source;
    if (source.runtime_generation < current_source.runtime_generation ||
        source.package_generation < current_source.package_generation ||
        (source.runtime_generation == current_source.runtime_generation &&
         source != current_source)) {
      result.error = Error::stale_identity;
      return result;
    }
    if (source.runtime_generation > current_source.runtime_generation &&
        source.package_generation <= current_source.package_generation) {
      result.error = Error::stale_identity;
      return result;
    }
  }

  const auto current_generation =
      live_ == nullptr ? 0U : live_->identity_.generation;
  const auto next_generation =
      sandbox_next_asset_generation(current_generation);
  if (!next_generation.valid) {
    result.error = Error::generation_exhausted;
    return result;
  }

  try {
    std::uint64_t standard_transfer_bytes{};
    std::uint64_t low_transfer_bytes{};
    std::uint64_t standard_decoded_bytes{};
    std::uint64_t low_decoded_bytes{};
    for (std::size_t index = 0; index < registry.entries.size(); ++index) {
      const auto &entry = registry.entries[index];
      const auto entry_error = validate_entry(entry);
      if (entry_error != Error::none) {
        result.error = entry_error;
        return result;
      }
      for (std::size_t previous = 0; previous < index; ++previous) {
        const auto &other = registry.entries[previous];
        if (entry.stable_id == other.stable_id ||
            entry.stable_key == other.stable_key) {
          result.error = Error::duplicate_asset;
          return result;
        }
        const std::array current_ids{
            entry.standard.artifact_id,
            entry.low.artifact_id,
        };
        const std::array previous_ids{
            other.standard.artifact_id,
            other.low.artifact_id,
        };
        for (const auto current_id : current_ids) {
          for (const auto previous_id : previous_ids) {
            if (current_id == previous_id) {
              result.error = Error::duplicate_asset;
              return result;
            }
          }
        }
      }
      if (entry.standard.artifact_id == entry.low.artifact_id) {
        result.error = Error::duplicate_asset;
        return result;
      }
      standard_transfer_bytes += entry.standard.canonical_bytes.size();
      low_transfer_bytes += entry.low.canonical_bytes.size();
      standard_decoded_bytes += entry.standard.decoded_bytes;
      low_decoded_bytes += entry.low.decoded_bytes;
    }
    if (standard_transfer_bytes > registry.standard_transfer_limit ||
        low_transfer_bytes > registry.low_transfer_limit ||
        standard_decoded_bytes > registry.standard_decoded_limit ||
        low_decoded_bytes > registry.low_decoded_limit) {
      result.error = Error::budget_exceeded;
      return result;
    }

    std::vector<const SandboxAssetRegistryEntryView *> matched{};
    matched.reserve(requirements.size());
    for (std::size_t requirement_index = 0;
         requirement_index < requirements.size(); ++requirement_index) {
      const auto &requirement = requirements[requirement_index];
      const auto id = requirement_id(requirement);
      if (id.empty() || requirement.key == 0 ||
          contracts::stable_content_key(id) != requirement.key) {
        result.error = Error::stable_key_mismatch;
        return result;
      }
      if (!valid_kind(requirement.kind)) {
        result.error = Error::wrong_kind;
        return result;
      }
      for (std::size_t previous = 0; previous < requirement_index; ++previous) {
        const auto &other = requirements[previous];
        if (requirement.key == other.key || id == requirement_id(other)) {
          result.error = Error::duplicate_asset;
          return result;
        }
      }

      const auto by_id = std::find_if(
          registry.entries.begin(), registry.entries.end(),
          [&](const SandboxAssetRegistryEntryView &entry) noexcept {
            return entry.stable_id == id;
          });
      if (by_id == registry.entries.end()) {
        result.error = Error::missing_asset;
        return result;
      }
      if (by_id->stable_key != requirement.key) {
        result.error = Error::stable_key_mismatch;
        return result;
      }
      if (by_id->kind != requirement.kind) {
        result.error = Error::wrong_kind;
        return result;
      }
      matched.push_back(&*by_id);
    }

    std::sort(matched.begin(), matched.end(),
              [](const auto *left, const auto *right) noexcept {
                return left->stable_id < right->stable_id;
              });

    for (std::size_t index = 0; index < matched.size(); ++index) {
      if (index != 0 &&
          (matched[index - 1]->stable_id == matched[index]->stable_id ||
           matched[index - 1]->stable_key == matched[index]->stable_key)) {
        result.error = Error::duplicate_asset;
        return result;
      }
    }

    auto candidate = std::make_unique<SandboxResolvedAssetSet>();
    candidate->identity_ = {
        next_generation.generation,
        registry.fingerprint,
        source,
        quality,
    };
    candidate->assets_.reserve(matched.size());
    for (const auto *entry : matched) {
      const auto &artifact = artifact_for(*entry, quality);
      auto &destination = candidate->assets_.emplace_back();
      destination.stable_id_.assign(entry->stable_id);
      destination.stable_key_ = entry->stable_key;
      destination.kind_ = entry->kind;
      destination.artifact_id_.assign(artifact.artifact_id);
      destination.quality_ = quality;
      destination.sha256_ = artifact.sha256;
      destination.width_ = artifact.width;
      destination.height_ = artifact.height;
      destination.root_anchor_x_ = artifact.root_anchor_x;
      destination.root_anchor_y_ = artifact.root_anchor_y;
      destination.format_ = artifact.format;
      destination.pixel_format_ = artifact.pixel_format;
      destination.color_space_ = artifact.color_space;
      destination.alpha_mode_ = artifact.alpha_mode;
      destination.filter_ = artifact.filter;
      destination.wrap_ = artifact.wrap;
      destination.metrics_ = entry->metrics;
      destination.canonical_bytes_.assign(artifact.canonical_bytes.begin(),
                                          artifact.canonical_bytes.end());
      destination.owned_anchors_.reserve(entry->anchors.size());
      destination.anchor_views_.reserve(entry->anchors.size());
      for (const auto &anchor : entry->anchors) {
        destination.owned_anchors_.push_back({
            std::string{anchor.name},
            anchor.x_mm,
            anchor.y_mm,
            anchor.height_mm,
            std::string{anchor.role},
        });
      }
      for (const auto &anchor : destination.owned_anchors_) {
        destination.anchor_views_.push_back({
            anchor.name,
            anchor.x_mm,
            anchor.y_mm,
            anchor.height_mm,
            anchor.role,
        });
      }
    }

    auto state = std::make_unique<SandboxAssetPreparedUpdate::State>();
    state->origin = lifetime_;
    state->expected_generation = current_generation;
    state->candidate = std::move(candidate);
    result.error = Error::none;
    result.prepared = SandboxAssetPreparedUpdate{std::move(state)};
    return result;
  } catch (const std::bad_alloc &) {
    result.error = Error::allocation_failed;
    return result;
  } catch (const std::length_error &) {
    result.error = Error::capacity_exceeded;
    return result;
  }
}

SandboxAssetCommitResult
SandboxAssetResolver::commit(SandboxAssetPreparedUpdate &&prepared) noexcept {
  static_assert(noexcept(live_.swap(live_)));
  static_assert(std::is_nothrow_destructible_v<SandboxResolvedAssetSet>);

  auto state = std::move(prepared.state_);
  const auto current_identity = identity();
  if (state == nullptr || state->candidate == nullptr) {
    return {
        SandboxAssetCommitDisposition::invalid_prepared_update,
        current_identity,
    };
  }
  if (state->origin.get() != lifetime_.get()) {
    return {
        SandboxAssetCommitDisposition::foreign_resolver,
        current_identity,
    };
  }
  const auto current_generation =
      live_ == nullptr ? 0U : live_->identity_.generation;
  if (state->expected_generation != current_generation) {
    return {
        SandboxAssetCommitDisposition::stale_identity,
        current_identity,
    };
  }

  const auto committed_identity = state->candidate->identity_;
  live_.swap(state->candidate);
  return {
      SandboxAssetCommitDisposition::committed,
      committed_identity,
  };
}

const SandboxResolvedAssetSet *SandboxAssetResolver::live_set() const noexcept {
  return live_.get();
}

SandboxResolvedAssetSetIdentity
SandboxAssetResolver::identity() const noexcept {
  return live_ == nullptr ? SandboxResolvedAssetSetIdentity{}
                          : live_->identity_;
}

} // namespace tgd::presentation
