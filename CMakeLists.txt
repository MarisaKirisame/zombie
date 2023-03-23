cmake_minimum_required(VERSION 3.10)

project(zombie)

file(GLOB ALL_SRCS "src/*.cpp")
add_library(zombie_lib ${ALL_SRCS})
target_include_directories(zombie_lib PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

target_compile_features(zombie_lib PUBLIC cxx_std_17)

install(TARGETS zombie_lib EXPORT zombie)
install(EXPORT zombie DESTINATION lib/${CMAKE_LIBRARY_ARCHITECTURE}/cmake/zombie)

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
