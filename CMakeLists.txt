cmake_minimum_required(VERSION 3.12)
project(WindowCC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(window_cc
    src/u_baseline.cpp
    src/tsf_index.cpp
    src/msf_index.cpp
    src/main.cpp
)

if(MSVC)
    target_compile_options(window_cc PRIVATE /O2 /EHsc)
else()
    target_compile_options(window_cc PRIVATE -O3 -Wall -Wextra)
endif()
