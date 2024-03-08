set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")

add_executable(ak-to-std
    main.cc
        local_filedb.h)

target_link_libraries(ak-to-std PUBLIC code-comprehension)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/project_source_dir.txt" "${PROJECT_SOURCE_DIR}")