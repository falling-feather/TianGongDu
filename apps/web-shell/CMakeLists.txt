if(NOT WASM)
  message(FATAL_ERROR "apps/web-shell is only available in an Emscripten build.")
endif()

set(TGD_WEB_APP tgd_web_shell)

add_executable(
  ${TGD_WEB_APP}
  Source/AppDelegate.cpp
  Source/AppDelegate.hpp
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

# The F1 host has no external game assets yet. Keeping this list explicitly
# empty prevents a second, accidental resource packaging path.
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
  "${AX_WASM_EXPORTS},_tgd_web_visibility_changed,_tgd_web_focus_changed,_tgd_web_context_lost,_tgd_web_context_restored,_tgd_web_presentation_state"
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
