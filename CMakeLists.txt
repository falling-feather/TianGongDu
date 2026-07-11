cmake_minimum_required(VERSION 3.25)

project(TianGongDu LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(TgdArchitecture)
include(TgdAxmol)
include(TgdWarnings)

option(TGD_BUILD_TESTS "Build TianGongDu native and cross-compiled smoke tests." ON)
option(TGD_BUILD_F1_REPLAY_PROBE "Build the F1 command replay Web probe." OFF)
option(TGD_ENABLE_AXMOL_HOST "Build the Axmol presentation host." OFF)
set(TGD_AXMOL_ROOT "$ENV{AX_ROOT}" CACHE PATH "Path to the locked Axmol source tree.")
set(
  TGD_WEB_STAGING_ROOT
  ""
  CACHE PATH
  "ASCII-only staging root for Emscripten file packaging on Windows."
)
set(TGD_CHANNEL "prototype_f1" CACHE STRING "Build channel embedded in diagnostics.")

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(TGD_ENABLE_AXMOL_HOST)
  tgd_enable_axmol()
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/package.json" TGD_PACKAGE_JSON)
string(JSON TGD_SEMANTIC_VERSION GET "${TGD_PACKAGE_JSON}" version)

set(TGD_GIT_COMMIT "unknown")
find_package(Git QUIET)
if(Git_FOUND)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE TGD_GIT_RESULT
    OUTPUT_VARIABLE TGD_GIT_OUTPUT
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(TGD_GIT_RESULT EQUAL 0)
    set(TGD_GIT_COMMIT "${TGD_GIT_OUTPUT}")
  endif()
endif()

set(TGD_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/tgd_build_config.hpp.in"
  "${TGD_GENERATED_INCLUDE_DIR}/tgd/contracts/build_config.hpp"
  @ONLY
)

function(tgd_add_static_layer target)
  add_library("${target}" STATIC)
  target_compile_features("${target}" PUBLIC cxx_std_20)
  tgd_enable_strict_warnings("${target}")
endfunction()

tgd_add_static_layer(tgd_contracts)
target_sources(
  tgd_contracts
  PRIVATE
    src/contracts/src/build_identity.cpp
    src/contracts/src/save_envelope.cpp
    src/contracts/src/sha256.cpp
)
target_include_directories(
  tgd_contracts
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/contracts/include>"
    "$<BUILD_INTERFACE:${TGD_GENERATED_INCLUDE_DIR}>"
)
tgd_register_layer(tgd_contracts)

tgd_add_static_layer(tgd_runtime)
target_sources(
  tgd_runtime
  PRIVATE
    src/runtime/src/collision_world.cpp
    src/runtime/src/command_replay.cpp
    src/runtime/src/fixed_step_driver.cpp
    src/runtime/src/game_session.cpp
    src/runtime/src/profile_storage_coordinator.cpp
    src/runtime/src/runtime_facade.cpp
)
target_include_directories(
  tgd_runtime
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/include>"
)
target_link_libraries(tgd_runtime PUBLIC tgd_contracts)
tgd_register_layer(tgd_runtime ALLOW tgd_contracts)

tgd_add_static_layer(tgd_gameplay)
target_sources(
  tgd_gameplay
  PRIVATE
    src/gameplay/src/combat_resolver.cpp
    src/gameplay/src/encounter_director.cpp
    src/gameplay/src/layer_probe.cpp
    src/gameplay/src/quest_runtime.cpp
    src/gameplay/src/session_input_state.cpp
    src/gameplay/src/vertical_slice_session.cpp
)
target_include_directories(
  tgd_gameplay
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/gameplay/include>"
)
target_link_libraries(tgd_gameplay PUBLIC tgd_runtime tgd_contracts)
tgd_register_layer(tgd_gameplay ALLOW tgd_runtime tgd_contracts)

tgd_add_static_layer(tgd_presentation_axmol)
target_sources(
  tgd_presentation_axmol
  PRIVATE src/presentation-axmol/src/presentation_lifecycle.cpp
)
target_include_directories(
  tgd_presentation_axmol
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/presentation-axmol/include>"
)
target_link_libraries(tgd_presentation_axmol PUBLIC tgd_runtime tgd_contracts)
if(TGD_ENABLE_AXMOL_HOST)
  target_link_libraries(tgd_presentation_axmol PRIVATE "${TGD_AXMOL_TARGET}")
endif()
tgd_register_layer(tgd_presentation_axmol ALLOW tgd_runtime tgd_contracts)

tgd_add_static_layer(tgd_platform_web)
target_sources(
  tgd_platform_web
  PRIVATE
    src/platform/web/src/layer_probe.cpp
    src/platform/web/src/web_platform_bridge.cpp
)
target_include_directories(
  tgd_platform_web
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web/include>"
)
target_link_libraries(tgd_platform_web PUBLIC tgd_runtime tgd_contracts)
tgd_register_layer(tgd_platform_web ALLOW tgd_runtime tgd_contracts)

tgd_add_static_layer(tgd_platform_native)
target_sources(tgd_platform_native PRIVATE src/platform/native/src/layer_probe.cpp)
target_link_libraries(tgd_platform_native PUBLIC tgd_runtime tgd_contracts)
tgd_register_layer(tgd_platform_native ALLOW tgd_runtime tgd_contracts)

tgd_add_static_layer(tgd_content_core)
target_sources(
  tgd_content_core
  PRIVATE
    src/content-core/src/content_definition_provider.cpp
    src/content-core/src/layer_probe.cpp
)
target_include_directories(
  tgd_content_core
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/content-core/include>"
)
target_link_libraries(tgd_content_core PUBLIC tgd_contracts)
tgd_register_layer(tgd_content_core ALLOW tgd_contracts)

tgd_add_static_layer(tgd_sync_contracts)
target_sources(tgd_sync_contracts PRIVATE src/sync-contracts/src/layer_probe.cpp)
target_link_libraries(tgd_sync_contracts PUBLIC tgd_contracts)
tgd_register_layer(tgd_sync_contracts ALLOW tgd_contracts)

# The service implementation remains outside the G1 critical path. The interface
# target reserves its dependency direction without pretending Drogon/DB code exists.
add_library(tgd_sync_service INTERFACE)
target_link_libraries(tgd_sync_service INTERFACE tgd_sync_contracts)
tgd_register_layer(tgd_sync_service ALLOW tgd_sync_contracts)

if(TGD_ENABLE_AXMOL_HOST)
  add_subdirectory(apps/web-shell)
endif()

tgd_validate_architecture()

if(TGD_BUILD_TESTS OR TGD_BUILD_F1_REPLAY_PROBE)
  add_subdirectory(tests/replay)
endif()

if(TGD_BUILD_F1_REPLAY_PROBE)
  add_subdirectory(tests/web)
endif()

if(TGD_BUILD_TESTS)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(tests/native)
  endif()
endif()
