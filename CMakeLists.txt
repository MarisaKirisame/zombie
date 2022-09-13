cmake_minimum_required(VERSION 3.10)

project(Zombie)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
  zombie_test
  zombie_test.cc
)

target_compile_features(zombie_test PRIVATE cxx_std_17)

target_link_libraries(
  zombie_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(zombie_test)
