#include <tgd/content/sandbox_package_service_abi.h>

#include <tgd/content/sandbox_package_provider.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace tgd::contracts;
using namespace tgd::content;

constexpr std::size_t service_capacity = TGD_SANDBOX_COMPILER_SERVICE_MAX_SERVICES;
constexpr std::size_t request_capacity = TGD_SANDBOX_COMPILER_SERVICE_MAX_REQUESTS;

struct Service final {
    SandboxPackageProvider provider{};
};

struct ServiceSlot final {
    alignas(Service) std::array<std::byte, sizeof(Service)> storage{};
    std::uint32_t serial{};
    bool occupied{};

    [[nodiscard]] Service* value() noexcept {
        return occupied ? std::launder(reinterpret_cast<Service*>(storage.data())) : nullptr;
    }
    void create() {
        std::construct_at(reinterpret_cast<Service*>(storage.data()));
        occupied = true;
    }
    void destroy() noexcept {
        if (auto* service = value(); service != nullptr) {
            std::destroy_at(service);
            occupied = false;
        }
    }
};

struct OwnedRequest final {
    tgd_sandbox_service_handle owner{};
    tgd_sandbox_service_identity expected{};
    std::vector<std::string> strings{};
    std::optional<tgd_sandbox_service_metadata> metadata{};
    std::optional<tgd_sandbox_service_player> player{};
    std::vector<tgd_sandbox_service_region> regions{};
    std::vector<tgd_sandbox_service_asset> assets{};
    std::vector<tgd_sandbox_service_placement> actors{};
    std::vector<tgd_sandbox_service_ground_blocker> blockers{};
    std::vector<tgd_sandbox_service_placement> safe_points{};
    std::vector<tgd_sandbox_service_placement> interactions{};
    std::vector<tgd_sandbox_service_placement> mechanisms{};
    std::vector<tgd_sandbox_service_wave> waves{};
    std::vector<tgd_sandbox_service_wave_spawn> wave_spawns{};
    std::vector<tgd_sandbox_service_objective> objectives{};
    std::vector<tgd_sandbox_service_interaction_binding> interaction_bindings{};
    std::vector<tgd_sandbox_service_mechanism_binding> mechanism_bindings{};
    std::size_t copied_utf8_bytes{};
};

struct RequestSlot final {
    std::uint32_t serial{};
    std::unique_ptr<OwnedRequest> request{};
};

std::array<ServiceSlot, service_capacity> services{};
std::array<RequestSlot, request_capacity> requests{};

[[nodiscard]] constexpr std::uint64_t make_handle(
    std::uint32_t serial,
    std::size_t index
) noexcept {
    return (static_cast<std::uint64_t>(serial) << 32U) |
           static_cast<std::uint64_t>(index + 1U);
}

[[nodiscard]] constexpr std::size_t handle_index(std::uint64_t handle) noexcept {
    const auto low = static_cast<std::uint32_t>(handle);
    return low == 0 ? std::numeric_limits<std::size_t>::max()
                    : static_cast<std::size_t>(low - 1U);
}

[[nodiscard]] constexpr std::uint32_t handle_serial(std::uint64_t handle) noexcept {
    return static_cast<std::uint32_t>(handle >> 32U);
}

[[nodiscard]] Service* find_service(tgd_sandbox_service_handle handle) noexcept {
    const auto index = handle_index(handle);
    if (index >= services.size() || handle_serial(handle) == 0) return nullptr;
    auto& slot = services[index];
    return slot.occupied && slot.serial == handle_serial(handle) ? slot.value() : nullptr;
}

[[nodiscard]] RequestSlot* find_request_slot(tgd_sandbox_request_handle handle) noexcept {
    const auto index = handle_index(handle);
    if (index >= requests.size() || handle_serial(handle) == 0) return nullptr;
    auto& slot = requests[index];
    return slot.request != nullptr && slot.serial == handle_serial(handle) ? &slot : nullptr;
}

[[nodiscard]] OwnedRequest* find_owned_request(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    int32_t& status
) noexcept {
    if (find_service(service) == nullptr) {
        status = TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_SERVICE;
        return nullptr;
    }
    auto* slot = find_request_slot(request);
    if (slot == nullptr) {
        status = TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_REQUEST;
        return nullptr;
    }
    if (slot->request->owner != service) {
        status = TGD_SANDBOX_SERVICE_TRANSPORT_FOREIGN_REQUEST;
        return nullptr;
    }
    status = TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
    return slot->request.get();
}

[[nodiscard]] SandboxPackagePublicationIdentity to_identity(
    const tgd_sandbox_service_identity& value
) noexcept {
    Sha256Digest checksum{};
    std::copy(std::begin(value.checksum), std::end(value.checksum), checksum.begin());
    return {value.generation, checksum};
}

void write_identity(
    const SandboxPackagePublicationIdentity& value,
    tgd_sandbox_service_identity& output
) noexcept {
    output.generation = value.generation();
    std::copy(value.checksum().begin(), value.checksum().end(), std::begin(output.checksum));
}

[[nodiscard]] bool valid_ref(
    const OwnedRequest& request,
    tgd_sandbox_request_string_ref ref
) noexcept {
    return ref != 0 && static_cast<std::size_t>(ref) <= request.strings.size();
}

[[nodiscard]] std::string_view get_string(
    const OwnedRequest& request,
    tgd_sandbox_request_string_ref ref
) noexcept {
    return valid_ref(request, ref) ? std::string_view{request.strings[ref - 1U]}
                                   : std::string_view{};
}

[[nodiscard]] SandboxBoundsMm to_bounds(const tgd_sandbox_service_bounds& value) noexcept {
    return {value.min_x, value.max_x, value.min_y, value.max_y,
            value.min_height, value.max_height,
            value.min_floor_layer, value.max_floor_layer};
}

[[nodiscard]] GroundPoseMm to_pose(const tgd_sandbox_service_pose& value) noexcept {
    return {value.x, value.y, value.height, value.floor_layer};
}

template <typename Record>
[[nodiscard]] bool append_record(
    std::vector<Record>& records,
    std::size_t limit,
    const Record& record
) {
    if (records.size() >= limit) return false;
    records.push_back(record);
    return true;
}

struct RuntimeProjection final {
    std::vector<SandboxAuthoringRegion> regions{};
    std::vector<SandboxAuthoringAsset> assets{};
    std::vector<SandboxAuthoringPlacement> actors{};
    std::vector<SandboxAuthoringGroundBlocker> blockers{};
    std::vector<SandboxAuthoringPlacement> safe_points{};
    std::vector<SandboxAuthoringPlacement> interactions{};
    std::vector<SandboxAuthoringPlacement> mechanisms{};
    std::vector<SandboxAuthoringWave> waves{};
    std::vector<SandboxAuthoringWaveSpawn> wave_spawns{};
    std::vector<SandboxAuthoringObjective> objectives{};
    std::vector<SandboxAuthoringInteractionBinding> interaction_bindings{};
    std::vector<SandboxAuthoringMechanismBinding> mechanism_bindings{};
    SandboxAuthoringRuntimeView view{};

    explicit RuntimeProjection(const OwnedRequest& request) {
        const auto& metadata = *request.metadata;
        const auto& player = *request.player;
        for (const auto& value : request.regions)
            regions.push_back({get_string(request, value.id), to_bounds(value.bounds)});
        for (const auto& value : request.assets)
            assets.push_back({get_string(request, value.id),
                              static_cast<SandboxAssetKind>(value.asset_kind)});
        const auto project_placement = [&](const auto& value) {
            return SandboxAuthoringPlacement{
                get_string(request, value.id), get_string(request, value.region_id),
                get_string(request, value.asset_id), to_pose(value.pose),
                value.facing_millidegrees,
            };
        };
        for (const auto& value : request.actors) actors.push_back(project_placement(value));
        for (const auto& value : request.blockers) {
            blockers.push_back({
                get_string(request, value.id), get_string(request, value.region_id),
                get_string(request, value.asset_id), value.min_x, value.max_x,
                value.min_y, value.max_y, value.min_height, value.max_height,
                value.floor_layer,
            });
        }
        for (const auto& value : request.safe_points) safe_points.push_back(project_placement(value));
        for (const auto& value : request.interactions) interactions.push_back(project_placement(value));
        for (const auto& value : request.mechanisms) mechanisms.push_back(project_placement(value));
        for (const auto& value : request.waves) {
            waves.push_back({
                get_string(request, value.id), get_string(request, value.region_id),
                get_string(request, value.predecessor_wave_id),
                {static_cast<SandboxTriggerKind>(value.trigger_kind),
                 get_string(request, value.trigger_target_id)},
            });
        }
        for (const auto& value : request.wave_spawns) {
            wave_spawns.push_back({
                get_string(request, value.wave_id), get_string(request, value.actor_id),
                value.delay_ticks, value.spawn_order,
            });
        }
        for (const auto& value : request.objectives) {
            objectives.push_back({
                get_string(request, value.id), get_string(request, value.region_id),
                get_string(request, value.predecessor_objective_id),
                {static_cast<SandboxObjectiveCompletionKind>(value.completion_kind),
                 get_string(request, value.completion_target_id)},
            });
        }
        for (const auto& value : request.interaction_bindings) {
            interaction_bindings.push_back({
                get_string(request, value.interaction_id),
                static_cast<SandboxInteractionOperation>(value.operation), value.range_mm,
                get_string(request, value.target_mechanism_id),
            });
        }
        for (const auto& value : request.mechanism_bindings) {
            mechanism_bindings.push_back({
                get_string(request, value.mechanism_id),
                static_cast<SandboxMechanismActivation>(value.activation),
                get_string(request, value.target_ground_blocker_id),
            });
        }
        view = {
            get_string(request, metadata.package_id),
            get_string(request, metadata.sandbox_id),
            to_bounds(metadata.bounds),
            get_string(request, metadata.completion_objective_id),
            {
                get_string(request, player.id), get_string(request, player.region_id),
                get_string(request, player.asset_id),
                get_string(request, player.initial_safe_point_id),
                to_pose(player.pose), player.facing_millidegrees,
            },
            regions, assets, actors, blockers, safe_points, interactions, mechanisms,
            waves, wave_spawns, objectives, interaction_bindings, mechanism_bindings,
        };
    }
};

void append_u8(std::vector<std::uint8_t>& bytes, std::uint8_t value) { bytes.push_back(value); }
void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) {
    append_u8(bytes, static_cast<std::uint8_t>(value));
    append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
}
void append_u32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) append_u8(bytes, static_cast<std::uint8_t>(value >> shift));
}
void patch_u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes[offset + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}
void append_key(std::vector<std::uint8_t>& bytes, StableContentKey key) {
    append_u32(bytes, static_cast<std::uint32_t>(key));
    append_u32(bytes, static_cast<std::uint32_t>(key >> 32U));
}

[[nodiscard]] bool output_id_is_transport_safe(std::string_view value) noexcept {
    if (value.empty() || value.size() > sandbox_pack_max_id_bytes) return false;
    std::size_t index = 0;
    while (index < value.size()) {
        const auto first = static_cast<std::uint8_t>(value[index]);
        std::uint32_t code_point = 0;
        std::size_t continuation_count = 0;
        if (first <= 0x7fU) {
            code_point = first;
        } else if (first >= 0xc2U && first <= 0xdfU) {
            code_point = first & 0x1fU;
            continuation_count = 1;
        } else if (first >= 0xe0U && first <= 0xefU) {
            code_point = first & 0x0fU;
            continuation_count = 2;
        } else if (first >= 0xf0U && first <= 0xf4U) {
            code_point = first & 0x07U;
            continuation_count = 3;
        } else {
            return false;
        }
        if (index + continuation_count >= value.size()) return false;
        for (std::size_t offset = 1; offset <= continuation_count; ++offset) {
            const auto next = static_cast<std::uint8_t>(value[index + offset]);
            if ((next & 0xc0U) != 0x80U) return false;
            code_point = (code_point << 6U) | (next & 0x3fU);
        }
        if ((continuation_count == 2 && code_point < 0x800U) ||
            (continuation_count == 3 && code_point < 0x1'0000U) ||
            (code_point >= 0xd800U && code_point <= 0xdfffU) ||
            code_point > 0x10'ffffU) {
            return false;
        }
        index += continuation_count + 1;
    }
    return true;
}

[[nodiscard]] std::string_view resolve_id(
    const OwnedRequest& request,
    StableContentKey key
) noexcept {
    if (key == 0) return {};
    std::string_view found{};
    for (const auto& value : request.strings) {
        if (!output_id_is_transport_safe(value) || stable_content_key(value) != key) continue;
        if (found.empty()) found = value;
        else if (found != value) return {};
    }
    return found;
}

[[nodiscard]] std::vector<std::uint8_t> serialize_result(
    std::uint8_t outcome,
    SandboxPackageCompileStatus compile_status,
    const SandboxPackageValidation& validation,
    const SandboxPackagePublicationIdentity& identity,
    const OwnedRequest& request
) {
    std::vector<std::uint8_t> bytes(TGD_SANDBOX_COMPILER_SERVICE_RESULT_HEADER_BYTES, 0);
    std::vector<std::pair<std::string_view, std::string_view>> ids;
    ids.reserve(validation.diagnostics.size());
    for (const auto& diagnostic : validation.diagnostics) {
        ids.emplace_back(resolve_id(request, diagnostic.subject),
                         resolve_id(request, diagnostic.related));
    }
    const auto diagnostic_offset = static_cast<std::uint32_t>(bytes.size());
    for (std::size_t index = 0; index < validation.diagnostics.size(); ++index) {
        const auto& diagnostic = validation.diagnostics[index];
        append_u16(bytes, static_cast<std::uint16_t>(diagnostic.code));
        append_u8(bytes, static_cast<std::uint8_t>(sandbox_diagnostic_severity(diagnostic.code)));
        append_u8(bytes, static_cast<std::uint8_t>(diagnostic.domain));
        append_u16(bytes, static_cast<std::uint16_t>(diagnostic.field));
        std::uint16_t flags = 0;
        if (!ids[index].first.empty()) flags |= 1U;
        if (!ids[index].second.empty()) flags |= 2U;
        append_u16(bytes, flags);
        append_u32(bytes, diagnostic.record_index);
        append_key(bytes, diagnostic.subject);
        append_key(bytes, diagnostic.related);
        append_u32(bytes, 0); append_u32(bytes, static_cast<std::uint32_t>(ids[index].first.size()));
        append_u32(bytes, 0); append_u32(bytes, static_cast<std::uint32_t>(ids[index].second.size()));
        append_u32(bytes, 0);
    }
    const auto id_offset = static_cast<std::uint32_t>(bytes.size());
    for (std::size_t index = 0; index < ids.size(); ++index) {
        const auto record = static_cast<std::size_t>(diagnostic_offset) +
                            index * TGD_SANDBOX_COMPILER_SERVICE_DIAGNOSTIC_BYTES;
        if (!ids[index].first.empty()) {
            patch_u32(bytes, record + 28U, static_cast<std::uint32_t>(bytes.size()));
            bytes.insert(bytes.end(), ids[index].first.begin(), ids[index].first.end());
        }
        if (!ids[index].second.empty()) {
            patch_u32(bytes, record + 36U, static_cast<std::uint32_t>(bytes.size()));
            bytes.insert(bytes.end(), ids[index].second.begin(), ids[index].second.end());
        }
    }
    const auto binding = validation.gameplay_binding_validation;
    const auto binding_subject_id = resolve_id(request, binding.subject);
    const auto binding_related_id = resolve_id(request, binding.related);
    std::uint16_t binding_flags = 0;
    if (!binding_subject_id.empty()) {
        binding_flags |= TGD_SANDBOX_SERVICE_DIAGNOSTIC_HAS_SUBJECT_ID;
        patch_u32(bytes, 88, static_cast<std::uint32_t>(bytes.size()));
        patch_u32(bytes, 92, static_cast<std::uint32_t>(binding_subject_id.size()));
        bytes.insert(bytes.end(), binding_subject_id.begin(), binding_subject_id.end());
    }
    if (!binding_related_id.empty()) {
        binding_flags |= TGD_SANDBOX_SERVICE_DIAGNOSTIC_HAS_RELATED_ID;
        patch_u32(bytes, 96, static_cast<std::uint32_t>(bytes.size()));
        patch_u32(bytes, 100, static_cast<std::uint32_t>(binding_related_id.size()));
        bytes.insert(bytes.end(), binding_related_id.begin(), binding_related_id.end());
    }
    if (bytes.size() > TGD_SANDBOX_COMPILER_SERVICE_MAX_RESULT_BYTES ||
        validation.diagnostics.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("sandbox compiler service result too large");
    }
    bytes[0] = 1;
    bytes[1] = outcome;
    bytes[2] = static_cast<std::uint8_t>(compile_status);
    bytes[3] = static_cast<std::uint8_t>(validation.error);
    patch_u32(bytes, 4, identity.generation());
    patch_u32(bytes, 8, static_cast<std::uint32_t>(validation.diagnostics.size()));
    std::copy(identity.checksum().begin(), identity.checksum().end(), bytes.begin() + 12);
    bytes[44] = static_cast<std::uint8_t>(binding.code);
    bytes[45] = static_cast<std::uint8_t>(binding.domain);
    bytes[46] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(binding.field));
    bytes[47] = static_cast<std::uint8_t>(static_cast<std::uint16_t>(binding.field) >> 8U);
    patch_u32(bytes, 48, binding.record_index);
    for (unsigned shift = 0; shift < 64; shift += 8) bytes[52 + shift / 8] = static_cast<std::uint8_t>(binding.subject >> shift);
    for (unsigned shift = 0; shift < 64; shift += 8) bytes[60 + shift / 8] = static_cast<std::uint8_t>(binding.related >> shift);
    patch_u32(bytes, 68, diagnostic_offset);
    patch_u32(bytes, 72, id_offset);
    patch_u32(bytes, 76, static_cast<std::uint32_t>(bytes.size()));
    bytes[80] = static_cast<std::uint8_t>(TGD_SANDBOX_COMPILER_SERVICE_ABI_MAJOR);
    bytes[82] = static_cast<std::uint8_t>(TGD_SANDBOX_COMPILER_SERVICE_ABI_MINOR);
    bytes[84] = static_cast<std::uint8_t>(binding_flags);
    bytes[85] = static_cast<std::uint8_t>(binding_flags >> 8U);
    return bytes;
}

[[nodiscard]] bool metadata_valid(const OwnedRequest& request) noexcept {
    if (!request.metadata || !request.player) return false;
    const auto& metadata = *request.metadata;
    const auto& player = *request.player;
    return valid_ref(request, metadata.package_id) && valid_ref(request, metadata.sandbox_id) &&
           valid_ref(request, metadata.completion_objective_id) &&
           valid_ref(request, player.id) && valid_ref(request, player.region_id) &&
           valid_ref(request, player.asset_id) && valid_ref(request, player.initial_safe_point_id) &&
           player.pose.reserved == 0;
}

template <typename Record, typename Validate>
int32_t append_typed(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    const Record* record,
    std::vector<Record> OwnedRequest::*member,
    std::size_t limit,
    Validate validate
) {
    int32_t status{};
    auto* owned = find_owned_request(service, request, status);
    if (owned == nullptr) return status;
    if (record == nullptr || !validate(*owned, *record))
        return TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST;
    try {
        return append_record(owned->*member, limit, *record)
                   ? TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED
                   : TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED;
    } catch (const std::bad_alloc&) {
        return TGD_SANDBOX_SERVICE_TRANSPORT_ALLOCATION_FAILED;
    }
}

template <std::size_t Size>
[[nodiscard]] bool all_zero(const std::uint8_t (&reserved)[Size]) noexcept {
    return std::all_of(std::begin(reserved), std::end(reserved),
                       [](std::uint8_t value) { return value == 0; });
}

}  // namespace

extern "C" {

uint32_t tgd_sandbox_compiler_service_abi_version(void) {
    return (static_cast<std::uint32_t>(TGD_SANDBOX_COMPILER_SERVICE_ABI_MAJOR) << 16U) |
           TGD_SANDBOX_COMPILER_SERVICE_ABI_MINOR;
}

int32_t tgd_sandbox_compiler_service_create(tgd_sandbox_service_handle* output) {
    if (output == nullptr) return TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST;
    *output = 0;
    try {
        for (std::size_t index = 0; index < services.size(); ++index) {
            auto& slot = services[index];
            if (slot.occupied || slot.serial == UINT32_MAX) continue;
            ++slot.serial;
            slot.create();
            *output = make_handle(slot.serial, index);
            return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
        }
        return TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED;
    } catch (const std::bad_alloc&) {
        return TGD_SANDBOX_SERVICE_TRANSPORT_ALLOCATION_FAILED;
    }
}

int32_t tgd_sandbox_compiler_service_destroy(tgd_sandbox_service_handle service) {
    const auto index = handle_index(service);
    auto* value = find_service(service);
    if (value == nullptr) return TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_SERVICE;
    for (auto& request : requests) {
        if (request.request != nullptr && request.request->owner == service) request.request.reset();
    }
    services[index].destroy();
    return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
}

int32_t tgd_sandbox_compiler_service_read_identity(
    tgd_sandbox_service_handle service,
    tgd_sandbox_service_identity* output
) {
    auto* value = find_service(service);
    if (value == nullptr) return TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_SERVICE;
    if (output == nullptr) return TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST;
    write_identity(value->provider.identity(), *output);
    return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
}

int32_t tgd_sandbox_compile_request_create(
    tgd_sandbox_service_handle service,
    const tgd_sandbox_service_identity* expected,
    tgd_sandbox_request_handle* output
) {
    if (find_service(service) == nullptr) return TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_SERVICE;
    if (expected == nullptr || output == nullptr) return TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST;
    *output = 0;
    try {
        for (std::size_t index = 0; index < requests.size(); ++index) {
            auto& slot = requests[index];
            if (slot.request != nullptr || slot.serial == UINT32_MAX) continue;
            ++slot.serial;
            slot.request = std::make_unique<OwnedRequest>();
            slot.request->owner = service;
            slot.request->expected = *expected;
            *output = make_handle(slot.serial, index);
            return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
        }
        return TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED;
    } catch (const std::bad_alloc&) {
        return TGD_SANDBOX_SERVICE_TRANSPORT_ALLOCATION_FAILED;
    }
}

int32_t tgd_sandbox_compile_request_cancel(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request
) {
    int32_t status{};
    if (find_owned_request(service, request, status) == nullptr) return status;
    find_request_slot(request)->request.reset();
    return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
}

int32_t tgd_sandbox_compile_request_copy_utf8(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    const uint8_t* bytes,
    uint32_t length,
    tgd_sandbox_request_string_ref* output
) {
    int32_t status{};
    auto* owned = find_owned_request(service, request, status);
    if (owned == nullptr) return status;
    if ((bytes == nullptr && length != 0) || output == nullptr)
        return TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST;
    if (owned->copied_utf8_bytes + length > TGD_SANDBOX_COMPILER_SERVICE_MAX_COPIED_UTF8_BYTES ||
        owned->strings.size() >= UINT32_MAX)
        return TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED;
    try {
        if (length == 0) owned->strings.emplace_back();
        else owned->strings.emplace_back(reinterpret_cast<const char*>(bytes), length);
        owned->copied_utf8_bytes += length;
        *output = static_cast<std::uint32_t>(owned->strings.size());
        return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
    } catch (const std::bad_alloc&) {
        return TGD_SANDBOX_SERVICE_TRANSPORT_ALLOCATION_FAILED;
    }
}

int32_t tgd_sandbox_compile_request_set_metadata(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    const tgd_sandbox_service_metadata* metadata
) {
    int32_t status{};
    auto* owned = find_owned_request(service, request, status);
    if (owned == nullptr) return status;
    if (metadata == nullptr || owned->metadata || !valid_ref(*owned, metadata->package_id) ||
        !valid_ref(*owned, metadata->sandbox_id) ||
        !valid_ref(*owned, metadata->completion_objective_id))
        return TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST;
    owned->metadata = *metadata;
    return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
}

int32_t tgd_sandbox_compile_request_set_player(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    const tgd_sandbox_service_player* player
) {
    int32_t status{};
    auto* owned = find_owned_request(service, request, status);
    if (owned == nullptr) return status;
    if (player == nullptr || owned->player || player->pose.reserved != 0 ||
        !valid_ref(*owned, player->id) || !valid_ref(*owned, player->region_id) ||
        !valid_ref(*owned, player->asset_id) ||
        !valid_ref(*owned, player->initial_safe_point_id))
        return TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST;
    owned->player = *player;
    return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
}

#define TGD_APPEND_DEFINITION(name, type, member, limit, expression) \
    int32_t tgd_sandbox_compile_request_append_##name( \
        tgd_sandbox_service_handle service, \
        tgd_sandbox_request_handle request, \
        const type* record \
    ) { \
        return append_typed(service, request, record, &OwnedRequest::member, limit, \
            [](const OwnedRequest& owned, const type& value) noexcept { return (expression); }); \
    }

TGD_APPEND_DEFINITION(region, tgd_sandbox_service_region, regions,
    sandbox_region_capacity + 1U, valid_ref(owned, value.id))
TGD_APPEND_DEFINITION(asset, tgd_sandbox_service_asset, assets,
    sandbox_asset_capacity + 1U, valid_ref(owned, value.id) && all_zero(value.reserved))
TGD_APPEND_DEFINITION(actor, tgd_sandbox_service_placement, actors,
    sandbox_actor_capacity + 1U,
    valid_ref(owned, value.id) && valid_ref(owned, value.region_id) &&
    valid_ref(owned, value.asset_id) && value.pose.reserved == 0)
TGD_APPEND_DEFINITION(ground_blocker, tgd_sandbox_service_ground_blocker, blockers,
    sandbox_ground_blocker_capacity + 1U,
    valid_ref(owned, value.id) && valid_ref(owned, value.region_id) &&
    valid_ref(owned, value.asset_id) && value.reserved == 0)
TGD_APPEND_DEFINITION(safe_point, tgd_sandbox_service_placement, safe_points,
    sandbox_safe_point_capacity + 1U,
    valid_ref(owned, value.id) && valid_ref(owned, value.region_id) &&
    valid_ref(owned, value.asset_id) && value.pose.reserved == 0)
TGD_APPEND_DEFINITION(interaction, tgd_sandbox_service_placement, interactions,
    sandbox_interaction_capacity + 1U,
    valid_ref(owned, value.id) && valid_ref(owned, value.region_id) &&
    valid_ref(owned, value.asset_id) && value.pose.reserved == 0)
TGD_APPEND_DEFINITION(mechanism, tgd_sandbox_service_placement, mechanisms,
    sandbox_mechanism_capacity + 1U,
    valid_ref(owned, value.id) && valid_ref(owned, value.region_id) &&
    valid_ref(owned, value.asset_id) && value.pose.reserved == 0)
TGD_APPEND_DEFINITION(wave, tgd_sandbox_service_wave, waves,
    sandbox_wave_capacity + 1U,
    valid_ref(owned, value.id) && valid_ref(owned, value.region_id) &&
    valid_ref(owned, value.predecessor_wave_id) && valid_ref(owned, value.trigger_target_id) &&
    all_zero(value.reserved))
TGD_APPEND_DEFINITION(wave_spawn, tgd_sandbox_service_wave_spawn, wave_spawns,
    sandbox_wave_spawn_capacity + 1U,
    valid_ref(owned, value.wave_id) && valid_ref(owned, value.actor_id) && value.reserved == 0)
TGD_APPEND_DEFINITION(objective, tgd_sandbox_service_objective, objectives,
    sandbox_objective_capacity + 1U,
    valid_ref(owned, value.id) && valid_ref(owned, value.region_id) &&
    valid_ref(owned, value.predecessor_objective_id) &&
    valid_ref(owned, value.completion_target_id) && all_zero(value.reserved))
TGD_APPEND_DEFINITION(interaction_binding, tgd_sandbox_service_interaction_binding,
    interaction_bindings, sandbox_interaction_capacity + 1U,
    valid_ref(owned, value.interaction_id) &&
    valid_ref(owned, value.target_mechanism_id) && all_zero(value.reserved))
TGD_APPEND_DEFINITION(mechanism_binding, tgd_sandbox_service_mechanism_binding,
    mechanism_bindings, sandbox_mechanism_capacity + 1U,
    valid_ref(owned, value.mechanism_id) &&
    valid_ref(owned, value.target_ground_blocker_id) && all_zero(value.reserved))

#undef TGD_APPEND_DEFINITION

int32_t tgd_sandbox_compile_request_submit(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    uint8_t* output,
    uint32_t output_capacity,
    uint32_t* output_bytes
) {
    auto* request_slot = find_request_slot(request);
    if (request_slot == nullptr) return TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_REQUEST;
    const bool foreign = request_slot->request->owner != service;
    auto owned = std::move(request_slot->request);
    request_slot->request.reset();
    auto* service_value = find_service(service);
    if (service_value == nullptr) return TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_SERVICE;
    if (foreign) return TGD_SANDBOX_SERVICE_TRANSPORT_FOREIGN_REQUEST;
    if (output_bytes == nullptr || (output == nullptr && output_capacity != 0))
        return TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST;
    *output_bytes = 0;
    if (!metadata_valid(*owned)) return TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST;
    try {
        RuntimeProjection projection{*owned};
        auto compiled = compile_sandbox_package(projection.view);
        const auto compile_status = compiled.status();
        auto validation = compiled.validation();
        if (!compiled.succeeded()) {
            auto result = serialize_result(
                TGD_SANDBOX_SERVICE_COMPILER_REJECTED, compile_status, validation,
                service_value->provider.identity(), *owned
            );
            *output_bytes = static_cast<std::uint32_t>(result.size());
            if (output_capacity < result.size()) return TGD_SANDBOX_SERVICE_TRANSPORT_OUTPUT_TOO_SMALL;
            std::memcpy(output, result.data(), result.size());
            return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
        }
        auto candidate = std::move(compiled).take_candidate();
        auto prepared = service_value->provider.prepare(to_identity(owned->expected), std::move(candidate));
        std::uint8_t outcome = TGD_SANDBOX_SERVICE_PREPARE_REJECTED;
        switch (prepared.status()) {
            case SandboxPackagePrepareStatus::prepared: outcome = TGD_SANDBOX_SERVICE_PUBLISHED; break;
            case SandboxPackagePrepareStatus::stale_generation: outcome = TGD_SANDBOX_SERVICE_STALE_GENERATION; break;
            case SandboxPackagePrepareStatus::stale_checksum: outcome = TGD_SANDBOX_SERVICE_STALE_CHECKSUM; break;
            case SandboxPackagePrepareStatus::generation_exhausted: outcome = TGD_SANDBOX_SERVICE_GENERATION_EXHAUSTED; break;
            case SandboxPackagePrepareStatus::missing_candidate:
            case SandboxPackagePrepareStatus::invalid: break;
        }
        const auto* update = prepared.prepared_update();
        const auto planned_identity = update == nullptr ? service_value->provider.identity()
                                                        : update->next_identity();
        auto result = serialize_result(outcome, compile_status, validation, planned_identity, *owned);
        *output_bytes = static_cast<std::uint32_t>(result.size());
        if (output_capacity < result.size()) return TGD_SANDBOX_SERVICE_TRANSPORT_OUTPUT_TOO_SMALL;
        if (update != nullptr) {
            auto token = std::move(prepared).take_prepared_update();
            const auto committed = service_value->provider.commit(std::move(*token));
            if (committed.status() != SandboxPackageCommitStatus::committed) {
                result = serialize_result(TGD_SANDBOX_SERVICE_COMMIT_REJECTED,
                    compile_status, validation, service_value->provider.identity(), *owned);
            }
        }
        std::memcpy(output, result.data(), result.size());
        *output_bytes = static_cast<std::uint32_t>(result.size());
        return TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED;
    } catch (const std::bad_alloc&) {
        return TGD_SANDBOX_SERVICE_TRANSPORT_ALLOCATION_FAILED;
    } catch (const std::length_error&) {
        return TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED;
    } catch (...) {
        return TGD_SANDBOX_SERVICE_TRANSPORT_INTERNAL_FAILURE;
    }
}

}  // extern "C"
