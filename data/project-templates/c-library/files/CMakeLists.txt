cmake_minimum_required(VERSION 3.16)
project(${project_ident} C)

add_library(${project_ident} src/${project_ident}.c)
target_include_directories(${project_ident} PUBLIC include)

add_executable(test_${project_ident} tests/test_${project_ident}.c)
target_link_libraries(test_${project_ident} PRIVATE ${project_ident})
