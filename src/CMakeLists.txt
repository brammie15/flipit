add_library(flipit_lib
        bm_utils.cpp bm_utils.h
)

target_include_directories(flipit_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(flipit_lib PUBLIC heatshrink)
