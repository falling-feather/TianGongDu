import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const root = resolve(import.meta.dirname, "..");
const contractPath = resolve(root, "content/design/web-abi.json");
const outputPaths = Object.freeze({
  cHeader: resolve(root, "src/contracts/include/tgd/contracts/tgd_web_abi.h"),
  javascript: resolve(root, "apps/web-shell/web-abi.generated.js"),
  typescript: resolve(root, "apps/web-shell/web-abi.generated.d.ts")
});

function screamingSnake(value) {
  return value.toUpperCase();
}

function pascalCase(value) {
  return value.split("_").map((part) => `${part[0].toUpperCase()}${part.slice(1)}`).join("");
}

function assertIdList(entries, label) {
  const ids = new Set();
  const names = new Set();
  let previous = -1;
  for (const entry of entries) {
    if (!Number.isInteger(entry.id) || entry.id < 0 || entry.id > 0xffff) {
      throw new Error(`${label} id is out of uint16 range: ${entry.id}`);
    }
    if (!/^[a-z][a-z0-9_]*$/.test(entry.name)) {
      throw new Error(`${label} name is not stable snake_case: ${entry.name}`);
    }
    if (ids.has(entry.id) || names.has(entry.name)) {
      throw new Error(`${label} contains a duplicate id or name: ${entry.name}`);
    }
    if (entry.id <= previous) {
      throw new Error(`${label} ids must be strictly increasing: ${entry.name}`);
    }
    ids.add(entry.id);
    names.add(entry.name);
    previous = entry.id;
  }
}

export function validateWebAbiContract(contract) {
  if (contract.schemaVersion !== "1.0.0") throw new Error("Unsupported Web ABI schema.");
  if (contract.abi.major !== 1 || contract.abi.minor !== 0) {
    throw new Error("Web ABI generator only supports 1.0.");
  }
  if (contract.abi.endianness !== "little" || contract.abi.headerBytes !== 40) {
    throw new Error("Web ABI v1 requires a 40-byte explicit little-endian header.");
  }
  if (contract.abi.maxMessageBytes !== 256 * 1024) {
    throw new Error("Web ABI v1 max message size must stay at 256 KiB.");
  }
  const expectedPayloads = Object.freeze({
    bootConfigV1Bytes: 52,
    uiCommandV1Bytes: 20,
    uiEventV1Bytes: 40,
    storageRequestV1HeaderBytes: 208,
    storageCompletionV1HeaderBytes: 152,
    maxStorageTransferBytes: 16 * 1024 * 1024 + 176
  });
  for (const [name, value] of Object.entries(expectedPayloads)) {
    if (contract.payloads?.[name] !== value) {
      throw new Error(`Web ABI v1 payload size changed: ${name}`);
    }
  }
  assertIdList(contract.messageTypes, "messageTypes");
  assertIdList(contract.uiCommands, "uiCommands");
  assertIdList(contract.storageOperations, "storageOperations");
  assertIdList(contract.errorCodes, "errorCodes");
  for (const message of contract.messageTypes) {
    if (!["js_to_cpp", "cpp_to_js"].includes(message.direction) ||
        !Number.isInteger(message.payloadVersion) || typeof message.critical !== "boolean") {
      throw new Error(`Message metadata is incomplete: ${message.name}`);
    }
  }
  if (contract.errorCodes[0]?.id !== 0 || contract.errorCodes[0]?.name !== "none") {
    throw new Error("Web ABI error id 0 must remain none.");
  }
  return contract;
}

function renderEnum(entries, prefix) {
  return entries.map(
    (entry) => `    ${prefix}_${screamingSnake(entry.name)} = ${entry.id}`
  ).join(",\n");
}

export function renderCHeader(contract) {
  const messageEnum = renderEnum(contract.messageTypes, "TGD_WEB_MESSAGE");
  const uiCommandEnum = renderEnum(contract.uiCommands, "TGD_WEB_UI_COMMAND");
  const storageEnum = renderEnum(
    contract.storageOperations,
    "TGD_WEB_STORAGE"
  );
  const errorEnum = renderEnum(contract.errorCodes, "TGD_WEB_ERROR");
  return `/* Generated from content/design/web-abi.json. Do not edit by hand. */
#ifndef TGD_CONTRACTS_TGD_WEB_ABI_H
#define TGD_CONTRACTS_TGD_WEB_ABI_H

#include <stdint.h>

#define TGD_WEB_ABI_MAJOR UINT16_C(${contract.abi.major})
#define TGD_WEB_ABI_MINOR UINT16_C(${contract.abi.minor})
#define TGD_WEB_ABI_MESSAGE_HEADER_BYTES UINT32_C(${contract.abi.headerBytes})
#define TGD_WEB_ABI_MAX_MESSAGE_BYTES UINT32_C(${contract.abi.maxMessageBytes})
#define TGD_WEB_BOOT_CONFIG_V1_BYTES UINT32_C(${contract.payloads.bootConfigV1Bytes})
#define TGD_WEB_UI_COMMAND_V1_BYTES UINT32_C(${contract.payloads.uiCommandV1Bytes})
#define TGD_WEB_UI_EVENT_V1_BYTES UINT32_C(${contract.payloads.uiEventV1Bytes})
#define TGD_WEB_STORAGE_REQUEST_V1_HEADER_BYTES UINT32_C(${contract.payloads.storageRequestV1HeaderBytes})
#define TGD_WEB_STORAGE_COMPLETION_V1_HEADER_BYTES UINT32_C(${contract.payloads.storageCompletionV1HeaderBytes})
#define TGD_WEB_MAX_STORAGE_TRANSFER_BYTES UINT32_C(${contract.payloads.maxStorageTransferBytes})

typedef enum tgd_web_message_type {
${messageEnum}
} tgd_web_message_type;

typedef enum tgd_web_ui_command {
${uiCommandEnum}
} tgd_web_ui_command;

typedef enum tgd_web_storage_operation {
${storageEnum}
} tgd_web_storage_operation;

typedef enum tgd_web_error_code {
${errorEnum}
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
`;
}

function objectEntries(entries) {
  return entries.map((entry) => `    ${entry.name}: ${entry.id}`).join(",\n");
}

export function renderJavaScript(contract) {
  return `// Generated from content/design/web-abi.json. Do not edit by hand.
(() => {
  "use strict";
  const contract = Object.freeze({
    major: ${contract.abi.major},
    minor: ${contract.abi.minor},
    headerBytes: ${contract.abi.headerBytes},
    maxMessageBytes: ${contract.abi.maxMessageBytes},
    payload: Object.freeze({
      bootConfigV1Bytes: ${contract.payloads.bootConfigV1Bytes},
      uiCommandV1Bytes: ${contract.payloads.uiCommandV1Bytes},
      uiEventV1Bytes: ${contract.payloads.uiEventV1Bytes},
      storageRequestV1HeaderBytes: ${contract.payloads.storageRequestV1HeaderBytes},
      storageCompletionV1HeaderBytes: ${contract.payloads.storageCompletionV1HeaderBytes},
      maxStorageTransferBytes: ${contract.payloads.maxStorageTransferBytes}
    }),
    messageType: Object.freeze({
${objectEntries(contract.messageTypes)}
    }),
    uiCommand: Object.freeze({
${objectEntries(contract.uiCommands)}
    }),
    storageOperation: Object.freeze({
${objectEntries(contract.storageOperations)}
    }),
    errorCode: Object.freeze({
${objectEntries(contract.errorCodes)}
    })
  });
  globalThis.TgdWebAbi = contract;
})();
`;
}

function typeUnion(entries) {
  return entries.map((entry) => `  | "${entry.name}"`).join("\n");
}

export function renderTypeScript(contract) {
  const messageKeys = contract.messageTypes.map((entry) => `    readonly ${entry.name}: ${entry.id};`).join("\n");
  const uiCommandKeys = contract.uiCommands.map((entry) => `    readonly ${entry.name}: ${entry.id};`).join("\n");
  const storageKeys = contract.storageOperations.map((entry) => `    readonly ${entry.name}: ${entry.id};`).join("\n");
  const errorKeys = contract.errorCodes.map((entry) => `    readonly ${entry.name}: ${entry.id};`).join("\n");
  return `// Generated from content/design/web-abi.json. Do not edit by hand.
export type TgdWebMessageName =
${typeUnion(contract.messageTypes)};

export type TgdWebUiCommandName =
${typeUnion(contract.uiCommands)};

export type TgdWebStorageOperationName =
${typeUnion(contract.storageOperations)};

export type TgdWebErrorName =
${typeUnion(contract.errorCodes)};

export interface TgdWebAbiContract {
  readonly major: ${contract.abi.major};
  readonly minor: ${contract.abi.minor};
  readonly headerBytes: ${contract.abi.headerBytes};
  readonly maxMessageBytes: ${contract.abi.maxMessageBytes};
  readonly payload: {
    readonly bootConfigV1Bytes: ${contract.payloads.bootConfigV1Bytes};
    readonly uiCommandV1Bytes: ${contract.payloads.uiCommandV1Bytes};
    readonly uiEventV1Bytes: ${contract.payloads.uiEventV1Bytes};
    readonly storageRequestV1HeaderBytes: ${contract.payloads.storageRequestV1HeaderBytes};
    readonly storageCompletionV1HeaderBytes: ${contract.payloads.storageCompletionV1HeaderBytes};
    readonly maxStorageTransferBytes: ${contract.payloads.maxStorageTransferBytes};
  };
  readonly messageType: {
${messageKeys}
  };
  readonly uiCommand: {
${uiCommandKeys}
  };
  readonly storageOperation: {
${storageKeys}
  };
  readonly errorCode: {
${errorKeys}
  };
}

declare global {
  var TgdWebAbi: TgdWebAbiContract;
}

export {};
`;
}

export async function loadWebAbiContract() {
  return validateWebAbiContract(JSON.parse(await readFile(contractPath, "utf8")));
}

export function renderWebAbiOutputs(contract) {
  return Object.freeze({
    cHeader: renderCHeader(contract),
    javascript: renderJavaScript(contract),
    typescript: renderTypeScript(contract)
  });
}

async function main() {
  const outputs = renderWebAbiOutputs(await loadWebAbiContract());
  const check = process.argv.includes("--check");
  for (const [name, outputPath] of Object.entries(outputPaths)) {
    if (check) {
      const actual = await readFile(outputPath, "utf8").catch(() => "");
      if (actual !== outputs[name]) {
        throw new Error(`Generated Web ABI ${name} is stale. Run: npm run generate:web-abi`);
      }
      continue;
    }
    await mkdir(dirname(outputPath), { recursive: true });
    await writeFile(outputPath, outputs[name], "utf8");
    console.log(`Generated ${outputPath.slice(root.length + 1).replaceAll("\\", "/")}`);
  }
  if (check) console.log("Generated Web ABI contracts are current.");
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) {
  await main();
}
