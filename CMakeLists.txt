cmake_minimum_required(VERSION 3.10)

project(Zombie)

add_executable(Zombie main.cpp)

target_compile_features(Zombie PRIVATE cxx_std_17)
