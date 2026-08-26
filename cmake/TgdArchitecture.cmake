include_guard(GLOBAL)

function(tgd_register_layer target)
  cmake_parse_arguments(TGD "" "" "ALLOW" ${ARGN})
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "Cannot register missing architecture target: ${target}")
  endif()

  set_property(GLOBAL APPEND PROPERTY TGD_ARCHITECTURE_TARGETS "${target}")
  set_property(TARGET "${target}" PROPERTY TGD_ALLOWED_PROJECT_DEPENDENCIES "${TGD_ALLOW}")
endfunction()

function(tgd_validate_architecture)
  get_property(targets GLOBAL PROPERTY TGD_ARCHITECTURE_TARGETS)
  foreach(target IN LISTS targets)
    get_target_property(allowed "${target}" TGD_ALLOWED_PROJECT_DEPENDENCIES)
    get_target_property(private_links "${target}" LINK_LIBRARIES)
    get_target_property(public_links "${target}" INTERFACE_LINK_LIBRARIES)

    set(all_links ${private_links} ${public_links})
    list(REMOVE_DUPLICATES all_links)
    foreach(dependency IN LISTS all_links)
      if(dependency MATCHES "^tgd_" AND NOT dependency IN_LIST allowed)
        message(
          FATAL_ERROR
          "Architecture violation: ${target} links ${dependency}; allowed project dependencies: ${allowed}"
        )
      endif()
    endforeach()
  endforeach()
endfunction()
