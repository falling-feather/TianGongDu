if(NOT WASM)
  message(FATAL_ERROR "tests/web replay probe is only available in an Emscripten build.")
endif()

add_executable(tgd_web_replay_probe replay_probe.cpp)
target_compile_features(tgd_web_replay_probe PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_web_replay_probe)
target_link_libraries(
  tgd_web_replay_probe
  PRIVATE
    tgd_contracts
    tgd_runtime
    tgd_test_replay_fixture
)
target_link_options(tgd_web_replay_probe PRIVATE "-sEXIT_RUNTIME=1")
set_target_properties(
  tgd_web_replay_probe
  PROPERTIES
    OUTPUT_NAME "tgd-replay-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/web"
    SUFFIX ".html"
)

# Compile the same contract probe used by Native so Web cannot silently drift
# from the typed authoring projection, canonical producer, or owning lifetime.
add_executable(
  tgd_web_sandbox_package_compiler
  ../native/sandbox_package_compiler.cpp
)
target_compile_features(tgd_web_sandbox_package_compiler PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_web_sandbox_package_compiler)
target_link_libraries(
  tgd_web_sandbox_package_compiler
  PRIVATE
    tgd_contracts
    tgd_content_core
)
target_link_options(tgd_web_sandbox_package_compiler PRIVATE "-sEXIT_RUNTIME=1")
set_target_properties(
  tgd_web_sandbox_package_compiler
  PROPERTIES
    OUTPUT_NAME "tgd-sandbox-package-compiler-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/web"
    SUFFIX ".html"
)

# Build and execute the same last-valid publication probe on Web so provider
# move/lifetime and failure-preservation semantics cannot silently drift.
add_executable(
  tgd_web_sandbox_package_provider
  ../native/sandbox_package_provider.cpp
)
target_compile_features(tgd_web_sandbox_package_provider PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_web_sandbox_package_provider)
target_link_libraries(
  tgd_web_sandbox_package_provider
  PRIVATE
    tgd_contracts
    tgd_content_core
)
target_link_options(tgd_web_sandbox_package_provider PRIVATE "-sEXIT_RUNTIME=1")
set_target_properties(
  tgd_web_sandbox_package_provider
  PROPERTIES
    OUTPUT_NAME "tgd-sandbox-package-provider-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/web"
    SUFFIX ".html"
)

# Build the exact Native service/artifact contract probe as a modular
# Node-targeted WebAssembly evidence module and execute it through the service
# client. This is not the production Workbench loader/deployment path;
# link-only is not a pass.
add_executable(
  tgd_web_sandbox_package_service_abi
  ../native/sandbox_package_service_abi.cpp
)
target_compile_features(tgd_web_sandbox_package_service_abi PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_web_sandbox_package_service_abi)
target_link_libraries(
  tgd_web_sandbox_package_service_abi
  PRIVATE
    tgd_contracts
    tgd_content_core
)
target_link_options(
  tgd_web_sandbox_package_service_abi
  PRIVATE
    "-sWASM_BIGINT=1"
    "-sMODULARIZE=1"
    "-sEXPORT_ES6=1"
    "-sENVIRONMENT=node"
    "-sINVOKE_RUN=0"
    "-sEXIT_RUNTIME=0"
    "-sALLOW_MEMORY_GROWTH=1"
    "SHELL:-sEXPORTED_FUNCTIONS=['_malloc','_free','_tgd_sandbox_service_run_contract_probe','_tgd_sandbox_service_write_invalid_utf8_result','_tgd_sandbox_compiler_service_abi_version','_tgd_sandbox_compiler_service_create','_tgd_sandbox_compiler_service_destroy','_tgd_sandbox_compiler_service_read_identity','_tgd_sandbox_compile_request_create','_tgd_sandbox_compile_request_cancel','_tgd_sandbox_compile_request_copy_utf8','_tgd_sandbox_compile_request_set_metadata','_tgd_sandbox_compile_request_set_player','_tgd_sandbox_compile_request_append_region','_tgd_sandbox_compile_request_append_asset','_tgd_sandbox_compile_request_append_actor','_tgd_sandbox_compile_request_append_ground_blocker','_tgd_sandbox_compile_request_append_safe_point','_tgd_sandbox_compile_request_append_interaction','_tgd_sandbox_compile_request_append_mechanism','_tgd_sandbox_compile_request_append_wave','_tgd_sandbox_compile_request_append_wave_spawn','_tgd_sandbox_compile_request_append_objective','_tgd_sandbox_compile_request_append_interaction_binding','_tgd_sandbox_compile_request_append_mechanism_binding','_tgd_sandbox_compile_request_append_actor_binding','_tgd_sandbox_compile_request_submit']"
)
set_target_properties(
  tgd_web_sandbox_package_service_abi
  PROPERTIES
    OUTPUT_NAME "tgd-sandbox-package-service-abi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/web"
    SUFFIX ".mjs"
)

find_program(TGD_LOCKED_NODE_EXECUTABLE NAMES node REQUIRED)

set(
  TGD_SYSTEM_DEMO_PACKAGE_PREPARE_SCRIPT
  "${CMAKE_CURRENT_BINARY_DIR}/prepare-system-demo-package.mjs"
)
set(
  TGD_SYSTEM_DEMO_CANONICAL_PACKAGE
  "${CMAKE_BINARY_DIR}/dist/web/system-demo.tgdsbx"
)
set(
  TGD_SYSTEM_DEMO_CHANGED_CANONICAL_PACKAGE
  "${CMAKE_BINARY_DIR}/dist/web/system-demo.changed.tgdsbx"
)
file(
  WRITE
  "${TGD_SYSTEM_DEMO_PACKAGE_PREPARE_SCRIPT}"
  [=[
import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import { basename, dirname } from "node:path";
import { pathToFileURL } from "node:url";

const [modulePath, wasmPath, clientPath, sourcePath, outputPath, changedOutputPath] =
  process.argv.slice(2);
const sourceBytes = await readFile(sourcePath);
const sourceHash = createHash("sha256").update(sourceBytes).digest("hex");
if (sourceHash !== "07fa5537fc617e0686605a4b7eaeb20ceea24ab7696f5db63e42b23e68e0bb69") {
  throw new Error("system-demo author source SHA-256 drifted: " + sourceHash);
}

const authoring = JSON.parse(sourceBytes.toString("utf8"));
const moduleFactory = (await import(pathToFileURL(modulePath).href)).default;
const { SandboxPackageServiceClient } =
  await import(pathToFileURL(clientPath).href);
const module = await moduleFactory({
  noInitialRun: true,
  locateFile(name) {
    if (name !== basename(wasmPath)) throw new Error("unexpected WASM dependency " + name);
    return wasmPath;
  },
});
const client = SandboxPackageServiceClient.create(module);
const hex = (bytes) => Buffer.from(bytes).toString("hex");
const packageHash = (bytes) =>
  createHash("sha256").update(bytes).digest("hex");
const requirePublished = (result, generation) => {
  if (result.outcome !== 1 || result.identity.generation !== generation ||
      !(result.packageBytes instanceof Uint8Array)) {
    throw new Error("compiler/provider did not publish generation " + generation);
  }
};

try {
  const first = client.publish(authoring.runtime);
  requirePublished(first, 1);
  const firstIdentity = {
    bytes: first.packageBytes.length,
    packageSha256: packageHash(first.packageBytes),
    providerChecksum: hex(first.identity.checksum),
  };
  if (firstIdentity.bytes !== 2960 ||
      firstIdentity.packageSha256 !==
        "a09ce35e58a48c01b15d4946de8e380d752760142ddbefee7bce7f7476b0252e" ||
      firstIdentity.providerChecksum !==
        "0cfd8c17732278074deb08dccf5f1eb5d4abf1f59ffbaa1de610eeac7d87e2e9") {
    throw new Error(
      "system-demo canonical package identity drifted: " +
        JSON.stringify(firstIdentity)
    );
  }

  const second = client.publish(authoring.runtime);
  requirePublished(second, 2);
  if (hex(second.identity.checksum) !== hex(first.identity.checksum) ||
      packageHash(second.packageBytes) !== packageHash(first.packageBytes)) {
    throw new Error("same-source provider republish drifted");
  }

  const changedRuntime = structuredClone(authoring.runtime);
  changedRuntime.interactionBindings[0].rangeMm = 1300;
  const changed = client.publish(changedRuntime);
  requirePublished(changed, 3);
  if (hex(changed.identity.checksum) === hex(first.identity.checksum)) {
    throw new Error("changed package retained the source checksum");
  }

  await mkdir(dirname(outputPath), { recursive: true });
  await writeFile(outputPath, first.packageBytes);
  await writeFile(changedOutputPath, changed.packageBytes);
  process.stdout.write(JSON.stringify({
    sourceSha256: sourceHash,
    packageSha256: packageHash(first.packageBytes),
    providerChecksum: hex(first.identity.checksum),
    sameChecksumGeneration: second.identity.generation,
    changedGeneration: changed.identity.generation,
  }) + "\n");
} finally {
  client.destroy();
}
]=]
)

add_custom_command(
  OUTPUT
    "${TGD_SYSTEM_DEMO_CANONICAL_PACKAGE}"
    "${TGD_SYSTEM_DEMO_CHANGED_CANONICAL_PACKAGE}"
  COMMAND
    "${TGD_LOCKED_NODE_EXECUTABLE}"
    "${TGD_SYSTEM_DEMO_PACKAGE_PREPARE_SCRIPT}"
    "$<TARGET_FILE:tgd_web_sandbox_package_service_abi>"
    "${CMAKE_BINARY_DIR}/dist/web/tgd-sandbox-package-service-abi.wasm"
    "${CMAKE_SOURCE_DIR}/apps/content-workbench/src/sandbox-package-service-client.mjs"
    "${CMAKE_SOURCE_DIR}/content/design/system-demo.sandbox.json"
    "${TGD_SYSTEM_DEMO_CANONICAL_PACKAGE}"
    "${TGD_SYSTEM_DEMO_CHANGED_CANONICAL_PACKAGE}"
  DEPENDS
    tgd_web_sandbox_package_service_abi
    "${TGD_SYSTEM_DEMO_PACKAGE_PREPARE_SCRIPT}"
    "${CMAKE_SOURCE_DIR}/apps/content-workbench/src/sandbox-package-service-client.mjs"
    "${CMAKE_SOURCE_DIR}/content/design/system-demo.sandbox.json"
  VERBATIM
)
add_custom_target(
  tgd_prepare_system_demo_package
  DEPENDS
    "${TGD_SYSTEM_DEMO_CANONICAL_PACKAGE}"
    "${TGD_SYSTEM_DEMO_CHANGED_CANONICAL_PACKAGE}"
)

add_executable(
  tgd_web_sandbox_runtime_coordinator
  ../native/sandbox_runtime_coordinator.cpp
)
add_dependencies(
  tgd_web_sandbox_runtime_coordinator
  tgd_prepare_system_demo_package
)
set_property(
  TARGET tgd_web_sandbox_runtime_coordinator
  APPEND
  PROPERTY LINK_DEPENDS
    "${TGD_SYSTEM_DEMO_CANONICAL_PACKAGE}"
    "${TGD_SYSTEM_DEMO_CHANGED_CANONICAL_PACKAGE}"
)
target_compile_features(
  tgd_web_sandbox_runtime_coordinator
  PRIVATE cxx_std_20
)
tgd_enable_strict_warnings(tgd_web_sandbox_runtime_coordinator)
target_compile_definitions(
  tgd_web_sandbox_runtime_coordinator
  PRIVATE
    TGD_SYSTEM_DEMO_CANONICAL_PACKAGE_PATH="/system-demo.tgdsbx"
    TGD_SYSTEM_DEMO_CHANGED_CANONICAL_PACKAGE_PATH="/system-demo.changed.tgdsbx"
)
target_link_libraries(
  tgd_web_sandbox_runtime_coordinator
  PRIVATE tgd_sandbox_runtime_integration
)
target_link_options(
  tgd_web_sandbox_runtime_coordinator
  PRIVATE
    "-sENVIRONMENT=node"
    "-sEXIT_RUNTIME=1"
    "-sFORCE_FILESYSTEM=1"
    "-sSTACK_SIZE=131072"
    "SHELL:--embed-file dist/web/system-demo.tgdsbx@/system-demo.tgdsbx"
    "SHELL:--embed-file dist/web/system-demo.changed.tgdsbx@/system-demo.changed.tgdsbx"
)
set_target_properties(
  tgd_web_sandbox_runtime_coordinator
  PROPERTIES
    OUTPUT_NAME "tgd-sandbox-runtime-coordinator-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/web"
    SUFFIX ".cjs"
)

# Execute the exact Native resolver/last-valid probe against the same
# build-generated, path-free registry. This is product resolver evidence, not a
# Preview Host, renderer, or browser-visible asset integration claim.
add_executable(
  tgd_web_sandbox_asset_resolver
  ../native/sandbox_asset_resolver.cpp
)
add_dependencies(
  tgd_web_sandbox_asset_resolver
  tgd_prepare_system_demo_package
)
set_property(
  TARGET tgd_web_sandbox_asset_resolver
  APPEND
  PROPERTY LINK_DEPENDS "${TGD_SYSTEM_DEMO_CANONICAL_PACKAGE}"
)
target_compile_features(
  tgd_web_sandbox_asset_resolver
  PRIVATE cxx_std_20
)
target_compile_options(
  tgd_web_sandbox_asset_resolver
  PRIVATE -fexceptions
)
tgd_enable_strict_warnings(tgd_web_sandbox_asset_resolver)
target_compile_definitions(
  tgd_web_sandbox_asset_resolver
  PRIVATE
    TGD_SYSTEM_DEMO_CANONICAL_PACKAGE_PATH="/system-demo.tgdsbx"
)
target_link_libraries(
  tgd_web_sandbox_asset_resolver
  PRIVATE
    tgd_presentation_axmol
    tgd_sandbox_runtime_integration
)
target_link_options(
  tgd_web_sandbox_asset_resolver
  PRIVATE
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-sENVIRONMENT=node"
    "-sEXIT_RUNTIME=1"
    "-sFORCE_FILESYSTEM=1"
    # The combined resolver + runtime-coordinator composition probe exceeded
    # the coordinator-only 128 KiB stack by 4,176 bytes under assertions.
    "-sSTACK_SIZE=147456"
    "SHELL:--embed-file dist/web/system-demo.tgdsbx@/system-demo.tgdsbx"
)
set_target_properties(
  tgd_web_sandbox_asset_resolver
  PROPERTIES
    OUTPUT_NAME "tgd-sandbox-asset-resolver-probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/web"
    SUFFIX ".cjs"
)
