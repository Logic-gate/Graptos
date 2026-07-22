cmake_minimum_required(VERSION 3.16)
project(${project_ident} C)

add_executable(${project_slug} src/main.c)
target_include_directories(${project_slug} PRIVATE include)
