cmake_minimum_required(VERSION 3.24)
project(flatline CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Neither is on by default for this Clang-on-Windows toolchain: without -g, object
# files carry no line tables; without /DEBUG, lld-link won't (re)generate a PDB on
# relink even when one is requested via /pdb:, silently leaving a stale PDB in place
# from whenever one last happened to exist. Both are needed for breakpoints to bind.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g)
    add_link_options(-Xlinker /DEBUG)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(FLATLINE_JSON_FIXTURES
    ${CMAKE_SOURCE_DIR}/res/room_guardroom.json
    ${CMAKE_SOURCE_DIR}/res/workorder_bribe.json
    ${CMAKE_SOURCE_DIR}/res/workorder_hallucinated.json
)

function(flatline_add_executable name)
    add_executable(${name} ${ARGN})
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party
    )
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FLATLINE_JSON_FIXTURES} $<TARGET_FILE_DIR:${name}>
    )
endfunction()

flatline_add_executable(test_gauntlet src/test_gauntlet.cpp)
flatline_add_executable(test_log src/test_log.cpp)
flatline_add_executable(test_applier src/test_applier.cpp)
flatline_add_executable(test_roll src/test_roll.cpp)
flatline_add_executable(test_turn src/test_turn.cpp)
flatline_add_executable(woz src/wizard_of_oz_demo.cpp)
flatline_add_executable(woz_loop src/woz_loop.cpp)

# SDL3: static link, no runtime DLL to copy alongside woz_gui.exe
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/SDL3)

find_package(OpenGL REQUIRED)

add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_sdl3.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    third_party/imgui
    third_party/imgui/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3 OpenGL::GL)

flatline_add_executable(woz_gui src/woz_gui.cpp)
target_link_libraries(woz_gui PRIVATE imgui)
