cmake_minimum_required(VERSION 3.23)

project(process LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

add_library(process INTERFACE)

target_compile_features(process INTERFACE cxx_std_17)
target_precompile_headers(process INTERFACE pch.hpp)

set(LIB_TARGETS process PARENT_SCOPE)