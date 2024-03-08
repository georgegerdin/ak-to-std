cmake_minimum_required(VERSION 3.2)

project(ak-to-std)

add_subdirectory(extlibs)
add_subdirectory(src)


file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/project_source_dir.txt" "${PROJECT_SOURCE_DIR}")