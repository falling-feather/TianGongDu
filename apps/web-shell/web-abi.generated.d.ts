// Generated from content/design/web-abi.json. Do not edit by hand.
export type TgdWebMessageName =
  | "boot_config"
  | "boot_result"
  | "platform_event_batch"
  | "ui_command"
  | "ui_event"
  | "storage_request"
  | "storage_completion"
  | "storage_cancel"
  | "diagnostic";

export type TgdWebStorageOperationName =
  | "read"
  | "write_atomic"
  | "list"
  | "delete"
  | "estimate_quota"
  | "request_persistence"
  | "export_profile"
  | "import_profile"
  | "migrate_profile";

export type TgdWebErrorName =
  | "none"
  | "incompatible_abi"
  | "invalid_message"
  | "buffer_too_small"
  | "unknown_message_type"
  | "payload_too_large"
  | "backpressure"
  | "duplicate_completion"
  | "cancelled"
  | "stale_generation"
  | "timeout"
  | "storage_not_found"
  | "storage_conflict"
  | "storage_quota"
  | "storage_corrupt"
  | "storage_unavailable"
  | "internal";

export interface TgdWebAbiContract {
  readonly major: 1;
  readonly minor: 0;
  readonly headerBytes: 40;
  readonly maxMessageBytes: 262144;
  readonly messageType: {
    readonly boot_config: 1;
    readonly boot_result: 2;
    readonly platform_event_batch: 3;
    readonly ui_command: 4;
    readonly ui_event: 5;
    readonly storage_request: 100;
    readonly storage_completion: 101;
    readonly storage_cancel: 102;
    readonly diagnostic: 200;
  };
  readonly storageOperation: {
    readonly read: 1;
    readonly write_atomic: 2;
    readonly list: 3;
    readonly delete: 4;
    readonly estimate_quota: 5;
    readonly request_persistence: 6;
    readonly export_profile: 7;
    readonly import_profile: 8;
    readonly migrate_profile: 9;
  };
  readonly errorCode: {
    readonly none: 0;
    readonly incompatible_abi: 1;
    readonly invalid_message: 2;
    readonly buffer_too_small: 3;
    readonly unknown_message_type: 4;
    readonly payload_too_large: 5;
    readonly backpressure: 6;
    readonly duplicate_completion: 7;
    readonly cancelled: 8;
    readonly stale_generation: 9;
    readonly timeout: 10;
    readonly storage_not_found: 11;
    readonly storage_conflict: 12;
    readonly storage_quota: 13;
    readonly storage_corrupt: 14;
    readonly storage_unavailable: 15;
    readonly internal: 16;
  };
}

declare global {
  var TgdWebAbi: TgdWebAbiContract;
}

export {};
