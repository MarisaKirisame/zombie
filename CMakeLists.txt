cmake_minimum_required(VERSION 3.10)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(zombie)

file(GLOB_RECURSE ALL_SRCS "src/*.cpp")
add_library(zombie_lib ${ALL_SRCS})
target_include_directories(zombie_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

target_compile_features(zombie_lib PUBLIC cxx_std_17)

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
target_link_libraries(zombie_test zombie_lib)

target_link_libraries(
  zombie_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(zombie_test)
