cmake_minimum_required(VERSION 3.24)
project(flatline CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(FLATLINE_JSON_FIXTURES
    ${CMAKE_SOURCE_DIR}/src/room_guardroom.json
    ${CMAKE_SOURCE_DIR}/src/workorder_bribe.json
    ${CMAKE_SOURCE_DIR}/src/workorder_hallucinated.json
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
