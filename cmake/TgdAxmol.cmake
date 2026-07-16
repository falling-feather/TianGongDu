include_guard(GLOBAL)

set(_TGD_AXMOL_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

macro(tgd_enable_axmol)
  if(NOT TGD_AXMOL_ROOT)
    message(FATAL_ERROR "TGD_ENABLE_AXMOL_HOST requires TGD_AXMOL_ROOT or AX_ROOT.")
  endif()

  file(REAL_PATH "${TGD_AXMOL_ROOT}" _AX_ROOT)
  if(NOT EXISTS "${_AX_ROOT}/core/CMakeLists.txt")
    message(FATAL_ERROR "TGD_AXMOL_ROOT is not an Axmol source tree: ${_AX_ROOT}")
  endif()

  set(ENV{AX_ROOT} "${_AX_ROOT}")
  enable_language(C)
  set(_is_axmol_embed FALSE)
  set(_AX_USE_PREBUILT FALSE)
  set(ENGINE_BINARY_PATH "${CMAKE_CURRENT_BINARY_DIR}/engine")

  # F1 uses Axmol as a narrow host/lifecycle probe. Gameplay owns collision
  # and state, so unrelated engine modules stay disabled until the ADR-0009
  # representative 2.5D slice proves which rendering modules are required.
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(AX_EXT_HINT OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_3D OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_3D_PHYSICS OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_NAVMESH OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_PHYSICS OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_MFMEDIA OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_VLC_MEDIA OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_WEBSOCKET OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_HTTP OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_OPUS OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_MSEDGE_WEBVIEW2 OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_CONSOLE OFF CACHE BOOL "" FORCE)
  set(AX_ENABLE_AUDIO ON CACHE BOOL "" FORCE)
  set(AX_UPDATE_BUILD_VERSION OFF CACHE BOOL "" FORCE)
  set(AX_WASM_ASSETS_PRELOAD_FILE OFF CACHE BOOL "" FORCE)
  set(AX_WASM_ENABLE_DEVTOOLS ON CACHE BOOL "" FORCE)
  # Axmol 2.11.4 documents 0 as disabled, but its numeric-value condition
  # still appends -pthread for 0. An empty value is the implementation's
  # actual off switch for this pinned release.
  set(AX_WASM_THREADS "" CACHE STRING "F1 Web Single disables wasm pthreads" FORCE)

  if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    if(NOT TGD_WEB_STAGING_ROOT)
      message(FATAL_ERROR "Web builds require TGD_WEB_STAGING_ROOT.")
    endif()

    file(TO_CMAKE_PATH "${TGD_WEB_STAGING_ROOT}" _tgd_web_staging_root)
    string(REGEX MATCH "[^ -~]" _tgd_web_staging_non_ascii "${_tgd_web_staging_root}")
    if(_tgd_web_staging_non_ascii)
      message(FATAL_ERROR
        "TGD_WEB_STAGING_ROOT must be ASCII for Emscripten 3.1.73 on Windows: "
        "${_tgd_web_staging_root}"
      )
    endif()

    string(SHA256 _tgd_web_source_hash "${CMAKE_SOURCE_DIR}")
    string(SUBSTRING "${_tgd_web_source_hash}" 0 12 _tgd_web_source_id)
    string(TOLOWER "${CMAKE_BUILD_TYPE}" _tgd_web_configuration)
    set(
      AXSLCC_OUT_DIR
      "${_tgd_web_staging_root}/${_tgd_web_source_id}/${_tgd_web_configuration}/axslc"
      CACHE STRING
      "ASCII shader staging for the F1 Web Single host"
      FORCE
    )
  endif()

  list(PREPEND CMAKE_MODULE_PATH "${_AX_ROOT}/cmake/Modules")
  include(AXBuildSet)
  add_subdirectory("${_AX_ROOT}/core" "${ENGINE_BINARY_PATH}/axmol/core")

  set(TGD_AXMOL_TARGET "${_AX_CORE_LIB}")
  get_target_property(
    _tgd_axmol_public_includes
    "${TGD_AXMOL_TARGET}"
    INTERFACE_INCLUDE_DIRECTORIES
  )
  if(_tgd_axmol_public_includes)
    # Keep warnings-as-errors for TianGongDu sources without treating the
    # pinned engine's public headers as project-owned code.
    set_property(
      TARGET "${TGD_AXMOL_TARGET}"
      APPEND PROPERTY INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
      ${_tgd_axmol_public_includes}
    )
  endif()

  if(WASM)
    if("-pthread" IN_LIST _ax_compile_options OR "-pthread" IN_LIST _AX_EM_LD_FLAGS)
      message(FATAL_ERROR "F1 Web Single must not contain Axmol pthread flags.")
    endif()

    # Axmol 2.11.4 uses htonl in ParticleSystem.cpp without including the
    # declaring header. Keep the compatibility shim project-owned and
    # version-scoped instead of mutating the verified release archive.
    target_compile_options("${TGD_AXMOL_TARGET}" PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:-include>"
      "$<$<COMPILE_LANGUAGE:CXX>:${_TGD_AXMOL_MODULE_DIR}/compat/axmol-2.11.4-wasm.hpp>"
    )
  endif()
  message(STATUS "TianGongDu Axmol host enabled: ${_AX_ROOT} (${TGD_AXMOL_TARGET})")
endmacro()
