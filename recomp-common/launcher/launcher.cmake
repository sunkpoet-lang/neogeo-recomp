# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of recomp-common: https://github.com/tmcleroy/recomp-common
#
# The launcher the recomp ports share: the player's game file, optional features, controllers, Play.
# A port's launcher/CMakeLists.txt sets these and includes this file:
#
#   LAUNCHER_GAME_NAME    "My Game"                 the game's name, as the player reads it
#   LAUNCHER_GAME_EXE     mygame                    the game program (beside the launcher: mygame or mygame_app)
#   LAUNCHER_APP_ID       org.example.mygame
#   LAUNCHER_HOME_ENV     MYGAME_HOME               a variable naming a folder to use instead of the player's
#   LAUNCHER_DISCLAIMER   "Not affiliated with ..."
#   LAUNCHER_PORT_SOURCES description.cpp ...       the port's own: launcher::describe() and its file check
#
# It makes the target `launcher` (the program <LAUNCHER_GAME_EXE>-launcher), to which the port may add its
# own definitions, include folders and libraries. SDL3 is the system's or is built in; Dear ImGui and the
# Inter fonts are fetched (and Inter is compiled into the program: a binary must carry its license).
include(FetchContent)

set(RECOMP_LAUNCHER_DIR "${CMAKE_CURRENT_LIST_DIR}")
# (the settings code is in ../src in recomp-common itself and in .. in a port's copy)
if(EXISTS "${RECOMP_LAUNCHER_DIR}/../src/settings.cpp")
    set(RECOMP_COMMON_SRC "${RECOMP_LAUNCHER_DIR}/../src")
else()
    set(RECOMP_COMMON_SRC "${RECOMP_LAUNCHER_DIR}/..")
endif()

# SDL3: the system's (macOS: Homebrew), else built in statically (Linux release builds: the
# AppImage then needs nothing from the system but the usual X11/Wayland/audio libraries)
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(sdl3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz
        URL_HASH SHA256=7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(sdl3)
endif()

FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.9b.tar.gz
    URL_HASH SHA256=21d8a0a565e85dce943e375db00812c2f3f0ab21f3f0f7964e364a63422d7f99
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_Declare(inter
    URL https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
    URL_HASH SHA256=9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
FetchContent_MakeAvailable(imgui inter)

add_library(launcher_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp)
target_include_directories(launcher_imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(launcher_imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS IMGUI_DISABLE_OBSOLETE_FUNCTIONS IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS)
target_link_libraries(launcher_imgui PUBLIC SDL3::SDL3)

# The fonts, compiled in (blob("font_regular") / blob("font_semibold") for ui.cpp).
set(LAUNCHER_FONTS "${CMAKE_CURRENT_BINARY_DIR}/fonts.cpp")
add_custom_command(OUTPUT "${LAUNCHER_FONTS}"
    COMMAND "${CMAKE_COMMAND}" -DOUT=${LAUNCHER_FONTS}
            -DREGULAR=${inter_SOURCE_DIR}/extras/ttf/Inter-Regular.ttf
            -DSEMIBOLD=${inter_SOURCE_DIR}/extras/ttf/Inter-SemiBold.ttf
            -P "${RECOMP_LAUNCHER_DIR}/embed_fonts.cmake"
    DEPENDS "${RECOMP_LAUNCHER_DIR}/embed_fonts.cmake"
    VERBATIM)

add_executable(launcher
    ${RECOMP_LAUNCHER_DIR}/src/main.cpp ${RECOMP_LAUNCHER_DIR}/src/app.cpp ${RECOMP_LAUNCHER_DIR}/src/game_process.cpp
    ${RECOMP_LAUNCHER_DIR}/src/steam.cpp ${RECOMP_LAUNCHER_DIR}/src/ui.cpp ${RECOMP_COMMON_SRC}/settings.cpp
    ${LAUNCHER_PORT_SOURCES} "${LAUNCHER_FONTS}")
set_target_properties(launcher PROPERTIES OUTPUT_NAME "${LAUNCHER_GAME_EXE}-launcher")
target_include_directories(launcher PRIVATE ${RECOMP_LAUNCHER_DIR}/src ${RECOMP_COMMON_SRC})
target_compile_options(launcher PRIVATE -Wall -Wextra)
target_compile_definitions(launcher PRIVATE
    LAUNCHER_GAME_NAME="${LAUNCHER_GAME_NAME}" LAUNCHER_GAME_EXE="${LAUNCHER_GAME_EXE}" LAUNCHER_APP_ID="${LAUNCHER_APP_ID}"
    LAUNCHER_HOME_ENV="${LAUNCHER_HOME_ENV}" LAUNCHER_DISCLAIMER="${LAUNCHER_DISCLAIMER}")
target_link_libraries(launcher PRIVATE launcher_imgui)
