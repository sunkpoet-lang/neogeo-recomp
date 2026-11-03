// SPDX-License-Identifier: GPL-3.0-only
// Synthetic cartridge checks. No game data or data derived from game ROMs.
#include "neogeo_decrypt.h"
#include "neogeo.h"
#include "romset.h"
#include "state.h"
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
static void Loader(const std::filesystem::path &dir, const NeoGeoRoms &raw, const NeoGeoRoms &decoded,
                   const char *set = "kof2002") {
    std::filesystem::create_directories(dir);
    NeoGeoList list; list.set=set;
    if (!raw.fix.empty()) list.fix={Write(dir,"s.bin",raw.fix)};
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
    list.set=set;
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
    for (uint32_t seed : {0x12345678u, 0xdeadbeefu}) {
        uint32_t random = seed;
        NeoGeoRoms raw;
        Fill(raw.program,0x600000,random); Fill(raw.sprites,0x4000000,random);
        Fill(raw.soundProgram,0x20000,random); Fill(raw.samples,0x1000000,random);
        Fill(raw.fix,0x20000,random);
        auto decoded=raw;
        Check(DecryptKf2k2mp2(decoded,error),error.c_str());
        const std::array<uint32_t,5> expected=seed==0x12345678u ?
            std::array<uint32_t,5>{0x3905b7ce,0x7b9117a1,0x40ecf061,0xcf37ee59,0x460c03f0} :
            std::array<uint32_t,5>{0x902ef9ae,0x0cb550bf,0x227783c6,0x3f35e32a,0xa2eb06f3};
        Check(Hashes(decoded)==expected,"Magic Plus II MAME synthetic golden CRCs");
        Check(std::all_of(decoded.program.begin()+0x500000,decoded.program.end(),[](uint8_t b){return b==0;}),"Magic Plus II unused program tail is zero");
        Check(decoded.soundProgram.size()==0x80000,"Magic Plus II linear M1 size");
        if(seed==0x12345678u && argc>1) Loader(std::filesystem::path(argv[1])/"magic2",raw,decoded,"kf2k2mp2");
        for(unsigned region=0;region<5;++region) {
            auto invalid=raw;
            if(region==0) invalid.program.pop_back();
            if(region==1) invalid.sprites.pop_back();
            if(region==2) invalid.samples.pop_back();
            if(region==3) invalid.soundProgram.resize(0x80000);
            if(region==4) invalid.fix.clear();
            auto before=Hashes(invalid);
            Check(!DecryptKf2k2mp2(invalid,error),"Magic Plus II invalid layout rejected");
            Check(before==Hashes(invalid),"Magic Plus II invalid layout leaves all regions unchanged");
            Check(error.find("kf2k2mp2")!=std::string::npos,"Magic Plus II error identifies set");
        }
    }
    Banks(0x20000); Banks(0x40000); Banks(0x80000);
    for (uint32_t initial : {0x12345678u,0xdeadbeefu}) {
        uint32_t seed=initial;NeoGeoRoms raw;
        Fill(raw.program,0x600000,seed);Fill(raw.sprites,0x4000000,seed);
        Fill(raw.soundProgram,0x40000,seed);Fill(raw.samples,0x1000000,seed);Fill(raw.fix,0x20000,seed);
        auto decoded=raw;Check(DecryptKof98(decoded,error),error.c_str());
        auto before=Hashes(raw),after=Hashes(decoded);
        Check(after[0]==(initial==0x12345678u?0x18a84b60u:0x178dc47cu),"KOF98 independent MAME program golden");
        for(unsigned n=1;n<5;++n)Check(before[n]==after[n],"KOF98 unencrypted regions unchanged");
        Check(decoded.kof98Protection,"KOF98 enables protection");
        if(initial==0x12345678u && argc>1)Loader(std::filesystem::path(argv[1])/"kof98",raw,decoded,"kof98");
        for(unsigned n=0;n<5;++n){auto invalid=raw;
            if(n==0)invalid.program.pop_back();
            if(n==1)invalid.sprites.pop_back();
            if(n==2)invalid.fix.clear();
            if(n==3)invalid.soundProgram.pop_back();
            if(n==4)invalid.samples.pop_back();
            auto hashes=Hashes(invalid);Check(!DecryptKof98(invalid,error),"KOF98 malformed layout rejected");
            Check(Hashes(invalid)==hashes && !invalid.kof98Protection,"KOF98 rejection does not mutate");
        }
        decoded.bios.resize(0x20000);auto header=decoded.program;
        NeoGeo board(std::move(decoded));
        Check(board.Read16(0x100)==uint16_t((header[0x100]<<8)|header[0x101]),"KOF98 default header");
        board.Write16(0x20aaaa,0x90);
        Check(board.Read16(0x100)==0xc2 && board.Read16(0x102)==0xfd,"KOF98 overlay state 1");
        Check(board.Read8(0x100)==0 && board.Read8(0x101)==0xc2,"KOF98 overlay byte lanes");
        std::vector<uint8_t> state;Archive save(state,true);board.Serialize(save);auto hash=board.StateHash();
        board.Write8(0x20aaab,0xf0);Check(board.Read16(0x100)==0x4e45 && board.Read16(0x102)==0x4f2d,"KOF98 overlay state 2 byte write");
        Archive load(state,false);board.Serialize(load);Check(load.Ok() && board.StateHash()==hash && board.Read16(0x100)==0xc2,"KOF98 overlay state restored");
        board.Write16(0x20aaaa,0xaa);board.Reset(false);Check(board.Read16(0x100)==0xc2,"KOF98 unknown write and reset preserve overlay");
    }
    puts("decryption, loader isolation, validation and Z80 banking passed");
}
