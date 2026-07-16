#pragma once

#include <tgd/contracts/session_types.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace tgd::contracts {

// Canonical little-endian .tgdsbx v1 envelope. Header, directory, and record
// widths are wire constants; they are never native struct layouts.
inline constexpr std::array<std::uint8_t, 8> sandbox_pack_magic{
    'T', 'G', 'D', 'S', 'B', 'X', 0, 0,
};
inline constexpr std::uint16_t sandbox_pack_format_major = 1;
inline constexpr std::uint16_t sandbox_pack_format_minor = 0;
inline constexpr std::uint16_t sandbox_content_api_major = 1;
inline constexpr std::uint16_t sandbox_content_api_minor = 0;
inline constexpr std::uint16_t sandbox_authoring_schema_major = 1;
inline constexpr std::uint16_t sandbox_authoring_schema_minor = 0;
inline constexpr std::uint16_t sandbox_authoring_schema_patch = 0;
inline constexpr std::size_t sandbox_pack_header_bytes = 96;
inline constexpr std::size_t sandbox_pack_directory_entry_bytes = 24;
inline constexpr std::size_t sandbox_pack_alignment_bytes = 8;
inline constexpr std::size_t sandbox_pack_hash_offset = 64;
inline constexpr std::size_t sandbox_pack_hash_bytes = 32;
inline constexpr std::size_t sandbox_pack_max_bytes = 4U * 1024U * 1024U;
inline constexpr std::size_t sandbox_pack_max_sections = 32;
inline constexpr std::size_t sandbox_pack_max_strings = 2'048;
inline constexpr std::size_t sandbox_pack_max_string_bytes = 256U * 1024U;
inline constexpr std::size_t sandbox_pack_max_id_bytes = 96;

enum class SandboxPackByteOrder : std::uint8_t {
    little_endian = 1,
};

inline constexpr SandboxPackByteOrder sandbox_pack_byte_order =
    SandboxPackByteOrder::little_endian;

inline constexpr std::size_t sandbox_player_capacity = 1;
inline constexpr std::size_t sandbox_region_capacity = 16;
inline constexpr std::size_t sandbox_asset_capacity = 128;
inline constexpr std::size_t sandbox_actor_capacity = 15;
inline constexpr std::size_t sandbox_ground_blocker_capacity = 64;
inline constexpr std::size_t sandbox_safe_point_capacity = 16;
inline constexpr std::size_t sandbox_interaction_capacity = 64;
inline constexpr std::size_t sandbox_mechanism_capacity = 16;
inline constexpr std::size_t sandbox_wave_capacity = 16;
inline constexpr std::size_t sandbox_wave_spawn_capacity = 15;
inline constexpr std::size_t sandbox_objective_capacity = 64;

inline constexpr std::uint32_t sandbox_pack_section_optional = 1U;

// Append-only: existing values may not be reused or reordered.
enum class SandboxPackSectionType : std::uint16_t {
    strings = 1,
    metadata = 2,
    regions = 3,
    assets = 4,
    player = 5,
    actors = 6,
    ground_blockers = 7,
    safe_points = 8,
    interactions = 9,
    mechanisms = 10,
    waves = 11,
    wave_spawns = 12,
    objectives = 13,
};

inline constexpr std::uint32_t sandbox_pack_metadata_record_bytes = 64;
inline constexpr std::uint32_t sandbox_pack_region_record_bytes = 48;
inline constexpr std::uint32_t sandbox_pack_asset_record_bytes = 16;
inline constexpr std::uint32_t sandbox_pack_player_record_bytes = 72;
inline constexpr std::uint32_t sandbox_pack_actor_record_bytes = 64;
inline constexpr std::uint32_t sandbox_pack_ground_blocker_record_bytes = 64;
inline constexpr std::uint32_t sandbox_pack_safe_point_record_bytes = 64;
inline constexpr std::uint32_t sandbox_pack_interaction_record_bytes = 64;
inline constexpr std::uint32_t sandbox_pack_mechanism_record_bytes = 64;
inline constexpr std::uint32_t sandbox_pack_wave_record_bytes = 64;
inline constexpr std::uint32_t sandbox_pack_wave_spawn_record_bytes = 32;
inline constexpr std::uint32_t sandbox_pack_objective_record_bytes = 64;

enum class SandboxPackageError : std::uint8_t {
    none,
    allocation_failed,
    pack_too_large,
    truncated,
    trailing_bytes,
    invalid_magic,
    unsupported_version,
    invalid_header,
    invalid_reserved,
    hash_mismatch,
    invalid_directory,
    duplicate_section,
    missing_section,
    unknown_required_section,
    invalid_section,
    invalid_string_table,
    invalid_stable_id,
    semantic_validation_failed,
};

// Shared producer/decoder/compiler diagnostics. Values are append-only so tools
// can present the same stable code without copying validation rules.
enum class SandboxDiagnosticCode : std::uint16_t {
    duplicate_id = 1,
    invalid_world_bounds = 2,
    invalid_region_bounds = 3,
    missing_region = 4,
    missing_asset = 5,
    asset_kind_mismatch = 6,
    missing_safe_point = 7,
    invalid_player = 8,
    invalid_actor = 9,
    invalid_blocker = 10,
    invalid_safe_point = 11,
    object_out_of_bounds = 12,
    player_start_blocked = 13,
    safe_point_blocked = 14,
    invalid_interaction = 15,
    invalid_mechanism = 16,
    invalid_wave = 17,
    invalid_wave_spawn = 18,
    invalid_objective = 19,
    capacity_exceeded = 20,
    missing_reference = 21,
    reference_kind_mismatch = 22,
    dependency_cycle = 23,
    unreachable_node = 24,
    retry_inconsistent = 25,
    missing_platform_variant = 26,
    missing_asset_metadata = 27,
    missing_asset_anchor = 28,
    color_only_distinction = 29,
    universal_placeholder_conflict = 30,
    license_blocked = 31,
    web_budget_exceeded = 32,
};

struct SandboxDiagnostic final {
    SandboxDiagnosticCode code{SandboxDiagnosticCode::invalid_world_bounds};
    StableContentKey subject{};
    StableContentKey related{};
    std::uint32_t record_index{};

    [[nodiscard]] friend constexpr bool operator==(
        const SandboxDiagnostic&,
        const SandboxDiagnostic&
    ) noexcept = default;
};

}  // namespace tgd::contracts
