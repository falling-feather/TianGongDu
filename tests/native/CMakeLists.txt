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

add_executable(tgd_native_command_replay command_replay.cpp)
target_compile_features(tgd_native_command_replay PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_command_replay)
target_link_libraries(
  tgd_native_command_replay
  PRIVATE
    tgd_contracts
    tgd_runtime
    tgd_test_replay_fixture
)

add_test(NAME tgd.native.command-replay COMMAND tgd_native_command_replay)

add_executable(tgd_native_save_envelope save_envelope.cpp)
target_compile_features(tgd_native_save_envelope PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_save_envelope)
target_link_libraries(tgd_native_save_envelope PRIVATE tgd_contracts)

add_test(NAME tgd.native.save-envelope COMMAND tgd_native_save_envelope)

add_executable(tgd_native_profile_storage profile_storage_coordinator.cpp)
target_compile_features(tgd_native_profile_storage PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_profile_storage)
target_link_libraries(tgd_native_profile_storage PRIVATE tgd_contracts tgd_runtime)

add_test(NAME tgd.native.profile-storage COMMAND tgd_native_profile_storage)

add_executable(tgd_native_web_platform_bridge web_platform_bridge.cpp)
target_compile_features(tgd_native_web_platform_bridge PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_web_platform_bridge)
target_link_libraries(
  tgd_native_web_platform_bridge
  PRIVATE
    tgd_contracts
    tgd_runtime
    tgd_platform_web
)

add_test(NAME tgd.native.web-platform-bridge COMMAND tgd_native_web_platform_bridge)

add_executable(tgd_native_session_input_state session_input_state.cpp)
target_compile_features(tgd_native_session_input_state PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_session_input_state)
target_link_libraries(
  tgd_native_session_input_state
  PRIVATE
    tgd_contracts
    tgd_runtime
    tgd_gameplay
)

add_test(
  NAME tgd.native.session-input-state
  COMMAND tgd_native_session_input_state
)

add_executable(tgd_native_f1_content_contract f1_content_contract.cpp)
target_compile_features(tgd_native_f1_content_contract PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_f1_content_contract)
target_link_libraries(
  tgd_native_f1_content_contract
  PRIVATE
    tgd_contracts
    tgd_content_core
)

add_test(
  NAME tgd.native.f1-content-contract
  COMMAND tgd_native_f1_content_contract
)

add_executable(tgd_native_vertical_slice_session vertical_slice_session.cpp)
target_compile_features(tgd_native_vertical_slice_session PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_vertical_slice_session)
target_link_libraries(
  tgd_native_vertical_slice_session
  PRIVATE
    tgd_contracts
    tgd_runtime
    tgd_gameplay
    tgd_content_core
)

add_test(
  NAME tgd.native.vertical-slice-session
  COMMAND tgd_native_vertical_slice_session
)

add_executable(tgd_native_combat_resolver combat_resolver.cpp)
target_compile_features(tgd_native_combat_resolver PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_combat_resolver)
target_link_libraries(
  tgd_native_combat_resolver
  PRIVATE
    tgd_contracts
    tgd_gameplay
)

add_test(
  NAME tgd.native.combat-resolver
  COMMAND tgd_native_combat_resolver
)

add_executable(tgd_native_encounter_director encounter_director.cpp)
target_compile_features(tgd_native_encounter_director PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_encounter_director)
target_link_libraries(
  tgd_native_encounter_director
  PRIVATE
    tgd_contracts
    tgd_gameplay
)

add_test(
  NAME tgd.native.encounter-director
  COMMAND tgd_native_encounter_director
)

add_executable(tgd_native_quest_runtime quest_runtime.cpp)
target_compile_features(tgd_native_quest_runtime PRIVATE cxx_std_20)
tgd_enable_strict_warnings(tgd_native_quest_runtime)
target_link_libraries(
  tgd_native_quest_runtime
  PRIVATE
    tgd_contracts
    tgd_gameplay
    tgd_content_core
)

add_test(
  NAME tgd.native.quest-runtime
  COMMAND tgd_native_quest_runtime
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
