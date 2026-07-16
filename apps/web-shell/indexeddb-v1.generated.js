// Generated from content/design/indexeddb-v1.json. Do not edit by hand.
(() => {
  "use strict";
  const deepFreeze = (value) => {
    if (value && typeof value === "object" && !Object.isFrozen(value)) {
      Object.freeze(value);
      for (const child of Object.values(value)) deepFreeze(child);
    }
    return value;
  };
  globalThis.TgdIndexedDbContract = deepFreeze(
  {
    "$schema": "../schemas/indexeddb-v1.schema.json",
    "schemaVersion": "1.0.0",
    "database": {
      "name": "tiangongdu.prototype_f1.internal.profile",
      "version": 1,
      "channel": "prototype_f1"
    },
    "stores": [
      {
        "recordKind": 1,
        "name": "profile_heads",
        "keyPath": [
          "channel",
          "profileId"
        ],
        "indexes": []
      },
      {
        "recordKind": 2,
        "name": "snapshots",
        "keyPath": [
          "profileId",
          "snapshotId"
        ],
        "indexes": [
          {
            "name": "by_profile_sequence",
            "keyPath": [
              "profileId",
              "logicalSequence"
            ],
            "unique": false
          }
        ]
      },
      {
        "recordKind": 3,
        "name": "operations",
        "keyPath": [
          "profileId",
          "operationId"
        ],
        "indexes": [
          {
            "name": "by_profile_device_sequence",
            "keyPath": [
              "profileId",
              "deviceId",
              "logicalSequence"
            ],
            "unique": false
          },
          {
            "name": "by_profile_status",
            "keyPath": [
              "profileId",
              "status"
            ],
            "unique": false
          }
        ]
      },
      {
        "recordKind": 4,
        "name": "profile_meta",
        "keyPath": "profileId",
        "indexes": []
      },
      {
        "recordKind": 5,
        "name": "device_settings",
        "keyPath": [
          "channel",
          "settingGroup"
        ],
        "indexes": []
      },
      {
        "recordKind": 6,
        "name": "migration_workspace",
        "keyPath": [
          "profileId",
          "migrationId"
        ],
        "indexes": []
      }
    ]
  }
  );
})();
