if(NOT WASM)
  message(FATAL_ERROR "apps/system-demo-web is only available in an Emscripten build.")
endif()

if(NOT TARGET tgd_prepare_system_demo_package)
  message(
    FATAL_ERROR
    "The system Demo host requires the canonical tgd_prepare_system_demo_package target."
  )
endif()

set(TGD_SYSTEM_DEMO_WEB_APP tgd_system_demo_web)
set(
  TGD_SYSTEM_DEMO_CANONICAL_PACKAGE
  "${CMAKE_BINARY_DIR}/dist/web/system-demo.tgdsbx"
)

add_executable(
  ${TGD_SYSTEM_DEMO_WEB_APP}
  Source/AppDelegate.cpp
  Source/AppDelegate.hpp
  Source/SystemDemoLayer.cpp
  Source/SystemDemoLayer.hpp
  Source/main.cpp
)
add_dependencies(
  ${TGD_SYSTEM_DEMO_WEB_APP}
  tgd_prepare_system_demo_package
)
set_property(
  TARGET ${TGD_SYSTEM_DEMO_WEB_APP}
  APPEND
  PROPERTY LINK_DEPENDS
    "${TGD_SYSTEM_DEMO_CANONICAL_PACKAGE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
)
target_compile_features(${TGD_SYSTEM_DEMO_WEB_APP} PRIVATE cxx_std_20)
target_include_directories(
  ${TGD_SYSTEM_DEMO_WEB_APP}
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)
target_link_libraries(
  ${TGD_SYSTEM_DEMO_WEB_APP}
  PRIVATE
    tgd_sandbox_runtime_integration
    tgd_presentation_axmol
    tgd_platform_web
    "${TGD_AXMOL_TARGET}"
)
tgd_enable_strict_warnings(${TGD_SYSTEM_DEMO_WEB_APP})
target_compile_options(
  ${TGD_SYSTEM_DEMO_WEB_APP}
  PRIVATE
    "$<$<COMPILE_LANG_AND_ID:CXX,Clang>:-Wno-gnu-zero-variadic-macro-arguments>"
)
target_link_options(
  ${TGD_SYSTEM_DEMO_WEB_APP}
  PRIVATE
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "-sFORCE_FILESYSTEM=1"
    "-sSTACK_SIZE=262144"
    "SHELL:--embed-file dist/web/system-demo.tgdsbx@/system-demo.tgdsbx"
)

# The host consumes the package-embedded, path-free Sandbox asset registry.
# No second runtime resource folder or Stable-ID-to-path table is permitted.
set(_APP_RES_FOLDER "")

# Axmol stores these values in the CMake cache. Scope the system Demo exports
# to this target and restore the F1 values afterwards so repeated configure
# runs cannot make either host depend on the other's C symbols or shell.
set(_TGD_PREVIOUS_AX_WASM_SHELL_FILE "${AX_WASM_SHELL_FILE}")
set(_TGD_PREVIOUS_AX_WASM_EXPORTS "${AX_WASM_EXPORTS}")
set(
  AX_WASM_SHELL_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
  CACHE STRING
  "TianGongDu Demo 0.8.x Emscripten shell"
  FORCE
)
set(
  AX_WASM_EXPORTS
  "_main,_axmol_webglcontextlost,_axmol_webglcontextrestored,_axmol_hdoc_visibilitychange,_axmol_onwebclickcallback,_axmol_dev_pause,_axmol_dev_resume,_axmol_dev_step,_tgd_system_demo_qa_ready,_tgd_system_demo_qa_player_x,_tgd_system_demo_qa_player_y,_tgd_system_demo_qa_gate_open,_tgd_system_demo_qa_retry_count,_tgd_system_demo_qa_blocked_move_count,_tgd_system_demo_qa_package_byte_count,_tgd_system_demo_qa_asset_count,_tgd_system_demo_qa_player_health,_tgd_system_demo_qa_active_hostile_count,_tgd_system_demo_qa_defeated_hostile_count,_tgd_system_demo_qa_completed_wave_count,_tgd_system_demo_qa_completed_objective_count,_tgd_system_demo_qa_terminal_completed,_tgd_system_demo_qa_accepted_attack_count,_tgd_system_demo_qa_repeated_trigger_count,_tgd_system_demo_qa_operate,_tgd_system_demo_qa_attack_light,_tgd_system_demo_qa_attack_heavy,_tgd_system_demo_qa_retry"
  CACHE STRING
  "TianGongDu system Demo and Axmol WASM exports"
  FORCE
)

ax_setup_app_config(
  ${TGD_SYSTEM_DEMO_WEB_APP}
  RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/dist/web"
)
ax_setup_app_props(${TGD_SYSTEM_DEMO_WEB_APP})
set_target_properties(
  ${TGD_SYSTEM_DEMO_WEB_APP}
  PROPERTIES
    OUTPUT_NAME "tiangongdu-system-demo"
)

set(
  AX_WASM_SHELL_FILE
  "${_TGD_PREVIOUS_AX_WASM_SHELL_FILE}"
  CACHE STRING
  "Axmol Emscripten shell"
  FORCE
)
set(
  AX_WASM_EXPORTS
  "${_TGD_PREVIOUS_AX_WASM_EXPORTS}"
  CACHE STRING
  "Axmol WASM exports"
  FORCE
)
