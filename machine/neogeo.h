// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork: the Neo Geo board).
//
// The Neo Geo MVS as a game's code needs it: the 68000 and what it is wired to (the cartridge's program, with its
// second part in 1 MB banks at 0x200000; the work RAM; the system ROM at 0xC00000; the backup RAM; the inputs; the
// video chip, machine/../video/neogeo_video.h; the palette), the Z80 with its program and banks, and the timing of a
// frame: 264 lines of 384 pixels at 6 MHz, the 68000 at 12 MHz (768 cycles a line), the Z80 at 4 MHz (256).
//
// Interrupts, as the board gives them: vertical blanking (level 1) at line 240, the display position timer (level 2)
// when the program has set it, and the cold boot one (level 3) at power-on; each stays up until the program
// acknowledges it at 0x3C000C. The Z80 runs its program, answers the 68000's commands and plays the YM2610
// (machine/neogeo_sound.h), whose timers interrupt it.
//
// Like the CPS boards (machine/machine.h), not a model of the board's timing: a line is a fixed amount of CPU time.
// The map and the registers follow MAME's driver (src/mame/snk/neogeo.cpp, BSD-3-Clause).
#pragma once

#include "m68k.h"
#include "neogeo_sound.h"
#include "romset.h"

#include "../sound/z80.h"
#include "../video/neogeo_video.h"

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

namespace cps2recomp {

class Archive;

class NeoGeo final : public M68KBus, public Z80Bus {
public:
    static constexpr int kLines = 264;           // a frame (NEOGEO_VTOTAL)
    static constexpr int kBlankLine = 0xF0;      // vertical blanking starts: level 1 (NEOGEO_VBSTART)
    static constexpr int kAnimationLine = 0x100; // the sprites' auto-animation counts here (NEOGEO_VSSTART)
    static constexpr uint32_t kCpuPerLine = 768;
    static constexpr uint32_t kZ80PerLine = 256;

    explicit NeoGeo(NeoGeoRoms roms);

    // The players' controls, set before each frame: bits up (1), down, left, right, A, B, C, D (0x80), start
    // (0x100), select, coin (0x400); and the cabinet's service (0x800) and test (0x1000) buttons on player 1's
    enum : uint16_t {
        kUp = 0x001, kDown = 0x002, kLeft = 0x004, kRight = 0x008,
        kA = 0x010, kB = 0x020, kC = 0x040, kD = 0x080,
        kStart = 0x100, kSelect = 0x200, kCoin = 0x400, kService = 0x800, kTest = 0x1000,
    };
    // A coin, however short, is held for kCoinFrames from when it is pressed: the system ROM does not count a shorter one
    uint16_t players[2] = {};
    static constexpr int kCoinFrames = 40;
    // The MVS's eight DIP switches (a bit 0: the switch on); a new board has them all off
    uint8_t dip = 0xFF;

    // Power on (hard) or the reset button
    void Reset(bool hard);
    void RunFrame();
    uint64_t Frames() const {
        return m_frames;
    }
    const uint32_t *Picture() const {
        return m_video.Picture();
    }
    // The sound chip: what it has made (NeoGeoSound::made, taken by the frontend) and its rate
    NeoGeoSound &Sound() {
        return m_sound;
    }
    // Whether the game has the board (the cartridge's vectors in), not the system ROM: at power-on the system tests the
    // work RAM, and a game's sources writing it then make the test fail ("WORK RAM ERROR")
    bool GameRunning() const {
        return m_cartVectors;
    }
    // For a game's sources (runtime/neogeo_game.h): the work RAM (0x100000-0x10FFFF, mirrored), as the 68000 sees it
    uint8_t Peek8(uint32_t address) const {
        return m_ram[address & 0xFFFF];
    }
    uint16_t Peek16(uint32_t address) const {
        return static_cast<uint16_t>((m_ram[address & 0xFFFE] << 8) | m_ram[(address & 0xFFFE) + 1]);
    }
    void Poke8(uint32_t address, uint8_t value) {
        m_ram[address & 0xFFFF] = value;
    }
    void Poke16(uint32_t address, uint16_t value) {
        m_ram[address & 0xFFFE] = static_cast<uint8_t>(value >> 8);
        m_ram[(address & 0xFFFE) + 1] = static_cast<uint8_t>(value);
    }
    // The backup RAM (the system's and the games' settings, bookkeeping and high scores), kept in a file between runs
    std::vector<uint8_t> &BackupRam() {
        return m_backupRam;
    }

    // Save states and the hash of the whole state (68000, Z80, memories, registers, video)
    void Serialize(Archive &ar);
    uint32_t StateHash();

    // For the log: how often the program let the watchdog reset the board, and which addresses it read or wrote
    // that nothing answers
    unsigned watchdogResets = 0;
    std::set<uint32_t> unmapped;

    // M68KBus
    uint8_t Read8(uint32_t address) override;
    uint16_t Read16(uint32_t address) override;
    void Write8(uint32_t address, uint8_t value) override;
    void Write16(uint32_t address, uint16_t value) override;
    uint16_t Fetch16(uint32_t address) override {
        return Read16(address);
    }

    // Z80Bus
    uint8_t Read(uint16_t address) override;
    void Write(uint16_t address, uint8_t value) override;
    uint8_t In(uint16_t port) override;
    void Out(uint16_t port, uint8_t value) override;

private:
    uint16_t ReadRegister(uint32_t address);
    void WriteRegister(uint32_t address, uint16_t value, bool high, bool low);
    void SystemLatch(unsigned output, bool value);
    void BankSelect(uint16_t value);
    void Calendar(uint8_t lines);
    void Unmapped(uint32_t address);
    void UpdateInterrupts();
    void SoundCommand(uint8_t value);
    void UpdateNmi();
    // The display position timer: when it next fires, in pixels since power-on (0: not running)
    void ReloadTimer();
    void RunUntil(uint64_t cpuCycles);

    NeoGeoRoms m_roms;
    NeoGeoVideo m_video;
    NeoGeoSound m_sound;
    M68K m_cpu;
    Z80 m_z80;

    std::vector<uint8_t> m_ram, m_backupRam, m_z80Ram;
    uint32_t m_bankBase = 0;     // where 0x200000 reads in the program
    bool m_cartVectors = false;  // the first 128 bytes from the cartridge (true) or the system ROM
    bool m_cartSound = false;    // the Z80's first 32 KB from the cartridge's M1 (true) or the system's sm1
    bool m_backupUnlocked = false;
    uint8_t m_z80Banks[4] = {};  // the windows at 0xF000, 0xE000, 0xC000, 0x8000 (2, 4, 8, 16 KB)
    uint8_t m_command = 0, m_reply = 0;
    bool m_commandPending = false, m_nmiEnabled = false, m_nmiLine = false;

    // The coins as the board sees them: frames each is still held, and whether it was pressed the frame before
    uint8_t m_coinHeld[2] = {}, m_coinBefore[2] = {};

    // Interrupts and the display position timer
    bool m_blankPending = false, m_timerPending = false, m_coldPending = false;
    uint16_t m_timerControl = 0;
    uint32_t m_timerCounter = 0;
    uint64_t m_timerAt = 0;
    int m_line = 0;
    uint64_t m_frames = 0;
    uint64_t m_lineStart = 0; // the 68000's cycles at the start of the current line
    uint64_t m_lastKick = 0;  // ... when the program last kicked the watchdog

    // The calendar (uPD4990A): its 52-bit shift register (the time, then a 4-bit command on top), the time it keeps
    // (stopped at a fixed date, so that runs repeat), the lines the program last wrote and its test pulse's half period
    uint64_t m_calendarShift = 0, m_calendarTime = 0;
    uint8_t m_calendarLines = 0;
    uint32_t m_pulseHalf = 93750; // (64 Hz, in the 68000's cycles)
};

} // namespace cps2recomp
