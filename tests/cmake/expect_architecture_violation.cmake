foreach(required_variable IN ITEMS TGD_FIXTURE_SOURCE TGD_FIXTURE_BINARY TGD_SOURCE_ROOT)
  if(NOT DEFINED "${required_variable}")
    message(FATAL_ERROR "${required_variable} is required.")
  endif()
endforeach()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}"
    -S "${TGD_FIXTURE_SOURCE}"
    -B "${TGD_FIXTURE_BINARY}"
    "-DTGD_SOURCE_ROOT=${TGD_SOURCE_ROOT}"
  RESULT_VARIABLE configure_result
  OUTPUT_VARIABLE configure_stdout
  ERROR_VARIABLE configure_stderr
)

if(configure_result EQUAL 0)
  message(FATAL_ERROR "Invalid reverse dependency configured successfully.")
endif()

set(configure_output "${configure_stdout}\n${configure_stderr}")
if(NOT configure_output MATCHES "Architecture violation: tgd_contracts links tgd_runtime")
  message(FATAL_ERROR "Fixture failed for an unexpected reason:\n${configure_output}")
endif()

message(STATUS "Reverse dependency was rejected by the architecture guard.")
