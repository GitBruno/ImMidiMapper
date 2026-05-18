add_executable(ImMidiMapperDemo main.cpp)

target_link_libraries(ImMidiMapperDemo PRIVATE
    ImMidiMapperRtMidi
    imgui_lib
    OpenGL::GL)

# macOS: suppress OpenGL deprecation warnings (it still works on all macOS)
if(APPLE)
    target_compile_definitions(ImMidiMapperDemo PRIVATE
        GL_SILENCE_DEPRECATION)
    # Ensure the app can find its own resources when run from Finder
    set_target_properties(ImMidiMapperDemo PROPERTIES
        BUILD_RPATH "@executable_path")
endif()

# Windows: no console window in Release; keep it in Debug for printf output
if(WIN32)
    set_target_properties(ImMidiMapperDemo PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>)
endif()

# Output directory — put the binary next to the source for easy running
set_target_properties(ImMidiMapperDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
