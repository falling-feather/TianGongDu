import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { pathToFileURL } from "node:url";

const root = resolve(import.meta.dirname, "..");
const registryPath = resolve(root, "content/design/action-registry.json");
const outputPath = resolve(
  root,
  "src/contracts/include/tgd/contracts/action_registry.generated.hpp"
);

export function fnv1a32(value) {
  let hash = 0x811c9dc5;
  for (const byte of Buffer.from(value, "utf8")) {
    hash ^= byte;
    hash = Math.imul(hash, 0x01000193) >>> 0;
  }
  return hash;
}

function valueType(value) {
  const names = {
    digital: "ActionValueType::digital",
    axis1d: "ActionValueType::axis1d",
    vector2: "ActionValueType::vector2",
    text: "ActionValueType::text"
  };
  if (!names[value]) throw new Error(`Unsupported action value type: ${value}`);
  return names[value];
}

function assertUniqueIds(entries, label) {
  const seen = new Map();
  for (const entry of entries) {
    const hash = fnv1a32(entry.id);
    if (seen.has(hash)) {
      throw new Error(`${label} hash collision: ${seen.get(hash)} and ${entry.id}`);
    }
    seen.set(hash, entry.id);
  }
}

function idLiteral(type, value) {
  const hex = fnv1a32(value).toString(16).padStart(8, "0");
  return `${type}{0x${hex}U}`;
}

export function renderActionContract(registry) {
  assertUniqueIds(registry.contexts, "context");
  assertUniqueIds(registry.actions, "action");
  const contextCapacity = Math.max(...registry.actions.map((action) => action.contexts.length));
  const contextRows = registry.contexts.map(
    (context) =>
      `    {${idLiteral("InputContextId", context.id)}, "${context.id}", ${context.priority}, ${context.capturesText}},`
  );
  const actionRows = registry.actions.map((action) => {
    const contexts = [
      ...action.contexts.map((context) => idLiteral("InputContextId", context)),
      ...Array(contextCapacity - action.contexts.length).fill("InputContextId{}")
    ];
    return [
      `    {${idLiteral("ActionId", action.id)}, "${action.id}", ${valueType(action.valueType)},`,
      `     "${action.edgePolicy}", ${action.clearOnBlur},`,
      `     {${contexts.join(", ")}}, ${action.contexts.length}},`
    ].join("\n");
  });

  return `// Generated from content/design/action-registry.json. Do not edit by hand.
#pragma once

#include <tgd/contracts/input_action.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace tgd::contracts {

struct InputContextDescriptor final {
    InputContextId id{};
    std::string_view name{};
    std::int32_t priority{};
    bool captures_text{};
};

struct ActionDescriptor final {
    ActionId id{};
    std::string_view name{};
    ActionValueType value_type{ActionValueType::digital};
    std::string_view edge_policy{};
    bool clear_on_blur{};
    std::array<InputContextId, ${contextCapacity}> contexts{};
    std::uint8_t context_count{};
};

inline constexpr std::array<InputContextDescriptor, ${registry.contexts.length}> input_context_descriptors{{
${contextRows.join("\n")}
}};

inline constexpr std::array<ActionDescriptor, ${registry.actions.length}> action_descriptors{{
${actionRows.join("\n")}
}};

[[nodiscard]] constexpr const ActionDescriptor* find_action_descriptor(ActionId id) noexcept {
    for (const auto& descriptor : action_descriptors) {
        if (descriptor.id == id) {
            return &descriptor;
        }
    }
    return nullptr;
}

[[nodiscard]] constexpr bool action_supports_context(
    const ActionDescriptor& action,
    InputContextId context
) noexcept {
    for (std::size_t index = 0; index < action.context_count; ++index) {
        if (action.contexts[index] == context) {
            return true;
        }
    }
    return false;
}

}  // namespace tgd::contracts
`;
}

export async function loadActionRegistry() {
  return JSON.parse(await readFile(registryPath, "utf8"));
}

async function main() {
  const registry = await loadActionRegistry();
  const expected = renderActionContract(registry);
  if (process.argv.includes("--check")) {
    const actual = await readFile(outputPath, "utf8").catch(() => "");
    if (actual !== expected) {
      throw new Error(
        "Generated Action contract is stale. Run: npm run generate:action-contract"
      );
    }
    console.log("Generated Action contract is current.");
    return;
  }
  await mkdir(dirname(outputPath), { recursive: true });
  await writeFile(outputPath, expected, "utf8");
  console.log(`Generated ${outputPath.slice(root.length + 1).replaceAll("\\", "/")}`);
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) {
  await main();
}
