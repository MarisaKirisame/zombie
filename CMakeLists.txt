cmake_minimum_required(VERSION 3.10)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(zombie)

add_library(zombie_lib INTERFACE)
target_include_directories(zombie_lib INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

target_compile_features(zombie_lib INTERFACE cxx_std_17)

install(TARGETS zombie_lib EXPORT zombie)
install(DIRECTORY include DESTINATION ./)

install(EXPORT zombie DESTINATION lib/cmake/zombie)

include(CMakePackageConfigHelpers)

configure_package_config_file(
  Config.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/zombie-config.cmake
  INSTALL_DESTINATION lib/cmake)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/zombie-config.cmake
  DESTINATION lib/cmake/zombie)

add_executable(debug debug.cpp)
target_link_libraries(debug zombie_lib)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

enable_testing()

file(GLOB zombie_test_src "test/*.cc")
add_executable(
  zombie_test
  ${zombie_test_src}
)
target_link_libraries(zombie_test PUBLIC zombie_lib)

target_link_libraries(
  zombie_test
  PUBLIC
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(zombie_test)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark zombie_lib)

# ------- CODE COVERAGE -----------------
set (MAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
include (CodeCoverage.cmake)
append_coverage_compiler_flags_to_target (zombie_test)
setup_target_for_coverage_gcovr_html(
    NAME coverage
    EXECUTABLE zombie_test)

setup_target_for_coverage_lcov(
  NAME code_coverage                             # New target name
  EXECUTABLE ctest -j ${PROCESSOR_COUNT}         # Executable in PROJECT_BINARY_DIR
  DEPENDENCIES zombie_test                       # Dependencies to build first
  EXCLUDE "/usr/*" "${CMAKE_BINARY_DIR}/_deps/*" # Patterns to exclude (can be relative
                                                 #  to BASE_DIRECTORY, with CMake 3.4+)
  NO_DEMANGLE                                    # Don't demangle C++ symbols
                                                 #  even if c++filt is found
)
