# SPDX-License-Identifier: GPL-3.0-only
set(ROOT "${CMAKE_SOURCE_DIR}")
add_executable(neogeo_decrypt_test neogeo_decrypt_test.cpp
    ${ROOT}/machine/neogeo.cpp ${ROOT}/machine/neogeo_sound.cpp ${ROOT}/machine/neogeo_decrypt.cpp
    ${ROOT}/machine/romset.cpp ${ROOT}/machine/kabuki.cpp ${ROOT}/machine/cps2crypt.cpp
    ${ROOT}/machine/m68k.cpp ${ROOT}/sound/z80.cpp ${ROOT}/video/neogeo_video.cpp
    ${ROOT}/runtime/no_blocks.cpp ${ROOT}/runtime/no_z80_blocks.cpp
    ${ROOT}/third_party/ymfm_opn.cpp ${ROOT}/third_party/ymfm_ssg.cpp ${ROOT}/third_party/ymfm_adpcm.cpp)
target_include_directories(neogeo_decrypt_test PRIVATE ${ROOT}/machine ${ROOT}/runtime)
target_link_libraries(neogeo_decrypt_test PRIVATE ZLIB::ZLIB)
target_compile_options(neogeo_decrypt_test PRIVATE -Wall -Wextra -Wno-unused-parameter)
add_test(NAME neogeo_decrypt COMMAND neogeo_decrypt_test "${CMAKE_CURRENT_BINARY_DIR}/synthetic-roms")
