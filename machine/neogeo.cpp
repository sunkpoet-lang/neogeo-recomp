// SPDX-License-Identifier: GPL-3.0-only
// Part of neogeo-recomp.

#include "neogeo.h"

#include "../runtime/state.h"

#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace cps2recomp {

namespace {
    // The watchdog resets the board if the program does not kick it for this long (MAME: 3,244,030 ticks of the
    // 24 MHz clock), in the 68000's cycles
    constexpr uint64_t kWatchdogCycles = 3244030 / 2;

    uint16_t Word(const std::vector<uint8_t> &data, uint32_t at) {
        return at + 1 < data.size() ? static_cast<uint16_t>((data[at] << 8) | data[at + 1]) : 0xFFFF;
    }

    NeoGeoVideo::Sources VideoSources(const NeoGeoRoms &roms) {
        NeoGeoVideo::Sources s;
        s.sprites = roms.sprites.data();
        s.spritesSize = roms.sprites.size();
        s.fix = roms.fix.data();
        s.fixSize = roms.fix.size();
        s.fixBios = roms.fixBios.data();
        s.fixBiosSize = roms.fixBios.size();
        s.zoom = roms.zoom.data();
        return s;
    }

    // The calendar's fixed date: 2026-01-01 00:00:00, a Thursday (seconds, minutes, hours, day, weekday + month, year;
    // BCD, low byte first as it is shifted out)
    constexpr uint64_t kCalendarDate = 0x00ULL | (0x00ULL << 8) | (0x00ULL << 16) | (0x01ULL << 24) | (0x14ULL << 32) |
                                       (0x26ULL << 40);
} // namespace

NeoGeo::NeoGeo(NeoGeoRoms roms)
    : m_roms(std::move(roms))
    , m_video(VideoSources(m_roms))
    , m_sound(m_roms.samples)
    , m_cpu(*this)
    , m_z80(*this)
    , m_ram(0x10000)
    , m_backupRam(0x10000)
    , m_z80Ram(0x800) {
    Reset(true);
}

void NeoGeo::Reset(bool hard) {
    if (hard) {
        std::fill(m_ram.begin(), m_ram.end(), 0);
        std::fill(m_z80Ram.begin(), m_z80Ram.end(), 0);
        m_video.Reset();
        m_coldPending = true; // (power-on only, not the reset button or the watchdog)
        m_calendarTime = kCalendarDate;
        m_calendarShift = 0;
        m_calendarLines = 0;
        m_pulseHalf = 93750;
    }
    // The system latch's outputs all low: the system ROM's vectors, sound program and fix tiles, palette bank 0,
    // no shadow, the backup RAM locked
    for (unsigned q = 0; q < 8; q++) {
        SystemLatch(q, false);
    }
    // (with more than 1 MB of program, 0x200000 starts at its second MB)
    m_bankBase = m_roms.program.size() > 0x100000 ? 0x100000 : 0;
    // The Z80's windows as MAME starts them: the cartridges with no bank chip need these
    m_z80Banks[0] = 0x1E;
    m_z80Banks[1] = 0x0E;
    m_z80Banks[2] = 0x06;
    m_z80Banks[3] = 0x02;
    m_command = m_reply = 0;
    m_commandPending = m_nmiEnabled = m_nmiLine = false;
    m_blankPending = m_timerPending = false;
    m_timerControl = 0;
    m_timerCounter = 0;
    m_timerAt = 0;
    m_cpu.Reset(hard);
    m_z80.Reset();
    m_sound.Reset();
    m_lineStart = m_cpu.cycles;
    m_lastKick = m_cpu.cycles;
    UpdateInterrupts();
}

// --- the frame ---

void NeoGeo::RunFrame() {
    // A coin pressed now is held for kCoinFrames, however short the press (the system ROM ignores a short one)
    for (int p = 0; p < 2; p++) {
        const bool pressed = players[p] & kCoin;
        if (pressed && !m_coinBefore[p]) {
            m_coinHeld[p] = kCoinFrames;
        }
        m_coinBefore[p] = pressed;
        if (m_coinHeld[p]) {
            players[p] |= kCoin;
            m_coinHeld[p]--;
        } else {
            players[p] &= ~kCoin;
        }
    }
    for (int line = 0; line < kLines; line++) {
        m_line = line;
        if (line == kBlankLine) {
            m_blankPending = true;
            UpdateInterrupts();
            if (m_timerControl & 0x40) {
                ReloadTimer(); // (the display position timer reloaded at each blanking)
            }
        }
        if (line == kAnimationLine) {
            m_video.StepAnimation();
        }
        const uint64_t end = m_lineStart + kCpuPerLine;
        RunUntil(end);
        // The Z80 at a third of the 68000's clock, to the same point, and the YM2610 with it (at twice the Z80's clock),
        // its interrupt line where the Z80 sees it
        while (m_z80.cycles < end / 3) {
            m_z80.cycles += m_z80.Step();
            m_sound.RunTo(m_z80.cycles * 2);
            m_z80.SetInterruptLine(m_sound.Interrupt());
        }
        m_video.RenderLine(line);
        m_lineStart = end;
        if (m_cpu.cycles - m_lastKick > kWatchdogCycles) {
            watchdogResets++;
            fprintf(stderr, "neogeo: the watchdog reset the board (frame %llu, pc %06x)\n",
                    static_cast<unsigned long long>(m_frames), m_cpu.pc);
            Reset(false);
        }
    }
    m_frames++;
}

void NeoGeo::RunUntil(uint64_t cpuCycles) {
    while (m_cpu.cycles < cpuCycles) {
        if (m_timerAt && m_cpu.cycles / 2 >= m_timerAt) {
            // The display position timer runs out: an interrupt if it is on, and again if it repeats
            if (m_timerControl & 0x10) {
                m_timerPending = true;
                UpdateInterrupts();
            }
            if (m_timerControl & 0x80) {
                ReloadTimer();
            } else {
                m_timerAt = 0;
            }
        }
        m_cpu.cycles += m_cpu.Step();
    }
}

void NeoGeo::ReloadTimer() {
    // (in pixels, at half the 68000's clock: the counter plus one from now)
    m_timerAt = m_cpu.cycles / 2 + static_cast<uint64_t>(m_timerCounter) + 1;
}

void NeoGeo::UpdateInterrupts() {
    m_cpu.SetInterruptLevel(m_coldPending ? 3 : m_timerPending ? 2 : m_blankPending ? 1 : 0);
}

// --- the 68000's memory ---

uint16_t NeoGeo::Read16(uint32_t address) {
    const uint32_t a = address & 0xFFFFFE;
    if (a < 0x80) {
        return Word(m_cartVectors ? m_roms.program : m_roms.bios, a);
    }
    if (a < 0x100000) {
        return Word(m_roms.program, a);
    }
    if (a < 0x200000) {
        return static_cast<uint16_t>((m_ram[a & 0xFFFF] << 8) | m_ram[(a & 0xFFFF) + 1]);
    }
    if (a < 0x300000) {
        return Word(m_roms.program, m_bankBase + (a & 0xFFFFF));
    }
    if (a < 0x400000) {
        return ReadRegister(a);
    }
    if (a < 0x800000) {
        return m_video.ReadPalette((a & 0x1FFF) >> 1);
    }
    if (a >= 0xC00000 && a < 0xD00000) {
        return Word(m_roms.bios, a & 0x1FFFF);
    }
    if (a >= 0xD00000 && a < 0xE00000) {
        return static_cast<uint16_t>((m_backupRam[a & 0xFFFF] << 8) | m_backupRam[(a & 0xFFFF) + 1]);
    }
    Unmapped(a);
    return 0xFFFF;
}

uint8_t NeoGeo::Read8(uint32_t address) {
    const uint16_t word = Read16(address);
    return static_cast<uint8_t>((address & 1) ? word : word >> 8);
}

uint16_t NeoGeo::ReadRegister(uint32_t a) {
    switch (a & 0x3E0000) {
    case 0x300000:
        if (a & 0x80) {
            // the test button (low: pressed), the other bits as the board leaves them
            return static_cast<uint16_t>(0xFF00 | ((players[0] & kTest) ? 0 : 0x80));
        }
        // player 1's controls (low: pressed) and the DIP switches
        return static_cast<uint16_t>(((~players[0] & 0xFF) << 8) | dip);
    case 0x320000: {
        // the Z80's reply, the calendar's pulse and data, and the coins and service (low: pressed)
        const bool pulse = (m_cpu.cycles / m_pulseHalf) & 1;
        uint16_t value = static_cast<uint16_t>((m_reply << 8) | 0x18);
        value |= (players[0] & kCoin) ? 0 : 0x01;
        value |= (players[1] & kCoin) ? 0 : 0x02;
        value |= (players[0] & kService) ? 0 : 0x04;
        value |= pulse ? 0x40 : 0;
        value |= (m_calendarShift & 1) ? 0x80 : 0;
        return value;
    }
    case 0x340000:
        return static_cast<uint16_t>(((~players[1] & 0xFF) << 8) | 0xFF);
    case 0x380000: {
        // the starts (low: pressed), no memory card (0x7000), an MVS (0x8000)
        uint16_t value = 0xFFFF;
        if (players[0] & kStart) {
            value &= ~0x0100;
        }
        if (players[1] & kStart) {
            value &= ~0x0400;
        }
        return value;
    }
    case 0x3C0000:
        switch ((a >> 1) & 3) {
        case 0:
        case 1:
            return m_video.ReadData();
        case 2:
            return m_video.Modulo();
        default: {
            // the line counter (from 0xF8 to 0x1FF) and the auto-animation's count
            int counter = m_line + 0x100;
            if (counter >= 0x200) {
                counter -= kLines;
            }
            return static_cast<uint16_t>((counter << 7) | (m_video.AnimationCounter() & 7));
        }
        }
    default:
        Unmapped(a);
        return 0xFFFF;
    }
}

void NeoGeo::Write16(uint32_t address, uint16_t value) {
    const uint32_t a = address & 0xFFFFFE;
    if (a >= 0x100000 && a < 0x200000) {
        m_ram[a & 0xFFFF] = static_cast<uint8_t>(value >> 8);
        m_ram[(a & 0xFFFF) + 1] = static_cast<uint8_t>(value);
    } else if (a >= 0x2FFFF0 && a < 0x300000) {
        BankSelect(value);
    } else if (a >= 0x300000 && a < 0x400000) {
        WriteRegister(a, value, true, true);
    } else if (a >= 0x400000 && a < 0x800000) {
        m_video.WritePalette((a & 0x1FFF) >> 1, value);
    } else if (a >= 0xD00000 && a < 0xE00000) {
        if (m_backupUnlocked) {
            m_backupRam[a & 0xFFFF] = static_cast<uint8_t>(value >> 8);
            m_backupRam[(a & 0xFFFF) + 1] = static_cast<uint8_t>(value);
        }
    } else {
        Unmapped(a);
    }
}

void NeoGeo::Write8(uint32_t address, uint8_t value) {
    const uint32_t a = address & 0xFFFFFF;
    const bool odd = a & 1;
    // (a byte goes on its half of the bus: the high half at even addresses)
    const uint16_t onBus = odd ? value : static_cast<uint16_t>(value << 8);
    if (a >= 0x100000 && a < 0x200000) {
        m_ram[a & 0xFFFF] = value;
    } else if (a >= 0x2FFFF0 && a < 0x300000) {
        BankSelect(onBus);
    } else if (a >= 0x300000 && a < 0x400000) {
        WriteRegister(a & ~1u, onBus, !odd, odd);
    } else if (a >= 0x400000 && a < 0x800000) {
        const uint32_t index = (a & 0x1FFF) >> 1;
        const uint16_t old = m_video.ReadPalette(index);
        m_video.WritePalette(index, odd ? static_cast<uint16_t>((old & 0xFF00) | value)
                                        : static_cast<uint16_t>((old & 0x00FF) | (value << 8)));
    } else if (a >= 0xD00000 && a < 0xE00000) {
        if (m_backupUnlocked) {
            m_backupRam[a & 0xFFFF] = value;
        }
    } else {
        Unmapped(a);
    }
}

void NeoGeo::BankSelect(uint16_t value) {
    // The second part of the program in 1 MB banks at 0x200000 (a bank past the end: the first)
    const size_t size = m_roms.program.size();
    if (size <= 0x100000) {
        return;
    }
    uint32_t bank = value & 7;
    if ((bank + 1) * 0x100000 >= size) {
        bank = 0;
    }
    m_bankBase = (bank + 1) * 0x100000;
}

void NeoGeo::WriteRegister(uint32_t a, uint16_t value, bool high, bool low) {
    switch (a & 0x3E0000) {
    case 0x300000:
        if (low) {
            m_lastKick = m_cpu.cycles; // the watchdog (0x300001)
        }
        return;
    case 0x320000:
        if (high) {
            SoundCommand(static_cast<uint8_t>(value >> 8)); // a command for the Z80 (0x320000)
        }
        return;
    case 0x380000:
        if (low) {
            // The I/O control: the calendar's lines (0x380051) and things with no effect here (the controllers' and
            // the memory card's selects, the cabinet's LED displays at 0x380021-0x380041, the coin counters and
            // lockouts)
            const unsigned offset = (a & 0xFF) >> 1;
            const unsigned kind = offset & 0x78;
            if (kind == 0x28) {
                Calendar(static_cast<uint8_t>(value));
            } else if (kind != 0x00 && kind != 0x08 && kind != 0x10 && kind != 0x18 && kind != 0x20 && kind != 0x30 &&
                       kind != 0x70) {
                Unmapped(a | 1);
            }
        }
        return;
    case 0x3A0000:
        if (low) {
            // The system latch: output (offset & 7), set or cleared (offset & 8)
            const unsigned offset = (a & 0x1F) >> 1;
            SystemLatch(offset & 7, (offset & 8) != 0);
        }
        return;
    case 0x3C0000: {
        if (!high) {
            return; // (the low byte alone reaches nothing)
        }
        if (!low) {
            value = static_cast<uint16_t>((value & 0xFF00) | (value >> 8)); // (the high byte alone: in both halves)
        }
        switch ((a >> 1) & 7) {
        case 0:
            m_video.SetAddress(value);
            break;
        case 1:
            m_video.WriteData(value);
            break;
        case 2:
            m_video.SetModulo(value);
            break;
        case 3:
            m_video.SetMode(value);
            m_timerControl = value & 0xF0;
            break;
        case 4:
            m_timerCounter = (m_timerCounter & 0xFFFF) | (static_cast<uint32_t>(value) << 16);
            break;
        case 5:
            m_timerCounter = (m_timerCounter & 0xFFFF0000u) | value;
            if (m_timerControl & 0x20) {
                ReloadTimer(); // (loaded at once when the low half is written)
            }
            break;
        case 6:
            // The interrupts acknowledged: cold boot, timer, blanking
            if (value & 1) {
                m_coldPending = false;
            }
            if (value & 2) {
                m_timerPending = false;
            }
            if (value & 4) {
                m_blankPending = false;
            }
            UpdateInterrupts();
            break;
        default:
            break;
        }
        return;
    }
    default:
        Unmapped(a);
    }
}

void NeoGeo::SystemLatch(unsigned output, bool value) {
    switch (output) {
    case 0:
        m_video.SetShadow(value);
        break;
    case 1:
        m_cartVectors = value;
        break;
    case 5:
        // the Z80's first 32 KB and the fix layer's tiles: the cartridge's, or the system's
        m_cartSound = value;
        m_video.SetFixFromCartridge(value);
        break;
    case 6:
        m_backupUnlocked = value;
        break;
    case 7:
        m_video.SetPaletteBank(value);
        break;
    default:
        break; // (2-4: the memory card's)
    }
}

void NeoGeo::Calendar(uint8_t lines) {
    // Data in (bit 0), clock (bit 1), strobe (bit 2); its select and command lines are wired high, so commands come in
    // serially, in the register's top four bits
    const bool clockUp = (lines & 2) && !(m_calendarLines & 2);
    const bool strobeUp = (lines & 4) && !(m_calendarLines & 4);
    if (clockUp) {
        m_calendarShift = (m_calendarShift >> 1) | (static_cast<uint64_t>(lines & 1) << 51);
    }
    if (strobeUp && !(lines & 2)) {
        const unsigned command = static_cast<unsigned>(m_calendarShift >> 48) & 0xF;
        switch (command) {
        case 2: // time set
            m_calendarTime = m_calendarShift & 0xFFFFFFFFFFFFULL;
            break;
        case 3: // time read
            m_calendarShift = m_calendarTime;
            break;
        case 4: // the test pulse at 64, 256, 2048 or 4096 Hz
        case 5:
        case 6:
        case 7: {
            static const uint32_t kHertz[4] = {64, 256, 2048, 4096};
            m_pulseHalf = 6000000 / kHertz[command - 4];
            break;
        }
        case 8: // ... or every 1, 10, 30 or 60 seconds
        case 9:
        case 10:
        case 11: {
            static const uint32_t kSeconds[4] = {1, 10, 30, 60};
            m_pulseHalf = 6000000 * kSeconds[command - 8];
            break;
        }
        default:
            break; // (hold, shift: the register stays as it is)
        }
    }
    m_calendarLines = lines;
}

void NeoGeo::Unmapped(uint32_t address) {
    if (unmapped.size() < 256 && unmapped.insert(address).second) {
        fprintf(stderr, "neogeo: nothing at %06x (pc %06x)\n", address, m_cpu.instruction);
    }
}

// --- the Z80 ---

void NeoGeo::SoundCommand(uint8_t value) {
    // (NEOGEORECOMP_SOUNDTRACK_LOG: each command, with the frame, for finding a game's music; the Z80's replies too)
    static const bool log = getenv("NEOGEORECOMP_SOUNDTRACK_LOG") != nullptr;
    if (log) {
        fprintf(stderr, "sound: frame %llu command %02X\n", static_cast<unsigned long long>(m_frames), value);
    }
    m_command = value;
    m_commandPending = true;
    UpdateNmi();
}

void NeoGeo::UpdateNmi() {
    // (the NMI comes on the rising edge of "a command waits, and NMIs are enabled")
    const bool line = m_commandPending && m_nmiEnabled;
    if (line && !m_nmiLine) {
        m_z80.Nmi();
    }
    m_nmiLine = line;
}

uint8_t NeoGeo::Read(uint16_t address) {
    if (address < 0x8000) {
        const std::vector<uint8_t> &program = m_cartSound ? m_roms.soundProgram : m_roms.soundBios;
        return address < program.size() ? program[address] : 0xFF;
    }
    if (address >= 0xF800) {
        return m_z80Ram[address & 0x7FF];
    }
    // The cartridge's banked windows: 0x8000 (16 KB), 0xC000 (8 KB), 0xE000 (4 KB), 0xF000 (2 KB)
    int region;
    uint16_t start;
    if (address < 0xC000) {
        region = 3;
        start = 0x8000;
    } else if (address < 0xE000) {
        region = 2;
        start = 0xC000;
    } else if (address < 0xF000) {
        region = 1;
        start = 0xE000;
    } else {
        region = 0;
        start = 0xF000;
    }
    const std::vector<uint8_t> &program = m_roms.soundProgram;
    if (program.empty()) {
        return 0xFF;
    }
    const uint32_t at = ((static_cast<uint32_t>(m_z80Banks[region]) << (11 + region)) + (address - start)) %
                        static_cast<uint32_t>(program.size());
    return program[at];
}

void NeoGeo::Write(uint16_t address, uint8_t value) {
    if (address >= 0xF800) {
        m_z80Ram[address & 0x7FF] = value;
    }
}

uint8_t NeoGeo::In(uint16_t port) {
    const uint8_t low = static_cast<uint8_t>(port);
    if (low == 0x00) {
        return m_command;
    }
    if (low >= 0x04 && low <= 0x07) {
        m_sound.RunTo(m_z80.cycles * 2);
        return m_sound.Read(low - 0x04);
    }
    if ((low & 0x0F) >= 0x08 && (low & 0x0F) <= 0x0B) {
        m_z80Banks[low & 3] = static_cast<uint8_t>(port >> 8); // (a bank chosen by reading: the port's high byte)
        return 0x00;
    }
    return 0x00;
}

void NeoGeo::Out(uint16_t port, uint8_t value) {
    const uint8_t low = static_cast<uint8_t>(port);
    if (low == 0x00) {
        m_commandPending = false; // the command taken
        UpdateNmi();
    } else if (low == 0x08 || low == 0x18) {
        m_nmiEnabled = low == 0x08;
        UpdateNmi();
    } else if (low == 0x0C) {
        static const bool log = getenv("NEOGEORECOMP_SOUNDTRACK_LOG") != nullptr;
        if (log && value != m_reply) {
            fprintf(stderr, "sound: frame %llu reply %02X\n", static_cast<unsigned long long>(m_frames), value);
        }
        m_reply = value;
    } else if (low >= 0x04 && low <= 0x07) {
        m_sound.RunTo(m_z80.cycles * 2);
        m_sound.Write(low - 0x04, value);
        m_z80.SetInterruptLine(m_sound.Interrupt());
    }
}

// --- state ---

void NeoGeo::Serialize(Archive &ar) {
    ar.Mark(0x4E454F47); // "NEOG"
    m_cpu.Serialize(ar);
    m_z80.Serialize(ar);
    m_video.Serialize(ar);
    m_sound.Serialize(ar);
    ar.Vec(m_ram);
    ar.Vec(m_backupRam);
    ar.Vec(m_z80Ram);
    ar.Pod(m_bankBase);
    ar.Pod(m_cartVectors);
    ar.Pod(m_cartSound);
    ar.Pod(m_backupUnlocked);
    ar.Pod(m_z80Banks);
    ar.Pod(m_command);
    ar.Pod(m_reply);
    ar.Pod(m_commandPending);
    ar.Pod(m_nmiEnabled);
    ar.Pod(m_nmiLine);
    ar.Pod(m_blankPending);
    ar.Pod(m_timerPending);
    ar.Pod(m_coldPending);
    ar.Pod(m_timerControl);
    ar.Pod(m_timerCounter);
    ar.Pod(m_timerAt);
    ar.Pod(m_line);
    ar.Pod(m_frames);
    ar.Pod(m_lineStart);
    ar.Pod(m_lastKick);
    ar.Pod(m_calendarShift);
    ar.Pod(m_calendarTime);
    ar.Pod(m_calendarLines);
    ar.Pod(m_pulseHalf);
    ar.Pod(players);
    ar.Pod(dip);
    ar.Pod(m_coinHeld);
    ar.Pod(m_coinBefore);
    if (!ar.Saving()) {
        UpdateInterrupts();
    }
}

uint32_t NeoGeo::StateHash() {
    std::vector<uint8_t> data;
    Archive ar(data, true);
    Serialize(ar);
    return static_cast<uint32_t>(crc32(0, data.data(), static_cast<uInt>(data.size())));
}

} // namespace cps2recomp
