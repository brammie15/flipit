add_executable(flipit_test main.cpp)

target_link_libraries(flipit_test PRIVATE flipit_lib)

# Copy assets to build directory
add_custom_command(TARGET flipit_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:flipit_test>/assets
)
