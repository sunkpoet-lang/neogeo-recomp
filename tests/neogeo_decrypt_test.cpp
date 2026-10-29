// SPDX-License-Identifier: GPL-3.0-only
// Synthetic cartridge checks. No game data or data derived from game ROMs.
#include "neogeo_decrypt.h"
#include "neogeo.h"
#include "romset.h"
#include <zlib.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace cps2recomp;
static void Check(bool condition, const char *message) {
    if (!condition) { fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static uint32_t Crc(const std::vector<uint8_t> &v) {
    return crc32(0, v.data(), static_cast<uInt>(v.size()));
}
static void Fill(std::vector<uint8_t> &v, size_t size, uint32_t &seed) {
    v.resize(size);
    for (auto &b : v) {
        seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
        b = static_cast<uint8_t>(seed >> 11);
    }
}
static NeoGeoRoms Synthetic(uint32_t seed) {
    NeoGeoRoms r;
    Fill(r.program, 0x500000, seed); Fill(r.sprites, 0x4000000, seed);
    Fill(r.soundProgram, 0x20000, seed); Fill(r.samples, 0x1000000, seed);
    return r;
}
static std::array<uint32_t,5> Hashes(const NeoGeoRoms &r) {
    return {Crc(r.program),Crc(r.sprites),Crc(r.fix),Crc(r.soundProgram),Crc(r.samples)};
}
static void Banks(size_t size) {
    NeoGeoRoms r;
    uint32_t seed = 0x451;
    Fill(r.soundProgram, size, seed); Fill(r.soundBios, 0x20000, seed);
    // Safe reset vectors for the board constructor.
    r.bios.resize(0x20000); r.program.resize(0x100000);
    auto linear = r.soundProgram;
    const auto bios = r.soundBios;
    NeoGeo board(std::move(r));
    Check(board.Read(uint16_t(0x1234)) == bios[0x1234], "BIOS fixed window");
    board.Write8(0x3a001b, 0); // latch output 5: cartridge FIX/audio
    Check(board.Read(uint16_t(0x1234)) == linear[0x1234], "cartridge fixed window has no MAME prefix");
    const unsigned starts[] = {0xf000,0xe000,0xc000,0x8000};
    for (unsigned region=0; region<4; ++region) {
        const unsigned width = 1u << (11+region);
        for (unsigned bank=0; bank<256; ++bank) {
            board.In(static_cast<uint16_t>((bank<<8) | (8+region)));
            // MAME init_audio masks the bank base, then adds the window offset.
            const unsigned base = (bank*width) & ((size-1) & 0x3ffff);
            for (unsigned offset : {0u, 19u, region ? width-1 : 0x7ffu}) {
                Check(board.Read(static_cast<uint16_t>(starts[region]+offset)) == linear[base+offset], "Z80 bank read");
            }
        }
    }
    board.Write8(0x3a000b, 0);
    Check(board.Read(uint16_t(0x1234)) == bios[0x1234], "BIOS selection restored");
}
static RomFile Write(const std::filesystem::path &dir, const char *name, const std::vector<uint8_t> &data) {
    std::ofstream out(dir/name, std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    Check(bool(out), "write synthetic ROM");
    RomFile f; f.name=name; f.crc=Crc(data); return f;
}
static void Loader(const std::filesystem::path &dir, const NeoGeoRoms &raw, const NeoGeoRoms &decoded) {
    std::filesystem::create_directories(dir);
    NeoGeoList list; list.set="kof2002";
    auto p=raw.program;
    for (size_t i=0; i<p.size(); i+=2) std::swap(p[i],p[i+1]);
    list.program={Write(dir,"p.bin",p)};
    list.soundProgram={Write(dir,"m.bin",raw.soundProgram)};
    list.samples={Write(dir,"v.bin",raw.samples)};
    std::vector<uint8_t> even(raw.sprites.size()/2), odd(even.size());
    for (size_t i=0; i<even.size(); ++i) { even[i]=raw.sprites[2*i]; odd[i]=raw.sprites[2*i+1]; }
    RomFile pair; pair.parts={Write(dir,"c1.bin",even),Write(dir,"c2.bin",odd)};
    list.sprites={pair};
    for (auto name : {"bios.bin","fixbios.bin","sm1.bin","zoom.bin"}) list.bios.push_back(Write(dir,name,std::vector<uint8_t>(0x20000)));
    NeoGeoRoms loaded; std::string error;
    Check(LoadNeoGeoSet(dir.string(),list,loaded,error),error.c_str());
    Check(Hashes(loaded)==Hashes(decoded),"loader decrypts after interleaving and word swap");
    list.set="kof97";
    Check(LoadNeoGeoSet(dir.string(),list,loaded,error),error.c_str());
    Check(Hashes(loaded)==Hashes(raw),"other set bypasses decryption");
    // Corrupt data without updating CRC: the loader must reject before decryption.
    std::ofstream bad(dir/"m.bin", std::ios::binary|std::ios::trunc); bad.put(0); bad.close();
    list.set="kof2002";
    Check(!LoadNeoGeoSet(dir.string(),list,loaded,error),"corrupt ROM rejected");
    Check(error.find("m.bin")!=std::string::npos,"CRC error identifies file");
}
int main(int argc,char **argv) {
    std::string error;
    for (uint32_t seed : {0x12345678u, 0xdeadbeefu}) {
        auto raw = Synthetic(seed);
        auto decoded = raw;
        Check(DecryptKof2002(decoded,error),error.c_str());
        const auto hashes=Hashes(decoded);
        printf("%08x:",seed); for(auto h:hashes) printf(" %08x",h); puts("");
        // Golden CRCs generated with the unmodified upstream MAME algorithms.
        const std::array<uint32_t,5> expected = seed==0x12345678u ?
            std::array<uint32_t,5>{0xf6e98944,0x99c72e26,0xf7903932,0x7a861d99,0x3f25635a} : std::array<uint32_t,5>{0x95812652,0xe656e0b8,0xe5cea0c6,0x4e79da13,0xf187d379};
        Check(hashes==expected,"MAME synthetic golden CRCs");
        Check(decoded.fix.size()==0x20000 && decoded.soundProgram.size()==0x80000,"decoded region sizes");
        Check(std::all_of(decoded.soundProgram.begin()+0x20000,decoded.soundProgram.end(),[](uint8_t b){return b==0;}),"M1 zero tail, no repetition");
        if (seed==0x12345678u && argc>1) Loader(argv[1],raw,decoded);
        for(unsigned region=0;region<5;++region) {
            auto invalid=raw;
            if(region==0) invalid.program.pop_back();
            if(region==1) invalid.sprites.pop_back();
            if(region==2) invalid.samples.pop_back();
            if(region==3) invalid.soundProgram.resize(0x80000); // reject already-expanded/predecoded M1
            if(region==4) invalid.fix.resize(0x20000);
            auto before=Hashes(invalid);
            Check(!DecryptKof2002(invalid,error),"invalid layout rejected");
            Check(before==Hashes(invalid),"invalid layout leaves regions unchanged");
            Check(error.find("kof2002")!=std::string::npos,"layout error identifies set");
        }
    }
    Banks(0x20000); Banks(0x80000);
    puts("decryption, loader isolation, validation and Z80 banking passed");
}
