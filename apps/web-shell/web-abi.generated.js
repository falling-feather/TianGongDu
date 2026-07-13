// Generated from content/design/web-abi.json. Do not edit by hand.
(() => {
  "use strict";
  const contract = Object.freeze({
    major: 1,
    minor: 1,
    headerBytes: 40,
    maxMessageBytes: 262144,
    payload: Object.freeze({
      bootConfigV1Bytes: 52,
      uiCommandV1Bytes: 20,
      uiEventV1Bytes: 40,
      storageRequestV1HeaderBytes: 208,
      storageCompletionV1HeaderBytes: 152,
      maxStorageTransferBytes: 16777392,
      persistentOperationV1Bytes: 80,
      maxAtomicOperationsPerWrite: 16
    }),
    messageType: Object.freeze({
    boot_config: 1,
    boot_result: 2,
    platform_event_batch: 3,
    ui_command: 4,
    ui_event: 5,
    storage_request: 100,
    storage_completion: 101,
    storage_cancel: 102,
    diagnostic: 200
    }),
    uiCommand: Object.freeze({
    save_guest_checkpoint: 1,
    retry_pending_save: 2
    }),
    storageOperation: Object.freeze({
    read: 1,
    write_atomic: 2,
    list: 3,
    delete: 4,
    estimate_quota: 5,
    request_persistence: 6,
    export_profile: 7,
    import_profile: 8,
    migrate_profile: 9
    }),
    errorCode: Object.freeze({
    none: 0,
    incompatible_abi: 1,
    invalid_message: 2,
    buffer_too_small: 3,
    unknown_message_type: 4,
    payload_too_large: 5,
    backpressure: 6,
    duplicate_completion: 7,
    cancelled: 8,
    stale_generation: 9,
    timeout: 10,
    storage_not_found: 11,
    storage_conflict: 12,
    storage_quota: 13,
    storage_corrupt: 14,
    storage_unavailable: 15,
    internal: 16
    })
  });
  globalThis.TgdWebAbi = contract;
})();
