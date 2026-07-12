if(NOT WASM)
  message(FATAL_ERROR "apps/web-shell is only available in an Emscripten build.")
endif()

set(TGD_WEB_APP tgd_web_shell)

add_executable(
  ${TGD_WEB_APP}
  Source/AppDelegate.cpp
  Source/AppDelegate.hpp
  Source/F1GrayboxLayer.cpp
  Source/F1GrayboxLayer.hpp
  Source/main.cpp
)
target_compile_features(${TGD_WEB_APP} PRIVATE cxx_std_20)
target_include_directories(${TGD_WEB_APP} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_link_libraries(
  ${TGD_WEB_APP}
  PRIVATE
    tgd_gameplay
    tgd_platform_web
    tgd_presentation_axmol
    tgd_content_core
    "${TGD_AXMOL_TARGET}"
)
tgd_enable_strict_warnings(${TGD_WEB_APP})
target_compile_options(
  ${TGD_WEB_APP}
  PRIVATE
    "$<$<COMPILE_LANG_AND_ID:CXX,Clang>:-Wno-gnu-zero-variadic-macro-arguments>"
)
target_link_options(
  ${TGD_WEB_APP}
  PRIVATE
    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/web-abi.generated.js"
    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/indexeddb-v1.generated.js"
    "--pre-js=${CMAKE_CURRENT_SOURCE_DIR}/indexeddb-storage.js"
)
set_property(
  TARGET ${TGD_WEB_APP}
  APPEND
  PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/web-abi.generated.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/indexeddb-v1.generated.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/indexeddb-storage.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/service-worker.js"
)

# The first oblique control graybox is code-drawn and has no external assets.
# Keeping this list explicitly empty prevents a second packaging truth.
set(_APP_RES_FOLDER "")
set(
  AX_WASM_SHELL_FILE
  "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
  CACHE STRING
  "TianGongDu F1 Emscripten shell"
  FORCE
)
set(
  AX_WASM_EXPORTS
  "${AX_WASM_EXPORTS},_malloc,_free,_tgd_web_abi_version,_tgd_web_boot,_tgd_web_submit_platform_event,_tgd_web_submit_ui_command,_tgd_web_peek_platform_request_size,_tgd_web_poll_platform_request,_tgd_web_complete_async_request,_tgd_web_peek_ui_event_size,_tgd_web_poll_ui_event,_tgd_web_request_shutdown,_tgd_web_visibility_changed,_tgd_web_focus_changed,_tgd_web_context_lost,_tgd_web_context_restored,_tgd_web_presentation_state,_tgd_web_f1_qa_player_health,_tgd_web_f1_qa_player_active,_tgd_web_f1_qa_active_hostiles,_tgd_web_f1_qa_retry_count,_tgd_web_f1_qa_quest_beat_index,_tgd_web_f1_qa_quest_completed_objectives,_tgd_web_f1_qa_quest_required_objectives,_tgd_web_f1_qa_incoming_attack_ticks,_tgd_web_f1_qa_player_busy"
  CACHE STRING
  "TianGongDu and Axmol WASM exports"
  FORCE
)

ax_setup_app_config(
  ${TGD_WEB_APP}
  RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/dist/web"
)
ax_setup_app_props(${TGD_WEB_APP})
set_target_properties(
  ${TGD_WEB_APP}
  PROPERTIES
    OUTPUT_NAME "tiangongdu-f1"
)
add_custom_command(
  TARGET ${TGD_WEB_APP}
  POST_BUILD
  COMMAND
    "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/service-worker.js"
    "$<TARGET_FILE_DIR:${TGD_WEB_APP}>/service-worker.js"
  VERBATIM
)
