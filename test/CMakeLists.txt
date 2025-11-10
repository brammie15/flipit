add_executable(flipit main.cpp)

target_link_libraries(flipit PRIVATE flipit_lib)

# Copy assets to build directory
add_custom_command(TARGET flipit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:flipit>/assets
)