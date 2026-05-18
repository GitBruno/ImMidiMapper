add_executable(ImMidiMapperDemo main.cpp)

target_link_libraries(ImMidiMapperDemo PRIVATE
    ImMidiMapperRtMidi
    imgui_lib
    OpenGL::GL)

# Windows: link SDL2main (entry-point shim) and suppress console in Release
if(WIN32)
    target_link_libraries(ImMidiMapperDemo PRIVATE SDL2::SDL2main)
    set_target_properties(ImMidiMapperDemo PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>)
    # Copy SDL2.dll next to the binary so the exe can find it
    add_custom_command(TARGET ImMidiMapperDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:ImMidiMapperDemo>)
endif()

# macOS: suppress OpenGL deprecation warnings (still works on all macOS)
if(APPLE)
    target_compile_definitions(ImMidiMapperDemo PRIVATE
        GL_SILENCE_DEPRECATION)
    set_target_properties(ImMidiMapperDemo PROPERTIES
        BUILD_RPATH "@executable_path")
endif()

# Output directory — put the binary next to the source for easy running
set_target_properties(ImMidiMapperDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
