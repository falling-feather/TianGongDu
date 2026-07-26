#ifndef TGD_CONTENT_SANDBOX_PACKAGE_SERVICE_ABI_H
#define TGD_CONTENT_SANDBOX_PACKAGE_SERVICE_ABI_H

#include <stdint.h>

#define TGD_SANDBOX_COMPILER_SERVICE_ABI_MAJOR UINT16_C(1)
#define TGD_SANDBOX_COMPILER_SERVICE_ABI_MINOR UINT16_C(2)
#define TGD_SANDBOX_COMPILER_SERVICE_MAX_SERVICES UINT32_C(8)
#define TGD_SANDBOX_COMPILER_SERVICE_MAX_REQUESTS UINT32_C(32)
#define TGD_SANDBOX_COMPILER_SERVICE_MAX_STRING_REFS UINT32_C(4096)
#define TGD_SANDBOX_COMPILER_SERVICE_MAX_COPIED_UTF8_BYTES UINT32_C(262144)
#define TGD_SANDBOX_COMPILER_SERVICE_MAX_CANONICAL_PACKAGE_BYTES UINT32_C(4194304)
#define TGD_SANDBOX_COMPILER_SERVICE_RESULT_HEADER_BYTES UINT32_C(120)
#define TGD_SANDBOX_COMPILER_SERVICE_RESULT_ARTIFACT_BYTES UINT32_C(16)
#define TGD_SANDBOX_COMPILER_SERVICE_RESULT_PREFIX_BYTES UINT32_C(136)
#define TGD_SANDBOX_COMPILER_SERVICE_MAX_RESULT_BYTES UINT32_C(4194440)
#define TGD_SANDBOX_COMPILER_SERVICE_DIAGNOSTIC_BYTES UINT32_C(48)

typedef uint64_t tgd_sandbox_service_handle;
typedef uint64_t tgd_sandbox_request_handle;
typedef uint32_t tgd_sandbox_request_string_ref;

typedef enum tgd_sandbox_service_transport_status {
    TGD_SANDBOX_SERVICE_TRANSPORT_SUCCEEDED = 1,
    TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_SERVICE = 2,
    TGD_SANDBOX_SERVICE_TRANSPORT_INVALID_REQUEST = 3,
    TGD_SANDBOX_SERVICE_TRANSPORT_FOREIGN_REQUEST = 4,
    TGD_SANDBOX_SERVICE_TRANSPORT_MALFORMED_REQUEST = 5,
    TGD_SANDBOX_SERVICE_TRANSPORT_CAPACITY_EXCEEDED = 6,
    TGD_SANDBOX_SERVICE_TRANSPORT_ALLOCATION_FAILED = 7,
    TGD_SANDBOX_SERVICE_TRANSPORT_OUTPUT_TOO_SMALL = 8,
    TGD_SANDBOX_SERVICE_TRANSPORT_INTERNAL_FAILURE = 9,
    TGD_SANDBOX_SERVICE_TRANSPORT_INVALID = 255
} tgd_sandbox_service_transport_status;

typedef enum tgd_sandbox_service_publish_outcome {
    TGD_SANDBOX_SERVICE_PUBLISHED = 1,
    TGD_SANDBOX_SERVICE_COMPILER_REJECTED = 2,
    TGD_SANDBOX_SERVICE_STALE_GENERATION = 3,
    TGD_SANDBOX_SERVICE_STALE_CHECKSUM = 4,
    TGD_SANDBOX_SERVICE_GENERATION_EXHAUSTED = 5,
    TGD_SANDBOX_SERVICE_PREPARE_REJECTED = 6,
    TGD_SANDBOX_SERVICE_COMMIT_REJECTED = 7,
    TGD_SANDBOX_SERVICE_PUBLISH_INVALID = 255
} tgd_sandbox_service_publish_outcome;

typedef struct tgd_sandbox_service_identity {
    uint32_t generation;
    uint8_t checksum[32];
} tgd_sandbox_service_identity;

typedef struct tgd_sandbox_service_bounds {
    int32_t min_x;
    int32_t max_x;
    int32_t min_y;
    int32_t max_y;
    int32_t min_height;
    int32_t max_height;
    int16_t min_floor_layer;
    int16_t max_floor_layer;
} tgd_sandbox_service_bounds;

typedef struct tgd_sandbox_service_pose {
    int32_t x;
    int32_t y;
    int32_t height;
    int16_t floor_layer;
    uint16_t reserved;
} tgd_sandbox_service_pose;

typedef struct tgd_sandbox_service_metadata {
    tgd_sandbox_request_string_ref package_id;
    tgd_sandbox_request_string_ref sandbox_id;
    tgd_sandbox_request_string_ref completion_objective_id;
    tgd_sandbox_service_bounds bounds;
} tgd_sandbox_service_metadata;

typedef struct tgd_sandbox_service_player {
    tgd_sandbox_request_string_ref id;
    tgd_sandbox_request_string_ref region_id;
    tgd_sandbox_request_string_ref asset_id;
    tgd_sandbox_request_string_ref initial_safe_point_id;
    tgd_sandbox_service_pose pose;
    uint32_t facing_millidegrees;
} tgd_sandbox_service_player;

typedef struct tgd_sandbox_service_region {
    tgd_sandbox_request_string_ref id;
    tgd_sandbox_service_bounds bounds;
} tgd_sandbox_service_region;

typedef struct tgd_sandbox_service_asset {
    tgd_sandbox_request_string_ref id;
    uint8_t asset_kind;
    uint8_t reserved[3];
} tgd_sandbox_service_asset;

typedef struct tgd_sandbox_service_placement {
    tgd_sandbox_request_string_ref id;
    tgd_sandbox_request_string_ref region_id;
    tgd_sandbox_request_string_ref asset_id;
    tgd_sandbox_service_pose pose;
    uint32_t facing_millidegrees;
} tgd_sandbox_service_placement;

typedef struct tgd_sandbox_service_ground_blocker {
    tgd_sandbox_request_string_ref id;
    tgd_sandbox_request_string_ref region_id;
    tgd_sandbox_request_string_ref asset_id;
    int32_t min_x;
    int32_t max_x;
    int32_t min_y;
    int32_t max_y;
    int32_t min_height;
    int32_t max_height;
    int16_t floor_layer;
    uint16_t reserved;
} tgd_sandbox_service_ground_blocker;

typedef struct tgd_sandbox_service_wave {
    tgd_sandbox_request_string_ref id;
    tgd_sandbox_request_string_ref region_id;
    tgd_sandbox_request_string_ref predecessor_wave_id;
    tgd_sandbox_request_string_ref trigger_target_id;
    uint8_t trigger_kind;
    uint8_t reserved[3];
} tgd_sandbox_service_wave;

typedef struct tgd_sandbox_service_wave_spawn {
    tgd_sandbox_request_string_ref wave_id;
    tgd_sandbox_request_string_ref actor_id;
    uint32_t delay_ticks;
    uint16_t spawn_order;
    uint16_t reserved;
} tgd_sandbox_service_wave_spawn;

typedef struct tgd_sandbox_service_objective {
    tgd_sandbox_request_string_ref id;
    tgd_sandbox_request_string_ref region_id;
    tgd_sandbox_request_string_ref predecessor_objective_id;
    tgd_sandbox_request_string_ref completion_target_id;
    uint8_t completion_kind;
    uint8_t reserved[3];
} tgd_sandbox_service_objective;

typedef struct tgd_sandbox_service_interaction_binding {
    tgd_sandbox_request_string_ref interaction_id;
    tgd_sandbox_request_string_ref target_mechanism_id;
    int32_t range_mm;
    uint8_t operation;
    uint8_t reserved[3];
} tgd_sandbox_service_interaction_binding;

typedef struct tgd_sandbox_service_mechanism_binding {
    tgd_sandbox_request_string_ref mechanism_id;
    tgd_sandbox_request_string_ref target_ground_blocker_id;
    uint8_t activation;
    uint8_t reserved[3];
} tgd_sandbox_service_mechanism_binding;

typedef struct tgd_sandbox_service_actor_binding {
    tgd_sandbox_request_string_ref actor_id;
    tgd_sandbox_request_string_ref profile_id;
    int32_t max_health;
    uint8_t faction;
    uint8_t duty;
    uint8_t reserved[2];
} tgd_sandbox_service_actor_binding;

typedef struct tgd_sandbox_service_result_header {
    uint8_t complete;
    uint8_t outcome;
    uint8_t compile_status;
    uint8_t package_error;
    uint32_t generation;
    uint32_t diagnostic_count;
    uint8_t checksum[32];
    uint8_t binding_code;
    uint8_t binding_domain;
    uint16_t binding_field;
    uint32_t binding_record_index;
    uint32_t binding_subject_key_low;
    uint32_t binding_subject_key_high;
    uint32_t binding_related_key_low;
    uint32_t binding_related_key_high;
    uint32_t diagnostics_offset;
    uint32_t id_bytes_offset;
    uint32_t total_bytes;
    uint16_t abi_major;
    uint16_t abi_minor;
    uint16_t binding_flags;
    uint16_t binding_reserved;
    uint32_t binding_subject_id_offset;
    uint32_t binding_subject_id_length;
    uint32_t binding_related_id_offset;
    uint32_t binding_related_id_length;
    uint8_t reserved[16];
} tgd_sandbox_service_result_header;

// ABI 1.1+ appends this descriptor after the unchanged ABI 1.0 result header.
// A published result owns package_bytes_length canonical .tgdsbx bytes at
// package_bytes_offset. Every non-published result stores zero for both fields.
typedef struct tgd_sandbox_service_result_artifact {
    uint32_t package_bytes_offset;
    uint32_t package_bytes_length;
    uint32_t reserved[2];
} tgd_sandbox_service_result_artifact;

#define TGD_SANDBOX_SERVICE_DIAGNOSTIC_HAS_SUBJECT_ID UINT16_C(1)
#define TGD_SANDBOX_SERVICE_DIAGNOSTIC_HAS_RELATED_ID UINT16_C(2)

typedef struct tgd_sandbox_service_diagnostic {
    uint16_t code;
    uint8_t severity;
    uint8_t section;
    uint16_t field;
    uint16_t flags;
    uint32_t record_index;
    uint32_t subject_key_low;
    uint32_t subject_key_high;
    uint32_t related_key_low;
    uint32_t related_key_high;
    uint32_t subject_id_offset;
    uint32_t subject_id_length;
    uint32_t related_id_offset;
    uint32_t related_id_length;
    uint32_t reserved;
} tgd_sandbox_service_diagnostic;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t tgd_sandbox_compiler_service_abi_version(void);
int32_t tgd_sandbox_compiler_service_create(tgd_sandbox_service_handle* output);
int32_t tgd_sandbox_compiler_service_destroy(tgd_sandbox_service_handle service);
int32_t tgd_sandbox_compiler_service_read_identity(
    tgd_sandbox_service_handle service,
    tgd_sandbox_service_identity* output
);
int32_t tgd_sandbox_compile_request_create(
    tgd_sandbox_service_handle service,
    const tgd_sandbox_service_identity* expected,
    tgd_sandbox_request_handle* output
);
int32_t tgd_sandbox_compile_request_cancel(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request
);
int32_t tgd_sandbox_compile_request_copy_utf8(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    const uint8_t* bytes,
    uint32_t length,
    tgd_sandbox_request_string_ref* output
);
int32_t tgd_sandbox_compile_request_set_metadata(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    const tgd_sandbox_service_metadata* metadata
);
int32_t tgd_sandbox_compile_request_set_player(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    const tgd_sandbox_service_player* player
);
#define TGD_SANDBOX_DECLARE_APPEND(name, type) \
    int32_t tgd_sandbox_compile_request_append_##name( \
        tgd_sandbox_service_handle service, \
        tgd_sandbox_request_handle request, \
        const type* record \
    )
TGD_SANDBOX_DECLARE_APPEND(region, tgd_sandbox_service_region);
TGD_SANDBOX_DECLARE_APPEND(asset, tgd_sandbox_service_asset);
TGD_SANDBOX_DECLARE_APPEND(actor, tgd_sandbox_service_placement);
TGD_SANDBOX_DECLARE_APPEND(ground_blocker, tgd_sandbox_service_ground_blocker);
TGD_SANDBOX_DECLARE_APPEND(safe_point, tgd_sandbox_service_placement);
TGD_SANDBOX_DECLARE_APPEND(interaction, tgd_sandbox_service_placement);
TGD_SANDBOX_DECLARE_APPEND(mechanism, tgd_sandbox_service_placement);
TGD_SANDBOX_DECLARE_APPEND(wave, tgd_sandbox_service_wave);
TGD_SANDBOX_DECLARE_APPEND(wave_spawn, tgd_sandbox_service_wave_spawn);
TGD_SANDBOX_DECLARE_APPEND(objective, tgd_sandbox_service_objective);
TGD_SANDBOX_DECLARE_APPEND(interaction_binding, tgd_sandbox_service_interaction_binding);
TGD_SANDBOX_DECLARE_APPEND(mechanism_binding, tgd_sandbox_service_mechanism_binding);
TGD_SANDBOX_DECLARE_APPEND(actor_binding, tgd_sandbox_service_actor_binding);
#undef TGD_SANDBOX_DECLARE_APPEND
int32_t tgd_sandbox_compile_request_submit(
    tgd_sandbox_service_handle service,
    tgd_sandbox_request_handle request,
    uint8_t* output,
    uint32_t output_capacity,
    uint32_t* output_bytes
);

#ifdef __cplusplus
}
#endif

#endif
