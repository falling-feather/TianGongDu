add_library(tgd_test_replay_fixture STATIC src/f1_golden_replay.cpp)
target_compile_features(tgd_test_replay_fixture PUBLIC cxx_std_20)
tgd_enable_strict_warnings(tgd_test_replay_fixture)
target_include_directories(
  tgd_test_replay_fixture
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(tgd_test_replay_fixture PUBLIC tgd_runtime tgd_contracts)

add_library(tgd_test_f1_content_replay_fixture STATIC src/f1_content_replay.cpp)
target_compile_features(tgd_test_f1_content_replay_fixture PUBLIC cxx_std_20)
tgd_enable_strict_warnings(tgd_test_f1_content_replay_fixture)
target_include_directories(
  tgd_test_f1_content_replay_fixture
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(tgd_test_f1_content_replay_fixture PUBLIC tgd_contracts)
