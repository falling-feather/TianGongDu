#include <tgd/content/sandbox_package.hpp>
#include <tgd/contracts/content_definition.hpp>
#include <tgd/contracts/sha256.hpp>
#include <tgd/integration/sandbox_runtime_coordinator.hpp>
#include <tgd/presentation/sandbox_asset_resolver.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

std::atomic<bool> fail_next_allocation{};

} // namespace

void *operator new(std::size_t size) {
  if (fail_next_allocation.exchange(false)) {
    throw std::bad_alloc{};
  }
  if (void *memory = std::malloc(size)) {
    return memory;
  }
  throw std::bad_alloc{};
}

void operator delete(void *memory) noexcept { std::free(memory); }

void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }

namespace {

using namespace tgd;
using namespace tgd::contracts;
using namespace tgd::presentation;

constexpr Sha256Digest expected_registry_fingerprint{
    0xff, 0x40, 0x72, 0x5e, 0xea, 0x22, 0xaa, 0x26, 0x96, 0x96, 0x4f,
    0xd7, 0x3d, 0x92, 0xba, 0x36, 0xe0, 0xb4, 0x80, 0xb7, 0x38, 0x9f,
    0xf0, 0xa7, 0x40, 0x41, 0xd9, 0xff, 0xe1, 0x4d, 0x0a, 0x6e,
};
constexpr StableActorKey player_actor = 0x706c617965720001ULL;
constexpr std::string_view player_id = "player.system_demo.start";

[[noreturn]] void fail(std::string_view message) {
  std::cerr << message << '\n';
  std::exit(EXIT_FAILURE);
}

void expect(bool condition, std::string_view message) {
  if (!condition) {
    fail(message);
  }
}

[[nodiscard]] SandboxAssetSourceIdentity
source_identity(std::uint32_t package_generation = 1,
                std::uint32_t runtime_generation = 1) {
  constexpr std::array<std::uint8_t, 8> seed{0x53, 0x61, 0x6e, 0x64,
                                             0x62, 0x6f, 0x78, 0x31};
  return {
      package_generation,
      sha256(seed),
      runtime_generation,
  };
}

[[nodiscard]] std::array<SandboxPresentationAssetRequirement,
                         sandbox_runtime_asset_slot_count>
requirements_from(SandboxAssetRegistryView registry, bool reverse = false) {
  std::array<SandboxPresentationAssetRequirement,
             sandbox_runtime_asset_slot_count>
      requirements{};
  expect(registry.entries.size() == requirements.size(),
         "registry cardinality drifted");
  for (std::size_t index = 0; index < requirements.size(); ++index) {
    const auto source_index =
        reverse ? requirements.size() - 1U - index : index;
    const auto &entry = registry.entries[source_index];
    auto &requirement = requirements[index];
    expect(entry.stable_id.size() <= requirement.id_bytes.size(),
           "registry ID exceeds package capacity");
    requirement.key = entry.stable_key;
    requirement.id_byte_count =
        static_cast<std::uint16_t>(entry.stable_id.size());
    std::copy(entry.stable_id.begin(), entry.stable_id.end(),
              requirement.id_bytes.begin());
    requirement.kind = entry.kind;
  }
  return requirements;
}

struct MutableRegistry final {
  explicit MutableRegistry(SandboxAssetRegistryView source) : view(source) {
    expect(source.entries.size() == entries.size(), "mutable registry size");
    std::copy(source.entries.begin(), source.entries.end(), entries.begin());
    view.entries = entries;
  }

  std::array<SandboxAssetRegistryEntryView, sandbox_runtime_asset_slot_count>
      entries{};
  SandboxAssetRegistryView view{};
  std::vector<std::uint8_t> bytes{};
};

void expect_preserved(const SandboxAssetResolver &resolver,
                      const SandboxResolvedAssetSet *expected_set,
                      SandboxResolvedAssetSetIdentity expected_identity,
                      std::string_view message) {
  expect(resolver.live_set() == expected_set, message);
  expect(resolver.identity() == expected_identity, message);
}

void expect_prepare_failure(
    SandboxAssetResolver &resolver, SandboxAssetRegistryView registry,
    std::span<const SandboxPresentationAssetRequirement> requirements,
    SandboxAssetQuality quality, SandboxAssetSourceIdentity source,
    SandboxAssetResolveError error, std::string_view message) {
  const auto *live = resolver.live_set();
  const auto identity = resolver.identity();
  auto result = resolver.prepare(registry, requirements, quality, source);
  expect(result.error == error && !result.prepared &&
             result.prepared.candidate() == nullptr,
         message);
  expect_preserved(resolver, live, identity, message);
}

void check_public_contract() {
  static_assert(!std::is_copy_constructible_v<SandboxAssetResolver>);
  static_assert(!std::is_move_constructible_v<SandboxAssetResolver>);
  static_assert(!std::is_copy_constructible_v<SandboxAssetPreparedUpdate>);
  static_assert(
      std::is_nothrow_move_constructible_v<SandboxAssetPreparedUpdate>);
  static_assert(static_cast<std::uint8_t>(SandboxAssetQuality::standard) == 1 &&
                static_cast<std::uint8_t>(SandboxAssetQuality::low) == 2 &&
                static_cast<std::uint8_t>(SandboxAssetQuality::invalid) == 255);
  static_assert(
      static_cast<std::uint8_t>(SandboxAssetResolveError::allocation_failed) ==
          19 &&
      static_cast<std::uint8_t>(SandboxAssetResolveError::invalid) == 255);
  static_assert(
      static_cast<std::uint8_t>(SandboxAssetCommitDisposition::committed) ==
          1 &&
      static_cast<std::uint8_t>(SandboxAssetCommitDisposition::invalid) == 255);
  constexpr auto first = sandbox_next_asset_generation(0);
  constexpr auto exhausted =
      sandbox_next_asset_generation(std::numeric_limits<std::uint32_t>::max());
  static_assert(first.valid && first.generation == 1);
  static_assert(!exhausted.valid && exhausted.generation == 0);
}

void check_positive_registry_and_ownership() {
  const auto registry = system_sandbox_asset_registry();
  expect(registry.fingerprint == expected_registry_fingerprint &&
             registry.entries.size() == sandbox_runtime_asset_slot_count &&
             registry.maturity == SandboxAssetMaturity::blockout &&
             registry.channel == SandboxAssetChannel::internal_preview &&
             registry.license ==
                 SandboxAssetLicense::review_recorded_not_release_cleared &&
             !registry.preview_ready && !registry.release_allowed,
         "build-generated registry identity or maturity drifted");

  const auto requirements = requirements_from(registry, true);
  SandboxAssetResolver resolver;
  auto prepared = resolver.prepare(
      registry, requirements, SandboxAssetQuality::standard, source_identity());
  expect(prepared.error == SandboxAssetResolveError::none &&
             prepared.prepared && prepared.prepared.candidate() != nullptr &&
             prepared.prepared.candidate()->size() == 12 &&
             resolver.live_set() == nullptr,
         "Standard prepare did not remain private");
  const auto *first_candidate_asset =
      prepared.prepared.candidate()->asset_at(0);
  expect(first_candidate_asset != nullptr &&
             first_candidate_asset->quality() ==
                 SandboxAssetQuality::standard &&
             first_candidate_asset->format() == SandboxAssetFormat::png &&
             first_candidate_asset->pixel_format() ==
                 SandboxAssetPixelFormat::rgba8 &&
             first_candidate_asset->color_space() ==
                 SandboxAssetColorSpace::srgb &&
             first_candidate_asset->alpha_mode() ==
                 SandboxAssetAlphaMode::straight_source_premultiply_on_upload &&
             first_candidate_asset->filter() == SandboxAssetFilter::linear &&
             first_candidate_asset->wrap() == SandboxAssetWrap::clamp &&
             !first_candidate_asset->anchors().empty(),
         "resolved Standard presentation metadata drifted");
  const auto &source_entry = registry.entries.front();
  const auto *copied = prepared.prepared.candidate()->find(
      source_entry.stable_id, source_entry.stable_key, source_entry.kind);
  expect(copied != nullptr &&
             copied->stable_id().data() != source_entry.stable_id.data() &&
             copied->canonical_bytes().data() !=
                 source_entry.standard.canonical_bytes.data() &&
             copied->sha256() == source_entry.standard.sha256,
         "prepared asset retained borrowed registry storage");
  const auto commit = resolver.commit(std::move(prepared.prepared));
  expect(commit.disposition == SandboxAssetCommitDisposition::committed &&
             commit.identity.generation == 1 &&
             commit.identity.registry_fingerprint ==
                 expected_registry_fingerprint &&
             resolver.live_set() != nullptr &&
             resolver.live_set()->identity() == commit.identity,
         "Standard commit failed");

  auto low = resolver.prepare(registry, requirements, SandboxAssetQuality::low,
                              source_identity());
  expect(low.error == SandboxAssetResolveError::none &&
             low.prepared.candidate()->size() == 12 &&
             low.prepared.candidate()->asset_at(0)->quality() ==
                 SandboxAssetQuality::low,
         "explicit Low prepare failed");
  const auto low_commit = resolver.commit(std::move(low.prepared));
  expect(low_commit.disposition == SandboxAssetCommitDisposition::committed &&
             low_commit.identity.generation == 2 &&
             low_commit.identity.quality == SandboxAssetQuality::low,
         "explicit Low commit failed");

  SandboxAssetResolver reordered_resolver;
  const auto ordered = requirements_from(registry);
  auto ordered_result = reordered_resolver.prepare(
      registry, ordered, SandboxAssetQuality::low, source_identity());
  expect(ordered_result.error == SandboxAssetResolveError::none,
         "ordered Low prepare failed");
  const auto *ordered_set = ordered_result.prepared.candidate();
  const auto *reversed_set = resolver.live_set();
  expect(ordered_set != nullptr && reversed_set != nullptr &&
             ordered_set->size() == reversed_set->size(),
         "input order changed resolved cardinality");
  for (std::size_t index = 0; index < ordered_set->size(); ++index) {
    const auto *left = ordered_set->asset_at(index);
    const auto *right = reversed_set->asset_at(index);
    expect(left != nullptr && right != nullptr &&
               left->stable_id() == right->stable_id() &&
               left->artifact_id() == right->artifact_id() &&
               left->sha256() == right->sha256() &&
               left->canonical_bytes().size() ==
                   right->canonical_bytes().size() &&
               std::equal(left->canonical_bytes().begin(),
                          left->canonical_bytes().end(),
                          right->canonical_bytes().begin()),
           "input order changed resolved bytes");
  }
}

void check_temporary_registry_lifetime() {
  const auto source = system_sandbox_asset_registry();
  const auto requirements = requirements_from(source);
  SandboxAssetResolver resolver;
  std::string expected_stable_id;
  std::string expected_anchor_name;
  std::string expected_anchor_role;
  Sha256Digest expected_bytes_hash{};
  SandboxResolvedAssetSetIdentity expected_identity{};
  const char *borrowed_stable_id{};
  const char *borrowed_anchor_name{};
  const std::uint8_t *borrowed_bytes{};

  {
    MutableRegistry temporary{source};
    std::string stable_id{source.entries[0].stable_id};
    std::string standard_artifact_id{source.entries[0].standard.artifact_id};
    std::string low_artifact_id{source.entries[0].low.artifact_id};
    std::vector<std::uint8_t> standard_bytes{
        source.entries[0].standard.canonical_bytes.begin(),
        source.entries[0].standard.canonical_bytes.end()};
    std::vector<std::uint8_t> low_bytes{
        source.entries[0].low.canonical_bytes.begin(),
        source.entries[0].low.canonical_bytes.end()};
    std::vector<std::string> anchor_names;
    std::vector<std::string> anchor_roles;
    std::vector<SandboxAssetAnchorView> anchors;
    anchor_names.reserve(source.entries[0].anchors.size());
    anchor_roles.reserve(source.entries[0].anchors.size());
    anchors.reserve(source.entries[0].anchors.size());
    for (const auto &anchor : source.entries[0].anchors) {
      anchor_names.emplace_back(anchor.name);
      anchor_roles.emplace_back(anchor.role);
      anchors.push_back({anchor_names.back(), anchor.x_mm, anchor.y_mm,
                         anchor.height_mm, anchor_roles.back()});
    }

    auto &entry = temporary.entries[0];
    entry.stable_id = stable_id;
    entry.anchors = anchors;
    entry.standard.artifact_id = standard_artifact_id;
    entry.standard.canonical_bytes = standard_bytes;
    entry.low.artifact_id = low_artifact_id;
    entry.low.canonical_bytes = low_bytes;

    borrowed_stable_id = stable_id.data();
    borrowed_anchor_name = anchor_names.front().data();
    borrowed_bytes = standard_bytes.data();
    expected_stable_id = stable_id;
    expected_anchor_name = anchor_names.front();
    expected_anchor_role = anchor_roles.front();
    expected_bytes_hash = sha256(standard_bytes);

    auto prepared =
        resolver.prepare(temporary.view, requirements,
                         SandboxAssetQuality::standard, source_identity());
    expect(prepared.error == SandboxAssetResolveError::none &&
               prepared.prepared,
           "temporary registry prepare failed");
    const auto committed = resolver.commit(std::move(prepared.prepared));
    expect(committed.disposition == SandboxAssetCommitDisposition::committed &&
               resolver.live_set() != nullptr,
           "temporary registry commit failed");
    expected_identity = committed.identity;
  }

  const auto *asset = resolver.live_set()->find(
      expected_stable_id, requirements[0].key, requirements[0].kind);
  expect(asset != nullptr && resolver.identity() == expected_identity &&
             asset->stable_id() == expected_stable_id &&
             asset->stable_id().data() != borrowed_stable_id &&
             !asset->anchors().empty() &&
             asset->anchors().front().name == expected_anchor_name &&
             asset->anchors().front().role == expected_anchor_role &&
             asset->anchors().front().name.data() != borrowed_anchor_name &&
             asset->canonical_bytes().data() != borrowed_bytes &&
             sha256(asset->canonical_bytes()) == expected_bytes_hash &&
             asset->sha256() == expected_bytes_hash,
         "live set did not own temporary registry strings, anchors, or bytes");
}

void check_registry_and_lookup_failures() {
  const auto registry = system_sandbox_asset_registry();
  const auto requirements = requirements_from(registry);
  SandboxAssetResolver resolver;
  auto first = resolver.prepare(
      registry, requirements, SandboxAssetQuality::standard, source_identity());
  expect(first.error == SandboxAssetResolveError::none, "baseline prepare");
  expect(resolver.commit(std::move(first.prepared)).disposition ==
             SandboxAssetCommitDisposition::committed,
         "baseline commit");

  expect_prepare_failure(resolver, registry, std::span{requirements}.first(11),
                         SandboxAssetQuality::standard, source_identity(),
                         SandboxAssetResolveError::missing_asset,
                         "missing requirement was not precise");
  std::array<SandboxPresentationAssetRequirement, 13> too_many{};
  std::copy(requirements.begin(), requirements.end(), too_many.begin());
  too_many.back() = requirements.front();
  expect_prepare_failure(resolver, registry, too_many,
                         SandboxAssetQuality::standard, source_identity(),
                         SandboxAssetResolveError::capacity_exceeded,
                         "excess requirement was not bounded");
  expect_prepare_failure(resolver, registry, requirements,
                         static_cast<SandboxAssetQuality>(0), source_identity(),
                         SandboxAssetResolveError::invalid_quality,
                         "raw quality zero was accepted");
  expect_prepare_failure(
      resolver, registry, requirements, static_cast<SandboxAssetQuality>(255),
      source_identity(), SandboxAssetResolveError::invalid_quality,
      "raw quality 255 was accepted");

  auto bad_requirements = requirements;
  bad_requirements[0].key ^= 1U;
  expect_prepare_failure(resolver, registry, bad_requirements,
                         SandboxAssetQuality::standard, source_identity(),
                         SandboxAssetResolveError::stable_key_mismatch,
                         "bad Stable key was accepted");
  bad_requirements = requirements;
  bad_requirements[0].kind = SandboxAssetKind::effect;
  expect_prepare_failure(resolver, registry, bad_requirements,
                         SandboxAssetQuality::standard, source_identity(),
                         SandboxAssetResolveError::wrong_kind,
                         "wrong kind was accepted");
  bad_requirements = requirements;
  bad_requirements[0].kind = static_cast<SandboxAssetKind>(0);
  expect_prepare_failure(resolver, registry, bad_requirements,
                         SandboxAssetQuality::standard, source_identity(),
                         SandboxAssetResolveError::wrong_kind,
                         "raw kind zero was accepted");
  bad_requirements = requirements;
  bad_requirements[0].kind = static_cast<SandboxAssetKind>(255);
  expect_prepare_failure(resolver, registry, bad_requirements,
                         SandboxAssetQuality::standard, source_identity(),
                         SandboxAssetResolveError::wrong_kind,
                         "raw kind 255 was accepted");
  bad_requirements = requirements;
  bad_requirements[1] = bad_requirements[0];
  expect_prepare_failure(resolver, registry, bad_requirements,
                         SandboxAssetQuality::standard, source_identity(),
                         SandboxAssetResolveError::duplicate_asset,
                         "duplicate requirement was accepted");

  {
    MutableRegistry changed{registry};
    changed.entries[0].low.sha256[0] ^= 1U;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::artifact_hash_mismatch,
                           "Standard prepare hid broken Low artifact");
  }
  {
    MutableRegistry changed{registry};
    changed.entries[0].standard.sha256[0] ^= 1U;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::low, source_identity(),
                           SandboxAssetResolveError::artifact_hash_mismatch,
                           "Low prepare hid broken Standard artifact");
  }
  {
    MutableRegistry changed{registry};
    changed.entries[0].low.artifact_id =
        changed.entries[1].standard.artifact_id;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::duplicate_asset,
                           "cross-quality artifact ID collision was accepted");
  }
  {
    MutableRegistry changed{registry};
    changed.bytes.assign(changed.entries[0].standard.canonical_bytes.begin(),
                         changed.entries[0].standard.canonical_bytes.begin() +
                             33);
    auto &artifact = changed.entries[0].standard;
    artifact.canonical_bytes = changed.bytes;
    artifact.declared_file_bytes =
        static_cast<std::uint32_t>(changed.bytes.size());
    artifact.sha256 = sha256(changed.bytes);
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::artifact_import_failed,
                           "synced-hash truncated PNG was accepted");
  }
  {
    MutableRegistry changed{registry};
    auto &artifact = changed.entries[0].standard;
    artifact.root_anchor_x += 1U;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::missing_artifact,
                           "root anchor drift was accepted");
  }
  {
    MutableRegistry changed{registry};
    changed.entries[0].standard.color_space = SandboxAssetColorSpace::invalid;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::missing_artifact,
                           "format drift was accepted");
  }
  {
    MutableRegistry changed{registry};
    changed.entries[0].metrics.present |= 0x8000U;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::invalid_registry,
                           "unknown metrics bit was accepted");
  }
  {
    MutableRegistry changed{registry};
    changed.entries[0].metrics.width_mm = 0;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::invalid_registry,
                           "nonpositive present metric was accepted");
  }
  {
    MutableRegistry changed{registry};
    changed.view.low_transfer_limit = 1;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::budget_exceeded,
                           "Standard prepare hid Low budget overflow");
  }
  {
    MutableRegistry changed{registry};
    changed.view.channel = SandboxAssetChannel::invalid;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::channel_blocked,
                           "blocked channel was accepted");
  }
  {
    MutableRegistry changed{registry};
    changed.view.license = SandboxAssetLicense::invalid;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::license_blocked,
                           "blocked license was accepted");
  }
  {
    MutableRegistry changed{registry};
    changed.view.preview_ready = true;
    expect_prepare_failure(resolver, changed.view, requirements,
                           SandboxAssetQuality::standard, source_identity(),
                           SandboxAssetResolveError::maturity_blocked,
                           "Preview-ready overclaim was accepted");
  }

  auto newer = source_identity(2, 2);
  newer.package_checksum[0] ^= 1U;
  auto update = resolver.prepare(registry, requirements,
                                 SandboxAssetQuality::standard, newer);
  expect(update.error == SandboxAssetResolveError::none, "newer source");
  expect(resolver.commit(std::move(update.prepared)).disposition ==
             SandboxAssetCommitDisposition::committed,
         "newer source commit");
  auto runtime_new_package_old = source_identity(1, 3);
  runtime_new_package_old.package_checksum[0] ^= 2U;
  expect_prepare_failure(
      resolver, registry, requirements, SandboxAssetQuality::standard,
      runtime_new_package_old, SandboxAssetResolveError::stale_identity,
      "runtime advance accepted regressed package generation");
  auto same_runtime_conflict = source_identity(3, 2);
  same_runtime_conflict.package_checksum[0] ^= 3U;
  expect_prepare_failure(resolver, registry, requirements,
                         SandboxAssetQuality::standard, same_runtime_conflict,
                         SandboxAssetResolveError::stale_identity,
                         "same runtime accepted conflicting package identity");

  const auto *live = resolver.live_set();
  const auto identity = resolver.identity();
  fail_next_allocation = true;
  auto allocation =
      resolver.prepare(registry, requirements, SandboxAssetQuality::standard,
                       source_identity(3, 3));
  fail_next_allocation = false;
  expect(allocation.error == SandboxAssetResolveError::allocation_failed &&
             !allocation.prepared,
         "allocation failure did not fail closed");
  expect_preserved(resolver, live, identity,
                   "allocation failure changed last-valid");
}

void check_token_provenance() {
  const auto registry = system_sandbox_asset_registry();
  const auto requirements = requirements_from(registry);

  SandboxAssetResolver resolver;
  auto first = resolver.prepare(
      registry, requirements, SandboxAssetQuality::standard, source_identity());
  auto second = resolver.prepare(
      registry, requirements, SandboxAssetQuality::standard, source_identity());
  expect(resolver.commit(std::move(first.prepared)).disposition ==
             SandboxAssetCommitDisposition::committed,
         "first competing token failed");
  const auto *first_live = resolver.live_set();
  const auto first_identity = resolver.identity();
  expect(resolver.commit(std::move(second.prepared)).disposition ==
             SandboxAssetCommitDisposition::stale_identity,
         "second competing token was accepted");
  expect_preserved(resolver, first_live, first_identity,
                   "stale competing token changed last-valid");

  SandboxAssetResolver foreign;
  auto foreign_token = resolver.prepare(
      registry, requirements, SandboxAssetQuality::low, source_identity());
  expect(foreign.commit(std::move(foreign_token.prepared)).disposition ==
             SandboxAssetCommitDisposition::foreign_resolver,
         "foreign resolver accepted token");
  expect(foreign.commit(std::move(foreign_token.prepared)).disposition ==
             SandboxAssetCommitDisposition::invalid_prepared_update,
         "consumed foreign token was reusable");
  expect(foreign.live_set() == nullptr, "foreign token published state");

  auto moved_source = resolver.prepare(
      registry, requirements, SandboxAssetQuality::low, source_identity());
  SandboxAssetPreparedUpdate moved{std::move(moved_source.prepared)};
  expect(resolver.commit(std::move(moved_source.prepared)).disposition ==
             SandboxAssetCommitDisposition::invalid_prepared_update,
         "moved-from token was accepted");
  expect(resolver.commit(std::move(moved)).disposition ==
             SandboxAssetCommitDisposition::committed,
         "moved token did not commit");
  expect(resolver.commit(std::move(moved)).disposition ==
             SandboxAssetCommitDisposition::invalid_prepared_update,
         "committed token was reusable");

  alignas(SandboxAssetResolver)
      std::array<std::byte, sizeof(SandboxAssetResolver)>
          storage{};
  auto *old_resolver = std::construct_at(
      reinterpret_cast<SandboxAssetResolver *>(storage.data()));
  auto old_token = old_resolver->prepare(
      registry, requirements, SandboxAssetQuality::standard, source_identity());
  std::destroy_at(old_resolver);
  auto *replacement = std::construct_at(
      reinterpret_cast<SandboxAssetResolver *>(storage.data()));
  expect(replacement->commit(std::move(old_token.prepared)).disposition ==
             SandboxAssetCommitDisposition::foreign_resolver,
         "same-address replacement accepted cross-lifetime token");
  expect(replacement->commit(std::move(old_token.prepared)).disposition ==
                 SandboxAssetCommitDisposition::invalid_prepared_update &&
             replacement->live_set() == nullptr,
         "cross-lifetime token was reusable or changed replacement");
  std::destroy_at(replacement);
}

[[nodiscard]] std::vector<std::uint8_t> read_bytes(const char *path) {
  std::ifstream input{path, std::ios::binary | std::ios::ate};
  expect(input.is_open(), "canonical package could not be opened");
  const auto end = input.tellg();
  expect(end > 0, "canonical package was empty");
  std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
  input.seekg(0, std::ios::beg);
  input.read(reinterpret_cast<char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  expect(input.good(), "canonical package read failed");
  return bytes;
}

void check_runtime_composition(const char *package_path) {
  auto bytes = read_bytes(package_path);
  const auto decoded = content::decode_sandbox_package(bytes);
  expect(decoded.validation.valid() && decoded.document != nullptr,
         "composition package did not decode");
  const auto &assets = decoded.document->definition().assets;
  expect(assets.size() == sandbox_runtime_asset_slot_count, "package assets");
  std::array<SandboxPresentationAssetRequirement,
             sandbox_runtime_asset_slot_count>
      requirements{};
  for (std::size_t index = 0; index < assets.size(); ++index) {
    requirements[index].key = assets[index].id.key;
    requirements[index].id_byte_count =
        static_cast<std::uint16_t>(assets[index].id.name.size());
    std::copy(assets[index].id.name.begin(), assets[index].id.name.end(),
              requirements[index].id_bytes.begin());
    requirements[index].kind = assets[index].kind;
  }

  const auto package_identity = content::SandboxPackagePublicationIdentity{
      1,
      decoded.document->fingerprint(),
  };
  const SandboxAssetSourceIdentity source{
      package_identity.generation(),
      package_identity.checksum(),
      1,
  };
  SandboxAssetResolver resolver;
  auto prepared =
      resolver.prepare(system_sandbox_asset_registry(), requirements,
                       SandboxAssetQuality::standard, source);
  expect(prepared.error == SandboxAssetResolveError::none &&
             resolver.live_set() == nullptr,
         "asset prepare changed live state before GAME publish");

  integration::SandboxRuntimeCoordinator coordinator;
  const gameplay::SandboxPlayerRuntimeBinding invalid_player{
      content_id(player_id),
      0,
  };
  const auto game_failed =
      coordinator.publish({package_identity, bytes}, invalid_player);
  expect(
      game_failed.disposition == integration::SandboxRuntimePublishDisposition::
                                     session_prepare_failed &&
          resolver.live_set() == nullptr && !coordinator.snapshot().initialized,
      "GAME failure committed an asset set or runtime aggregate");
  prepared.prepared = {};

  prepared = resolver.prepare(system_sandbox_asset_registry(), requirements,
                              SandboxAssetQuality::standard, source);
  const gameplay::SandboxPlayerRuntimeBinding player{
      content_id(player_id),
      player_actor,
  };
  const auto game_published =
      coordinator.publish({package_identity, bytes}, player);
  expect(game_published.disposition ==
                 integration::SandboxRuntimePublishDisposition::published &&
             resolver.live_set() == nullptr,
         "GAME did not publish before asset commit");
  expect(resolver.commit(std::move(prepared.prepared)).disposition ==
                 SandboxAssetCommitDisposition::committed &&
             resolver.live_set() != nullptr &&
             resolver.identity().source.package_generation ==
                 coordinator.snapshot().package_generation &&
             resolver.identity().source.package_checksum ==
                 coordinator.snapshot().package_checksum &&
             resolver.identity().source.runtime_generation ==
                 coordinator.snapshot().runtime_generation,
         "successful composition did not commit matching identities");

  const auto runtime_snapshot = coordinator.snapshot();
  const auto *live_set = resolver.live_set();
  const auto live_identity = resolver.identity();
  MutableRegistry broken{system_sandbox_asset_registry()};
  broken.entries[0].standard.sha256[0] ^= 1U;
  expect_prepare_failure(resolver, broken.view, requirements,
                         SandboxAssetQuality::standard, source,
                         SandboxAssetResolveError::artifact_hash_mismatch,
                         "composition asset failure was accepted");
  expect(coordinator.snapshot() == runtime_snapshot &&
             resolver.live_set() == live_set &&
             resolver.identity() == live_identity,
         "asset prepare failure changed GAME or resolved last-valid");
}

} // namespace

int main(int argc, char **argv) {
  check_public_contract();
  check_positive_registry_and_ownership();
  check_temporary_registry_lifetime();
  check_registry_and_lookup_failures();
  check_token_provenance();
#if defined(TGD_SYSTEM_DEMO_CANONICAL_PACKAGE_PATH)
  static_cast<void>(argc);
  static_cast<void>(argv);
  check_runtime_composition(TGD_SYSTEM_DEMO_CANONICAL_PACKAGE_PATH);
#else
  if (argc == 2) {
    check_runtime_composition(argv[1]);
  } else {
    expect(argc == 1, "expected zero or one canonical package path");
  }
#endif
  std::cout << "sandbox asset resolver probe passed\n";
  return EXIT_SUCCESS;
}
