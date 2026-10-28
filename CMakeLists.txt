# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy and the neogeo-recomp contributors. Part of neogeo-recomp, built on cps2recomp.
#
# One Neo Geo game (games/<id>/, game.cfg's board = neogeo): the board (machine/neogeo*.cpp, video/neogeo_video.cpp)
# and its port (port/neogeo_*.cpp), with the pieces the recomp ports share around it (the menu, the settings, the
# presenter, the HUD, the game's sources' registry), in a window or with none, in the 68000's interpreter. The Makefile
# runs this: make GAME=<id>.

cmake_minimum_required(VERSION 3.28)
project(neogeo_recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

# Game selection: the game id names a directory under games/; work/<id>/gen holds what is made from its game.cfg
set(GAME "kof97" CACHE STRING "Game id (directory under games/)")
set(GAME_DIR "${CMAKE_SOURCE_DIR}/games/${GAME}" CACHE PATH "The game's directory (game.cfg, src/)")
set(WORK_DIR "${CMAKE_SOURCE_DIR}/work/${GAME}" CACHE PATH "Generated code directory")

# (macOS: 3.4.16 or later. 3.4.14's Metal GPU device takes a frame's fence still being worked on for one signalled, so
# after the first frames every picture is skipped and the window stays black while the game runs and sounds)
if (APPLE)
    find_package(SDL3 3.4.16 REQUIRED CONFIG)
else ()
    find_package(SDL3 REQUIRED CONFIG)
endif ()
find_package(ZLIB REQUIRED)

# GekkoNet (BSD-2-Clause): online play's rollback (port/neogeo_online.h), its library only, static, without asio (the
# port has its own UDP adapter). A newer release is a deliberate update.
include(FetchContent)
set(NO_ASIO_BUILD ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(gekkonet
    URL https://github.com/HeatXD/GekkoNet/archive/refs/tags/v20261024222910-9da2363.tar.gz
    URL_HASH SHA256=a9001dec8698120fe45b314527d181c403ee92e3a30a03d765d3e792b8c74e1c
    SOURCE_SUBDIR GekkoLib
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(gekkonet)

file(STRINGS "${GAME_DIR}/game.cfg" GAME_BOARD REGEX "^board[ \t]*=")
if (NOT GAME_BOARD MATCHES "neogeo")
    message(FATAL_ERROR "${GAME}: game.cfg does not say board = neogeo")
endif ()

# ymfm's YM2610 and the sound files' decoders (third_party/, unchanged: other projects' code, compiled as it is)
set(YMFM_OPN third_party/ymfm_opn.cpp third_party/ymfm_ssg.cpp third_party/ymfm_adpcm.cpp)
set_source_files_properties(${YMFM_OPN} port/decoders.cpp PROPERTIES COMPILE_OPTIONS "-w")
# The game's own sources (games/<id>/src: runtime/neogeo_game.h); without any, nothing is added
file(GLOB NEOGEO_GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
if (NOT NEOGEO_GAME_SOURCES)
    set(NEOGEO_GAME_SOURCES runtime/no_game.cpp)
endif ()
add_executable(${GAME} port/neogeo_main.cpp port/neogeo_port.cpp port/neogeo_window.cpp port/present.cpp port/menu.cpp
    port/hud.cpp recomp-common/menu.cpp recomp-common/settings.cpp runtime/game.cpp runtime/neogeo_game.cpp
    machine/neogeo.cpp machine/neogeo_sound.cpp video/neogeo_video.cpp machine/m68k.cpp sound/z80.cpp machine/romset.cpp
    machine/kabuki.cpp machine/cps2crypt.cpp runtime/no_blocks.cpp runtime/no_z80_blocks.cpp ${YMFM_OPN}
    machine/soundtrack.cpp port/soundtrack.cpp port/decoders.cpp
    ${NEOGEO_GAME_SOURCES})
target_include_directories(${GAME} PRIVATE runtime port machine "${WORK_DIR}/gen" "${GAME_DIR}")
target_link_libraries(${GAME} PRIVATE SDL3::SDL3 ZLIB::ZLIB GekkoNet)
if (WIN32)
    target_link_libraries(${GAME} PRIVATE ws2_32)
endif ()
target_compile_options(${GAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
# (on Windows a windowed program: no console window opens beside it; with the game's icon, games/<id>/icon.ico)
if (WIN32)
    set_target_properties(${GAME} PROPERTIES WIN32_EXECUTABLE ON)
    get_filename_component(GAME_ICON "${GAME_DIR}/icon.ico" ABSOLUTE)
    if (EXISTS "${GAME_ICON}")
        enable_language(RC)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc" "1 ICON \"${GAME_ICON}\"\n")
        target_sources(${GAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc")
    endif ()
endif ()
