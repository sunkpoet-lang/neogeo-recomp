# SPDX-License-Identifier: GPL-3.0-only
# (ForkAde) GekkoNet v20261024222910-9da2363 for the Neo Geo games: a spectator joining a match gets the confirmed
# state in 900-byte chunks. GekkoNet sends every chunk of it at once, again every 200 ms, until the spectator
# acknowledges the whole state; a Neo Geo state is some 280 KB (about 300 datagrams), and through the tunnel's relay
# (n0's) that burst is lost every time, so the spectator never gets a frame. Here the host sends the chunks at a pace
# a relay carries: 10 every 16 ms (about 560 KB/s), going round them until the state is acknowledged, and keeps the
# spectators' inputs for a minute meanwhile (10 s before). The packets and the spectator's side are GekkoNet's own:
# an unpatched spectator takes the paced chunks the same way. (sf3comp and MvC2 pace theirs the same way.)
#   cmake -DSOURCE=<gekkonet source dir> -P patch_gekkonet_neogeo.cmake
if(NOT SOURCE)
    message(FATAL_ERROR "SOURCE: GekkoNet's source folder")
endif()
set(backend_cpp "${SOURCE}/GekkoLib/src/backend.cpp")
set(backend_h "${SOURCE}/GekkoLib/include/private/backend.h")
file(READ "${backend_cpp}" cpp)
if(cpp MATCHES "ForkAde: Neo Geo spectator pace")
    message(STATUS "GekkoNet already paced for Neo Geo spectators")
    return()
endif()
file(READ "${backend_h}" hdr)

function(swap var old new)
    string(FIND "${${var}}" "${old}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "patch_gekkonet_neogeo: not found in GekkoNet: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" result "${${var}}")
    set(${var} "${result}" PARENT_SCOPE)
endfunction()

swap(hdr "        u64 last_spectator_state_send_time = 0;"
"        u64 last_spectator_state_send_time = 0;
        u32 forkade_state_cursor = 0; // (ForkAde: Neo Geo spectator pace) the next chunk to send")

swap(hdr "        const u32 MAX_SPECTATOR_INPUT_QUEUE_SIZE = 600;"
"        const u32 MAX_SPECTATOR_INPUT_QUEUE_SIZE = 3600; // (ForkAde: Neo Geo spectator pace) a minute")

swap(cpp "        if (player->last_spectator_state_send_time + NetStats::INPUT_RETRY_INTERVAL > now) {
            continue;
        }

        const u32 total_size = (u32)player->spectator_state.size();
        for (u32 offset = 0; offset < total_size; offset += SPECTATOR_STATE_CHUNK_SIZE) {"
"        // (ForkAde: Neo Geo spectator pace) 10 chunks every 16 ms, round the state until it is acknowledged
        constexpr u64 kPaceMs = 16;
        constexpr u32 kChunksPerSend = 10;
        if (player->last_spectator_state_send_time + kPaceMs > now) {
            continue;
        }

        const u32 total_size = (u32)player->spectator_state.size();
        const u32 chunk_count = (total_size + SPECTATOR_STATE_CHUNK_SIZE - 1) / SPECTATOR_STATE_CHUNK_SIZE;
        for (u32 sent = 0; sent < kChunksPerSend && sent < chunk_count; sent++) {
            const u32 offset = (player->forkade_state_cursor++ % chunk_count) * SPECTATOR_STATE_CHUNK_SIZE;")

file(WRITE "${backend_h}" "${hdr}")
file(WRITE "${backend_cpp}" "${cpp}")
message(STATUS "GekkoNet paced for Neo Geo spectators")
