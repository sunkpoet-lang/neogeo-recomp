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
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB_RECURSE REPLAY_IDENTITY_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/machine/*.cpp" "${CMAKE_SOURCE_DIR}/machine/*.h"
    "${CMAKE_SOURCE_DIR}/sound/z80.*" "${CMAKE_SOURCE_DIR}/video/neogeo_video.*"
    "${CMAKE_SOURCE_DIR}/third_party/ymfm*" "${CMAKE_SOURCE_DIR}/runtime/no*_blocks.cpp"
    "${CMAKE_SOURCE_DIR}/runtime/state.h" "${CMAKE_SOURCE_DIR}/runtime/training_recordings.h" "${CMAKE_SOURCE_DIR}/runtime/neogeo_game.*" "${CMAKE_SOURCE_DIR}/runtime/game.*" "${CMAKE_SOURCE_DIR}/runtime/no_game.cpp"
    "${GAME_DIR}/src/*.cpp" "${GAME_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/port/neogeo_port.*" "${CMAKE_SOURCE_DIR}/port/neogeo_replay.*" "${CMAKE_SOURCE_DIR}/port/replay_round_clock.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${REPLAY_IDENTITY_SOURCES}
    "${GAME_DIR}/game.cfg" "${CMAKE_SOURCE_DIR}/replay_identity.txt" "${CMAKE_SOURCE_DIR}/tools/replay_identity.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/replay_identity.py"
    --root "${CMAKE_SOURCE_DIR}" --game "${GAME}" --abi "${CMAKE_SYSTEM_NAME}-${CMAKE_SIZEOF_VOID_P}" --output "${WORK_DIR}/gen/replay_identity.h"
    COMMAND_ERROR_IS_FATAL ANY)

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
# (ForkAde) the spectators' state at a pace a relay carries (tools/patch_gekkonet_neogeo.cmake); idempotent
execute_process(COMMAND "${CMAKE_COMMAND}" -DSOURCE=${gekkonet_SOURCE_DIR} -P "${CMAKE_SOURCE_DIR}/tools/patch_gekkonet_neogeo.cmake"
    RESULT_VARIABLE gekkonet_patched)
if(NOT gekkonet_patched EQUAL 0)
    message(FATAL_ERROR "GekkoNet: the Neo Geo spectator patch did not apply")
endif()

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
    port/hud.cpp recomp-common/menu.cpp recomp-common/menubar.cpp recomp-common/settings.cpp runtime/game.cpp runtime/neogeo_game.cpp
    machine/neogeo.cpp machine/neogeo_decrypt.cpp machine/neogeo_sound.cpp video/neogeo_video.cpp machine/m68k.cpp sound/z80.cpp machine/romset.cpp
    machine/kabuki.cpp machine/cps2crypt.cpp runtime/no_blocks.cpp runtime/no_z80_blocks.cpp ${YMFM_OPN}
    machine/soundtrack.cpp port/soundtrack.cpp port/decoders.cpp port/neogeo_online.cpp port/neogeo_chat.cpp recomp-common/chat.cpp
    ${NEOGEO_GAME_SOURCES})
target_sources(${GAME} PRIVATE port/neogeo_replay.cpp port/fareplay.cpp port/neogeo_input_display.cpp recomp-common/replay_viewer.cpp)
target_include_directories(${GAME} PRIVATE runtime port machine "${WORK_DIR}/gen" "${GAME_DIR}")
# (the version both players of an online session must have: VERSION's)
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" NEOGEO_RECOMP_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")
target_compile_definitions(${GAME} PRIVATE NEOGEO_RECOMP_VERSION="${NEOGEO_RECOMP_VERSION}")
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

option(NEOGEO_BUILD_TESTS "Build synthetic decryption and board tests" OFF)
if (NEOGEO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

# ForkAde product integration is explicit and limited to verified KOF sets.
set(FORKADE_BROKER_DIR "" CACHE PATH "Optional ForkAde native room owner sources")
if(FORKADE_BROKER_DIR)
    if(NOT WIN32 OR NOT GAME MATCHES "^(kof97|kof2002|kf2k2mp2|kof98)$")
        message(FATAL_ERROR "This ForkAde native room slice requires a verified Windows KOF set")
    endif()
    target_compile_definitions(${GAME} PRIVATE FORKADE_GAME_BROKER)
    target_include_directories(${GAME} PRIVATE "${FORKADE_BROKER_DIR}/native" "${FORKADE_BROKER_DIR}/launcher" "${CMAKE_SOURCE_DIR}/recomp-common/launcher/src")
    target_link_libraries(${GAME} PRIVATE bcrypt)
    target_sources(${GAME} PRIVATE recomp-common/launcher/src/run_program.cpp)
endif()
