cmake_minimum_required(VERSION 3.24)
project(space_shipper C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(FR_BUILD_GAME "Build the SDL3/OpenGL game executable" ON)
option(FR_BUILD_TESTS "Build unit tests and scenario runner" ON)

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# Warnings are errors only for first-party code.
set(FR_STRICT_OPTS "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Werror>")

# ---------------------------------------------------------------- libraries

# Deterministic world simulation (no platform dependencies).
add_library(freighter_sim STATIC
    src/sim/rng.c
    src/sim/level.c
    src/sim/shipgen.c
    src/sim/world.c
    src/sim/save.c
    third_party/cjson/cJSON.c
)
target_include_directories(freighter_sim PUBLIC src/sim third_party/cjson)
target_compile_options(freighter_sim PRIVATE ${FR_STRICT_OPTS})
set_source_files_properties(third_party/cjson/cJSON.c PROPERTIES COMPILE_OPTIONS "-w")

# ASCII renderer into a character grid (no platform dependencies).
add_library(freighter_render STATIC
    src/render/grid.c
    src/render/raster3d.c
)
target_include_directories(freighter_render PUBLIC src/render)
target_compile_options(freighter_render PRIVATE ${FR_STRICT_OPTS} -O2)
target_link_libraries(freighter_render PUBLIC freighter_sim)
if(UNIX AND NOT APPLE)
    target_link_libraries(freighter_render PUBLIC m)
endif()

# Game logic: states, input handling, screen composition.
add_library(freighter_core STATIC
    src/game/game.c
)
target_include_directories(freighter_core PUBLIC src/game)
target_compile_options(freighter_core PRIVATE ${FR_STRICT_OPTS})
target_link_libraries(freighter_core PUBLIC freighter_sim freighter_render)

# ------------------------------------------------------------------- game

if(FR_BUILD_GAME)
    include(FetchContent)
    set(SDL3_TARBALL "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL3-3.2.20.tar.gz")
    if(EXISTS "${SDL3_TARBALL}")
        set(SDL3_URL "${SDL3_TARBALL}")
        message(STATUS "Using vendored SDL3 tarball")
    else()
        set(SDL3_URL
            "https://github.com/libsdl-org/SDL/releases/download/release-3.2.20/SDL3-3.2.20.tar.gz")
        message(STATUS "SDL3 will be fetched from upstream")
    endif()
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(SDL3 URL ${SDL3_URL})
    FetchContent_MakeAvailable(SDL3)

    add_executable(space_shipper
        src/platform/sdl_app.c
        src/platform/font.c
        src/platform/font8x8_basic.h
    )
    target_compile_options(space_shipper PRIVATE ${FR_STRICT_OPTS})
    target_link_libraries(space_shipper PRIVATE freighter_core SDL3::SDL3)
    if(NOT EMSCRIPTEN)
        find_package(OpenGL REQUIRED)
        target_link_libraries(space_shipper PRIVATE OpenGL::GL)
    else()
        set_target_properties(space_shipper PROPERTIES SUFFIX ".html")
        target_link_options(space_shipper PRIVATE
            "-sALLOW_MEMORY_GROWTH=1" "-sMAX_WEBGL_VERSION=2" "-sMIN_WEBGL_VERSION=2")
    endif()
endif()

# ------------------------------------------------------------------ tests

if(FR_BUILD_TESTS)
    enable_testing()

    add_library(unity STATIC third_party/unity/src/unity.c)
    target_include_directories(unity PUBLIC third_party/unity/src)
    set_source_files_properties(third_party/unity/src/unity.c PROPERTIES COMPILE_OPTIONS "-w")

    foreach(t rng shipgen world save render)
        add_executable(test_${t} tests/unit/test_${t}.c)
        target_compile_options(test_${t} PRIVATE ${FR_STRICT_OPTS})
        target_link_libraries(test_${t} PRIVATE unity freighter_sim freighter_render)
        add_test(NAME unit_${t} COMMAND test_${t})
    endforeach()

    add_executable(scenario_runner tools/scenario_runner.c)
    target_compile_options(scenario_runner PRIVATE ${FR_STRICT_OPTS})
    target_link_libraries(scenario_runner PRIVATE freighter_core)

    # Automated game tests: scripted full gameplay scenarios.
    foreach(sc scenario_full_loop scenario_travel scenario_save_load
             scenario_docking scenario_shipgen_explore)
        add_test(NAME ${sc}
            COMMAND scenario_runner ${CMAKE_CURRENT_SOURCE_DIR}/tests/scenarios/${sc}.txt
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endforeach()

    # Golden-frame tests (ASCII framebuffer snapshots).
    foreach(gf golden_walk_bridge golden_docking golden_menu)
        add_test(NAME ${gf}
            COMMAND scenario_runner ${CMAKE_CURRENT_SOURCE_DIR}/tests/scenarios/${gf}.txt
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endforeach()
endif()
