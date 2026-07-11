/* Generated from content/design/web-abi.json. Do not edit by hand. */
#ifndef TGD_CONTRACTS_TGD_WEB_ABI_H
#define TGD_CONTRACTS_TGD_WEB_ABI_H

#include <stdint.h>

#define TGD_WEB_ABI_MAJOR UINT16_C(1)
#define TGD_WEB_ABI_MINOR UINT16_C(0)
#define TGD_WEB_ABI_MESSAGE_HEADER_BYTES UINT32_C(40)
#define TGD_WEB_ABI_MAX_MESSAGE_BYTES UINT32_C(262144)
#define TGD_WEB_BOOT_CONFIG_V1_BYTES UINT32_C(52)
#define TGD_WEB_UI_COMMAND_V1_BYTES UINT32_C(20)
#define TGD_WEB_UI_EVENT_V1_BYTES UINT32_C(40)
#define TGD_WEB_STORAGE_REQUEST_V1_HEADER_BYTES UINT32_C(208)
#define TGD_WEB_STORAGE_COMPLETION_V1_HEADER_BYTES UINT32_C(152)
#define TGD_WEB_MAX_STORAGE_TRANSFER_BYTES UINT32_C(16777392)

typedef enum tgd_web_message_type {
    TGD_WEB_MESSAGE_BOOT_CONFIG = 1,
    TGD_WEB_MESSAGE_BOOT_RESULT = 2,
    TGD_WEB_MESSAGE_PLATFORM_EVENT_BATCH = 3,
    TGD_WEB_MESSAGE_UI_COMMAND = 4,
    TGD_WEB_MESSAGE_UI_EVENT = 5,
    TGD_WEB_MESSAGE_STORAGE_REQUEST = 100,
    TGD_WEB_MESSAGE_STORAGE_COMPLETION = 101,
    TGD_WEB_MESSAGE_STORAGE_CANCEL = 102,
    TGD_WEB_MESSAGE_DIAGNOSTIC = 200
} tgd_web_message_type;

typedef enum tgd_web_storage_operation {
    TGD_WEB_STORAGE_READ = 1,
    TGD_WEB_STORAGE_WRITE_ATOMIC = 2,
    TGD_WEB_STORAGE_LIST = 3,
    TGD_WEB_STORAGE_DELETE = 4,
    TGD_WEB_STORAGE_ESTIMATE_QUOTA = 5,
    TGD_WEB_STORAGE_REQUEST_PERSISTENCE = 6,
    TGD_WEB_STORAGE_EXPORT_PROFILE = 7,
    TGD_WEB_STORAGE_IMPORT_PROFILE = 8,
    TGD_WEB_STORAGE_MIGRATE_PROFILE = 9
} tgd_web_storage_operation;

typedef enum tgd_web_error_code {
    TGD_WEB_ERROR_NONE = 0,
    TGD_WEB_ERROR_INCOMPATIBLE_ABI = 1,
    TGD_WEB_ERROR_INVALID_MESSAGE = 2,
    TGD_WEB_ERROR_BUFFER_TOO_SMALL = 3,
    TGD_WEB_ERROR_UNKNOWN_MESSAGE_TYPE = 4,
    TGD_WEB_ERROR_PAYLOAD_TOO_LARGE = 5,
    TGD_WEB_ERROR_BACKPRESSURE = 6,
    TGD_WEB_ERROR_DUPLICATE_COMPLETION = 7,
    TGD_WEB_ERROR_CANCELLED = 8,
    TGD_WEB_ERROR_STALE_GENERATION = 9,
    TGD_WEB_ERROR_TIMEOUT = 10,
    TGD_WEB_ERROR_STORAGE_NOT_FOUND = 11,
    TGD_WEB_ERROR_STORAGE_CONFLICT = 12,
    TGD_WEB_ERROR_STORAGE_QUOTA = 13,
    TGD_WEB_ERROR_STORAGE_CORRUPT = 14,
    TGD_WEB_ERROR_STORAGE_UNAVAILABLE = 15,
    TGD_WEB_ERROR_INTERNAL = 16
} tgd_web_error_code;

typedef struct tgd_web_abi_message_header {
    uint16_t abi_major;
    uint16_t abi_minor;
    uint16_t message_type;
    uint16_t payload_version;
    uint32_t payload_length;
    uint32_t session_generation;
    uint64_t sequence;
    uint64_t request_id_high;
    uint64_t request_id_low;
} tgd_web_abi_message_header;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t tgd_web_abi_version(void);
int32_t tgd_web_boot(const uint8_t* bytes, uint32_t length);
int32_t tgd_web_submit_platform_event(const uint8_t* bytes, uint32_t length);
int32_t tgd_web_submit_ui_command(const uint8_t* bytes, uint32_t length);
uint32_t tgd_web_peek_platform_request_size(void);
int32_t tgd_web_poll_platform_request(uint8_t* output, uint32_t capacity);
int32_t tgd_web_complete_async_request(const uint8_t* bytes, uint32_t length);
uint32_t tgd_web_peek_ui_event_size(void);
int32_t tgd_web_poll_ui_event(uint8_t* output, uint32_t capacity);
void tgd_web_request_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif
