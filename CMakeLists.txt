cmake_minimum_required(VERSION 3.20)

project(Cerberus VERSION 1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(cerberus src/main.cpp src/CerberusCore.cpp)

target_include_directories(cerberus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(cerberus PRIVATE cxx_std_23)

target_compile_options(cerberus PRIVATE 
    -Wall 
    -Wextra 
    -Wpedantic 
    -Werror
    -O3
)
