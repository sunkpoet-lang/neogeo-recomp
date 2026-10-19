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

# (macOS: 3.4.16 or later. 3.4.14's Metal GPU device takes a frame's fence still being worked on for one signalled, so
# after the first frames every picture is skipped and the window stays black while the game runs and sounds)
if (APPLE)
    find_package(SDL3 3.4.16 REQUIRED CONFIG)
else ()
    find_package(SDL3 REQUIRED CONFIG)
endif ()
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
    machine/m68k.cpp machine/machine.cpp machine/sound_board.cpp machine/soundtrack.cpp machine/eeprom.cpp machine/romset.cpp machine/cps2crypt.cpp machine/kabuki.cpp
    runtime/recomp.cpp runtime/recomp_z80.cpp
)
# The game's own sources (features, routines replaced by hand-written C++); without any, nothing is added
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
# ... and those of the games it shares sources with (game.cfg's "sources": another game made from the same program,
# whose sources name its places as At(0x...); the game's own family.h, in its folder, says where they are in it)
file(STRINGS "${GAME_DIR}/game.cfg" GAME_SHARES REGEX "^sources[ \t]*=")
if (GAME_SHARES)
    string(REGEX REPLACE "^sources[ \t]*=[ \t]*" "" GAME_SHARES "${GAME_SHARES}")
    separate_arguments(GAME_SHARES)
    foreach (other ${GAME_SHARES})
        file(GLOB shared CONFIGURE_DEPENDS "${GAME_DIR}/../${other}/src/*.cpp")
        list(APPEND GAME_SOURCES ${shared})
        list(APPEND GAME_INCLUDES "${GAME_DIR}/../${other}/src") # (for the game's own sources, which use them)
        message(STATUS "cps2recomp: ${GAME}: the sources of ${other} too")
    endforeach ()
endif ()
if (NOT GAME_SOURCES)
    set(GAME_SOURCES runtime/no_game.cpp)
endif ()
# (recomp-common/: the code the recomp ports share, a copy of recomp-common's; tools/sync-common.sh)
set(PORT_SOURCES port/main.cpp port/frontend_sdl.cpp port/present.cpp port/menu.cpp port/hud.cpp port/soundtrack.cpp port/decoders.cpp recomp-common/menu.cpp recomp-common/settings.cpp runtime/game.cpp runtime/board.cpp runtime/spare.cpp fight/frames.cpp fight/training.cpp fight/colours.cpp fight/colour_editor.cpp fight/battle.cpp fight/alternative.cpp fight/session.cpp beat/modes.cpp beat/model.cpp model/model.cpp ${GAME_SOURCES})

# (the sound files' decoders, third_party/, are other projects' code, compiled as they are)
set_source_files_properties(port/decoders.cpp PROPERTIES COMPILE_OPTIONS "-w")

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
    target_include_directories(${target} PRIVATE runtime port machine fight beat model "${WORK_DIR}/gen" "${GAME_DIR}" ${GAME_INCLUDES})
    target_link_libraries(${target} PRIVATE SDL3::SDL3 ZLIB::ZLIB)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-label -Wno-unused-variable -Wno-parentheses-equality)
endforeach ()
