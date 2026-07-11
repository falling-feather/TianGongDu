add_executable(tgd_native_bootstrap_smoke bootstrap_smoke.cpp)
target_compile_features(tgd_native_bootstrap_smoke PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_bootstrap_smoke)
target_link_libraries(
  tgd_native_bootstrap_smoke
  PRIVATE
    tgd_contracts
    tgd_runtime
    tgd_gameplay
    tgd_presentation_axmol
    tgd_platform_native
    tgd_content_core
    tgd_sync_contracts
)

add_test(NAME tgd.native.bootstrap-smoke COMMAND tgd_native_bootstrap_smoke)

add_executable(tgd_native_game_session_determinism game_session_determinism.cpp)
target_compile_features(tgd_native_game_session_determinism PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_game_session_determinism)
target_link_libraries(
  tgd_native_game_session_determinism
  PRIVATE
    tgd_contracts
    tgd_runtime
)

add_test(
  NAME tgd.native.game-session-determinism
  COMMAND tgd_native_game_session_determinism
)

add_test(
  NAME tgd.architecture.reject-reverse-link
  COMMAND
    "${CMAKE_COMMAND}"
    "-DTGD_FIXTURE_SOURCE=${CMAKE_SOURCE_DIR}/tests/fixtures/cmake-architecture-violation"
    "-DTGD_FIXTURE_BINARY=${CMAKE_CURRENT_BINARY_DIR}/architecture-violation"
    "-DTGD_SOURCE_ROOT=${CMAKE_SOURCE_DIR}"
    -P "${CMAKE_SOURCE_DIR}/tests/cmake/expect_architecture_violation.cmake"
)
