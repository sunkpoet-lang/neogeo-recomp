# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy and the neogeo-recomp contributors. Part of neogeo-recomp, built on cps2recomp.
#
# The neogeo-recomp launcher: the player's ROM set, optional features, controllers; starts the game. Built
# beside the game program (it runs <id> from its own folder). Needs SDL3.
# The launcher itself is the one the recomp ports share (../recomp-common/launcher, a copy of recomp-common's;
# never changed here). What is this toolkit's is src/description.cpp: what it tells that launcher, and the
# check of the ROM set. The game comes from these (tools/collection.sh passes them from games/<id>/game.cfg):
#   GAME_NAME GAME_EXE APP_ID HOME_ENV DISCLAIMER ROM_SET ROM_PARENT ROM_NAME and the set's file lists
#   ROM_PROGRAM ROM_GRAPHICS ROM_SOUND_PROGRAM ROM_SAMPLES ROM_KEY
# and, if the game has features whose switch belongs on the first page with "Full screen" (game.cfg, display):
#   DISPLAY_SWITCHES  name|Title|What it says under it, and more of them with || between
# and, for a game of more than two players or with buttons of its own (game.cfg, players and buttons):
#   PLAYERS  BUTTONS
# and, for a game on another board than a CPS-2 (game.cfg, board and cps_b; machine/romset.h):  BOARD  CPS_B
# The collection's launcher, for every game (tools/collection.sh): COLLECTION=ON and GAMES (their ids; the table of
# them, games.h, is written from their game.cfg files); GAME_NAME and the rest then name the collection. It does not
# update itself yet (recomp-common/launcher/src/update.h): this repository has no releases to update from.
cmake_minimum_required(VERSION 3.20)
project(neogeo_recomp_launcher CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(LAUNCHER_GAME_NAME "${GAME_NAME}")
set(LAUNCHER_GAME_EXE "${GAME_EXE}")
set(LAUNCHER_APP_ID "${APP_ID}")
set(LAUNCHER_HOME_ENV "${HOME_ENV}")
set(LAUNCHER_DISCLAIMER "${DISCLAIMER}")
set(LAUNCHER_PORT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/description.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../machine/romset.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../machine/cps2crypt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../machine/kabuki.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../machine/neogeo_decrypt.cpp)
set(LAUNCHER_COLLECTION ${COLLECTION})
if(COLLECTION)
    # (no LAUNCHER_UPDATE_REPO: no updates until this repository publishes releases with their manifests; VERSION's
    # version, or COLLECTION_VERSION)
    if(COLLECTION_VERSION)
        set(LAUNCHER_VERSION ${COLLECTION_VERSION})
    else()
        file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION LAUNCHER_VERSION LIMIT_COUNT 1)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION)
    endif()
endif()
include(../recomp-common/launcher/launcher.cmake)
# A windowed program on Windows, as the games and the bridges are: a double click opens no console window
# beside it. Its messages still reach a file or a pipe it is started with (the tests'). Set here, not in recomp-common.
if(WIN32)
    set_target_properties(launcher PROPERTIES WIN32_EXECUTABLE ON)
endif()
if(COLLECTION)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(GAMES_HEADER "${CMAKE_CURRENT_BINARY_DIR}/collection/games.h")
    execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../tools/gamecfg.py --collection ${GAMES_HEADER} ${GAMES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/.. RESULT_VARIABLE written)
    if(NOT written EQUAL 0)
        message(FATAL_ERROR "the table of games was not written")
    endif()
    target_include_directories(launcher PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/collection")
endif()

target_compile_definitions(launcher PRIVATE
    LAUNCHER_ROM_SET="${ROM_SET}" LAUNCHER_ROM_PARENT="${ROM_PARENT}" LAUNCHER_ROM_NAME="${ROM_NAME}"
    LAUNCHER_ROM_PROGRAM="${ROM_PROGRAM}" LAUNCHER_ROM_GRAPHICS="${ROM_GRAPHICS}"
    LAUNCHER_ROM_SOUND_PROGRAM="${ROM_SOUND_PROGRAM}" LAUNCHER_ROM_SAMPLES="${ROM_SAMPLES}" LAUNCHER_ROM_KEY="${ROM_KEY}"
    LAUNCHER_DISPLAY_SWITCHES="${DISPLAY_SWITCHES}" LAUNCHER_PLAYERS="${PLAYERS}" LAUNCHER_BUTTONS="${BUTTONS}"
    LAUNCHER_BOARD="${BOARD}" LAUNCHER_CPS_B="${CPS_B}")
find_package(ZLIB REQUIRED)
target_link_libraries(launcher PRIVATE ZLIB::ZLIB)
