cmake_minimum_required(VERSION 3.10)

project(Zombie)

file(GLOB ALL_SRCS "src/*.cpp")
add_library(ZombieLib ${ALL_SRCS})
target_include_directories(ZombieLib PUBLIC include)
target_compile_features(ZombieLib PUBLIC cxx_std_17)

include_directories(../libassert/include)
link_directories(../libassert/bin)
link_libraries(assert)

add_executable(debug debug.cpp)
target_link_libraries(debug ZombieLib)

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
target_link_libraries(zombie_test ZombieLib)

target_link_libraries(
  zombie_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(zombie_test)
