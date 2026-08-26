include_guard(GLOBAL)

function(tgd_enable_strict_warnings target)
  if(MSVC)
    target_compile_options("${target}" PRIVATE /W4 /WX /permissive- /Zc:__cplusplus)
  else()
    target_compile_options("${target}" PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()
