cmake_minimum_required(VERSION 3.20)
project(${project_ident} VERSION 0.1.0 LANGUAGES CXX)

add_executable(${executable_name} src/main.cpp)
target_include_directories(${executable_name} PRIVATE include)
target_compile_features(${executable_name} PRIVATE cxx_std_20)

enable_testing()
add_executable(test_${executable_name} tests/test_main.cpp)
target_include_directories(test_${executable_name} PRIVATE include)
target_compile_features(test_${executable_name} PRIVATE cxx_std_20)
add_test(NAME starter COMMAND test_${executable_name})
