cmake_minimum_required(VERSION 3.25)

project(TgdArchitectureViolation LANGUAGES NONE)

if(NOT DEFINED TGD_SOURCE_ROOT)
  message(FATAL_ERROR "TGD_SOURCE_ROOT is required for this fixture.")
endif()

list(APPEND CMAKE_MODULE_PATH "${TGD_SOURCE_ROOT}/cmake")
include(TgdArchitecture)

add_library(tgd_contracts INTERFACE)
add_library(tgd_runtime INTERFACE)

target_link_libraries(tgd_contracts INTERFACE tgd_runtime)
tgd_register_layer(tgd_contracts)
tgd_register_layer(tgd_runtime ALLOW tgd_contracts)

tgd_validate_architecture()
