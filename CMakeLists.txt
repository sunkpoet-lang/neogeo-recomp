# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.

cmake_minimum_required(VERSION 3.28)
project(cps2recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# Game selection: the game id names a directory under games/ and the generated code under work/
set(GAME "vampire_savior" CACHE STRING "Game id (directory under games/)")
set(GAME_DIR "${CMAKE_SOURCE_DIR}/../games/${GAME}" CACHE PATH "The game's directory (game.cfg, src/)")
set(WORK_DIR "${CMAKE_SOURCE_DIR}/../work/${GAME}" CACHE PATH "Generated code directory")

find_package(SDL3 REQUIRED CONFIG)
find_package(ZLIB REQUIRED)

# Translated game code: work/<game>/gen/native_*.cpp, written by tools/recomp.py.
# Without it the game still builds and runs everything in the interpreter.
file(GLOB GEN_SOURCES CONFIGURE_DEPENDS "${WORK_DIR}/gen/native_*.cpp")
if (GEN_SOURCES)
    message(STATUS "cps2recomp: ${GAME}: translated code found in ${WORK_DIR}/gen")
else ()
    message(STATUS "cps2recomp: ${GAME}: no translated code yet (interpreter only)")
    set(GEN_SOURCES runtime/no_blocks.cpp)
endif ()

# The sound driver translated: work/<game>/gen/z80_*.cpp, written by tools/recompz80.py
file(GLOB GEN_Z80_SOURCES CONFIGURE_DEPENDS "${WORK_DIR}/gen/z80_*.cpp")
if (NOT GEN_Z80_SOURCES)
    set(GEN_Z80_SOURCES runtime/no_z80_blocks.cpp)
endif ()

set(MACHINE_SOURCES
    video/video.cpp
    sound/z80.cpp sound/qsound.cpp sound/driver.cpp
    machine/m68k.cpp machine/machine.cpp machine/eeprom.cpp machine/romset.cpp machine/cps2crypt.cpp
    runtime/recomp.cpp runtime/recomp_z80.cpp
)
# The game's own sources (features, routines replaced by hand-written C++); without any, nothing is added
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
if (NOT GAME_SOURCES)
    set(GAME_SOURCES runtime/no_game.cpp)
endif ()
# (recomp-common/: the code the recomp ports share, a copy of recomp-common's; tools/sync-common.sh)
set(PORT_SOURCES port/main.cpp port/frontend_sdl.cpp port/present.cpp port/menu.cpp port/hud.cpp recomp-common/menu.cpp recomp-common/settings.cpp runtime/game.cpp runtime/board.cpp runtime/spare.cpp fight/frames.cpp fight/training.cpp fight/colours.cpp fight/colour_editor.cpp ${GAME_SOURCES})

# The game, on cps2recomp's own machine (machine/). No emulator is compiled or linked into it.
add_executable(${GAME} ${PORT_SOURCES} ${MACHINE_SOURCES} ${GEN_SOURCES} ${GEN_Z80_SOURCES})
# The check build ('make check-blocks'): the same, with every translated block run beside the interpreter
# and compared (runtime/recomp.cpp)
add_executable(${GAME}-check EXCLUDE_FROM_ALL ${PORT_SOURCES} ${MACHINE_SOURCES} ${GEN_SOURCES} ${GEN_Z80_SOURCES})
target_compile_definitions(${GAME}-check PRIVATE CPS2RECOMP_CHECK)
# (on Windows the game is a windowed program: no console window opens beside it, whether the launcher starts it
# or a double click does)
if (WIN32)
    set_target_properties(${GAME} PROPERTIES WIN32_EXECUTABLE ON)
endif ()
foreach (target ${GAME} ${GAME}-check)
    target_include_directories(${target} PRIVATE runtime port machine fight "${WORK_DIR}/gen")
    target_link_libraries(${target} PRIVATE SDL3::SDL3 ZLIB::ZLIB)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-label -Wno-unused-variable -Wno-parentheses-equality)
endforeach ()
