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

# Build the exact Native service contract probe as a modular Node-targeted
# WebAssembly evidence module and execute it through the service client. This is
# not the production Workbench loader/deployment path; link-only is not a pass.
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
    "SHELL:-sEXPORTED_FUNCTIONS=['_malloc','_free','_tgd_sandbox_service_run_contract_probe','_tgd_sandbox_service_write_invalid_utf8_result','_tgd_sandbox_compiler_service_abi_version','_tgd_sandbox_compiler_service_create','_tgd_sandbox_compiler_service_destroy','_tgd_sandbox_compiler_service_read_identity','_tgd_sandbox_compile_request_create','_tgd_sandbox_compile_request_cancel','_tgd_sandbox_compile_request_copy_utf8','_tgd_sandbox_compile_request_set_metadata','_tgd_sandbox_compile_request_set_player','_tgd_sandbox_compile_request_append_region','_tgd_sandbox_compile_request_append_asset','_tgd_sandbox_compile_request_append_actor','_tgd_sandbox_compile_request_append_ground_blocker','_tgd_sandbox_compile_request_append_safe_point','_tgd_sandbox_compile_request_append_interaction','_tgd_sandbox_compile_request_append_mechanism','_tgd_sandbox_compile_request_append_wave','_tgd_sandbox_compile_request_append_wave_spawn','_tgd_sandbox_compile_request_append_objective','_tgd_sandbox_compile_request_append_interaction_binding','_tgd_sandbox_compile_request_append_mechanism_binding','_tgd_sandbox_compile_request_submit']"
)
set_target_properties(
  tgd_web_sandbox_package_service_abi
  PROPERTIES
    OUTPUT_NAME "tgd-sandbox-package-service-abi"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist/web"
    SUFFIX ".mjs"
)
