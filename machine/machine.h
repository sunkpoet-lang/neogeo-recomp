// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps2recomp.
//
// The CPS-2 board as the game's code needs it: the 68000 and what it is wired to (ROM, RAM, the video
// chips' registers and memory, the object memory, inputs, the settings EEPROM), the Z80 with the QSound
// chip, and the timing that ties them: 262 raster lines a frame, an interrupt at the start of vertical
// blanking and the raster interrupts the game asks for.
//
// It is not a model of the board's timing: a line is a fixed amount of CPU time, and the picture's lines
// are drawn as they pass. The hardware facts follow MAME's cps2 driver (BSD-3-Clause).
#pragma once

#include "../sound/driver.h"
#include "../sound/qsound.h"
#include "../sound/z80.h"
#include "../video/video.h"
#include "eeprom.h"
#include "m68k.h"
#include "romset.h"

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

namespace cps2recomp {

// A player's controls, as bits that are 1 while pressed
enum Button : uint16_t {
    kRight = 1 << 0,
    kLeft = 1 << 1,
    kDown = 1 << 2,
    kUp = 1 << 3,
    kPunch1 = 1 << 4, // light, medium, hard
    kPunch2 = 1 << 5,
    kPunch3 = 1 << 6,
    kKick1 = 1 << 7,
    kKick2 = 1 << 8,
    kKick3 = 1 << 9,
    kStart = 1 << 10,
    kCoin = 1 << 11,
};

class Machine final : public M68KBus, public Z80Bus {
public:
    static constexpr int kLines = 262, kBlankingLine = 240;
    static constexpr int kCpuCyclesPerLine = 1024; // 16 MHz, 512 pixel clocks of 8 MHz a line
    static constexpr int kSoundCyclesPerLine = 512; // 8 MHz
    // For tests: the 68000 this many times as fast as the board's, so that the game is never short of time in
    // a frame (runs whose code takes different amounts of time can then be compared picture by picture)
    int cpuSpeed = 1;
    static constexpr double kFramesPerSecond = 8000000.0 / 512 / 262;

    explicit Machine(const RomSet &roms);

    void Reset();
    // Runs to the start of the next vertical blanking: one picture and its sound
    void RunFrame();

    const uint32_t *Picture() const {
        return m_picture.data();
    }
    // How many pixels a line of it has: Video::kWidth, or more when the game's sources asked for a wider
    // picture (Video::wide) before the frame
    int PictureWidth() const {
        return m_pictureWidth;
    }
    // For a game's own sources that put the picture together themselves (two stages side by side, say): the
    // picture the frame made, to change before it is shown
    uint32_t *Picture() {
        return m_picture.data();
    }
    // Sound made and not yet taken: left and right samples at QSound::kSampleRate
    std::vector<int16_t> audio;

    // How the 68000 is run: translated code, or the interpreter (machine/m68k.h)
    M68K::Mode cpuMode = M68K::Mode::Interpreter;
    Z80::Mode soundMode = Z80::Mode::Interpreter;
    // What works the QSound chip: the board's Z80 running the game's sound program; that program as
    // ordinary code (sound/driver.h), where the game has a version of it that is known; or the Z80 with the
    // other beside it, every write to the chip compared
    enum class SoundSide { Z80, Driver, Check };
    SoundSide soundSide = SoundSide::Z80;
    std::unique_ptr<SoundDriver> soundDriver; // (none: the sound program is not a known one)
    uint64_t soundWritesChecked = 0;
    void ReportSoundTimes() const;
    bool monoForTest = false; // (--sound-test) the Z80 reads the game's stereo setting as mono

    uint16_t players[2] = {}; // Button bits
    bool serviceButton = false, testSwitch = false;

    M68K cpu;
    Z80 soundCpu;
    QSound qsound;
    Video video;
    Eeprom eeprom;

    // Memory, as bytes in the 68000's order
    std::vector<uint8_t> ram;       // FF0000-FFFFFF
    std::vector<uint8_t> videoRam;  // 900000-92FFFF
    std::vector<uint8_t> objectRam; // two banks of 0x2000
    // The object list the next frame's picture will be made from (the board takes it at the start of the
    // frame): 1024 entries of four big-endian words, as Video::LatchObjects reads them
    const uint8_t *ObjectsToCome() const {
        return &objectRam[m_objectBank ? 0x2000 : 0];
    }
    std::vector<uint8_t> extraRam;  // 660000-663FFF
    uint8_t soundRam[0x1000] = {};  // shared with the Z80 (its C000-CFFF)
    uint8_t soundRam2[0x1000] = {}; // the Z80's own (F000-FFFF)
    uint16_t cpsA[32] = {}, cpsB[32] = {}, objectRegs[6] = {};

    uint64_t frame = 0;
    void Serialize(Archive &ar);

    // M68KBus
    uint8_t Read8(uint32_t address) override;
    uint16_t Read16(uint32_t address) override;
    void Write8(uint32_t address, uint8_t value) override;
    void Write16(uint32_t address, uint16_t value) override;
    uint16_t Fetch16(uint32_t address) override;
    void InterruptAck(unsigned level) override;
    // Z80Bus
    uint8_t Read(uint16_t address) override;
    void Write(uint16_t address, uint8_t value) override;
    bool ChangesMapping(uint16_t address) override {
        return address == 0xD003;
    }
    uint32_t Mapping() override {
        return m_soundBank;
    }
    void SetMapping(uint32_t mapping) override {
        m_soundBank = static_cast<uint8_t>(mapping);
    }

private:
    const RomSet &m_roms;
    std::vector<uint32_t> m_picture;
    int m_pictureWidth = Video::kWidth;
    int m_line = 0;
    uint64_t m_lineStart = 0;  // the CPU's cycle count at which the next line starts
    bool m_blankInterrupt = false, m_rasterInterrupt = false;
    uint16_t m_rasterCounter[3] = {}, m_rasterReload[3] = {};
    uint8_t m_objectBank = 0;
    bool m_soundRunning = false;
    uint8_t m_soundBank = 0;
    uint64_t m_soundTime = 0;        // the Z80's cycle count at which the sound side has caught up
    uint64_t m_soundInterruptAt = 0; // ... of its next interrupt
    uint64_t m_sampleCount = 0;      // sound samples made
    uint64_t m_sampleAt = 0;         // the Z80's cycle count at which the next is due

    void RunLine();
    int LineCycles() const;
    void RunSound(int cycles);
    struct ChipWrite {
        uint8_t reg;
        uint16_t data;
        uint16_t pc;
        uint32_t gap; // Z80 cycles since the write before, or since the piece of work began
    };
    // (SoundSide::Driver) the driver's writes wait here for their time: each as long after the one before as
    // the Z80 would have been, and no more than one a sample
    std::vector<ChipWrite> m_chipQueue;
    uint64_t m_chipNext = 0, m_chipLast = 0; // when the first in the queue is due; when the last write was made
    uint64_t m_z80Mark = 0;                  // (SoundSide::Check) when the Z80 last wrote, or began a piece of work
    std::vector<ChipWrite> m_z80Writes, m_driverWrites; // (SoundSide::Check) not yet compared
    std::vector<ChipWrite> m_soundHistory;              // ... the last few that were
    uint16_t m_chipLatch = 0;
    bool m_driverInterrupt = false; // (SoundSide::Driver) raised and not yet taken
    void CompareSound();
    struct GapStat {
        uint64_t count = 0, z80 = 0, driver = 0;
    };
    std::unique_ptr<std::map<uint16_t, GapStat>> m_gapStats;
    void UpdateInterruptLevel();
    uint16_t ReadRegister(uint32_t address);
    void WriteRegister(uint32_t address, uint16_t value, uint16_t mask);
    uint16_t Input(int port) const;
};

} // namespace cps2recomp
