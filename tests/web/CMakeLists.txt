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
