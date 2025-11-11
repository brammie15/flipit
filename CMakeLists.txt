cmake_minimum_required(VERSION 3.16)
project(flipit LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
include(FetchContent)

FetchContent_Declare(
        Heatshrink
        GIT_REPOSITORY https://github.com/atomicobject/heatshrink.git
)
FetchContent_MakeAvailable(Heatshrink)

add_library(heatshrink STATIC
        ${heatshrink_SOURCE_DIR}/heatshrink_decoder.c
        ${heatshrink_SOURCE_DIR}/heatshrink_encoder.c
)
target_include_directories(heatshrink PUBLIC ${heatshrink_SOURCE_DIR})

add_subdirectory(src)
add_subdirectory(test)
