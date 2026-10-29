#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Reproduce synthetic golden CRCs using upstream MAME, not the port.

Download prot_cmc.cpp, prot_kof2k2.cpp and prot_pcm2.cpp from
https://github.com/mamedev/mame/tree/8c285468743535fbbde018fef23b3cfb8c9a3ec8/src/devices/bus/neogeo
into an ignored folder, then run:
python tests/mame_decrypt_reference.py --mame-dir work/mame-reference --output work/mame-golden --cxx g++
Only device framework declarations are replaced. Algorithm function bodies and tables remain upstream's.
No ROMs are read. The generated executable prints P/C/S/M/V CRCs for two artificial cartridges.
"""
import argparse
from pathlib import Path
import re
import subprocess


def function(src, name):
    match = re.search(r'(?:void|int) ' + re.escape(name) + r'\(', src)
    start = match.start()
    brace = src.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (src[end] == '{') - (src[end] == '}')
        end += 1
    return src[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mame-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cxx', default='g++')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    src = (args.mame_dir / 'prot_cmc.cpp').read_text()
    tables = '\n'.join(re.findall(r'static const uint8_t \w+\[256\]\s*=\s*\{.*?\};', src, re.S))
    header = r'''
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdio>
#include <zlib.h>
template<unsigned N, typename T, typename... B> uint32_t bitswap(T v, B... bits) {
    static_assert(sizeof...(bits)==N);
    uint32_t out=0; for(unsigned b : {static_cast<unsigned>(bits)...}) out=(out<<1)|((v>>b)&1); return out;
}
namespace util { struct sum16_creator {
    static uint16_t simple(const uint8_t *p, size_t n) { uint16_t s=0; while(n--) s+=*p++; return s; }
}; }
class cmc_prot_device {
    const uint8_t *type0_t03,*type0_t12,*type1_t03,*type1_t12;
    const uint8_t *address_8_15_xor1,*address_8_15_xor2,*address_16_23_xor1,*address_16_23_xor2,*address_0_7_xor;
    void decrypt(uint8_t*,uint8_t*,uint8_t,uint8_t,const uint8_t*,const uint8_t*,const uint8_t*,int,int);
    void gfx_decrypt(uint8_t*,uint32_t,int);
    int m1_address_scramble(int,uint16_t);
public:
    void cmc50_gfx_decrypt(uint8_t*,uint32_t,int);
    void sfix_decrypt(uint8_t*,uint32_t,uint8_t*,uint32_t);
    void cmc50_m1_decrypt(uint8_t*,uint32_t,uint8_t*,uint32_t);
};
class kof2002_prot_device { public: void kof2002_decrypt_68k(uint8_t*,uint32_t); };
class pcm2_prot_device { public: void swap(uint8_t*,uint32_t,int); };
'''
    methods = []
    for name in ['decrypt', 'gfx_decrypt', 'cmc50_gfx_decrypt', 'sfix_decrypt', 'm1_address_scramble', 'cmc50_m1_decrypt']:
        text = function(src, 'cmc_prot_device::' + name)
        # Omit only the two disabled debug dump blocks requiring machine().
        if name == 'cmc50_m1_decrypt':
            text = text[:text.index('\n\tif (0)')] + '\n}'
        methods.append(text)
    methods.append(function((args.mame_dir/'prot_kof2k2.cpp').read_text(), 'kof2002_prot_device::kof2002_decrypt_68k'))
    methods.append(function((args.mame_dir/'prot_pcm2.cpp').read_text(), 'pcm2_prot_device::swap'))
    driver = r'''
void fill(std::vector<uint8_t>& v,size_t n,uint32_t& seed) {
    v.resize(n); for(auto& b:v) {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;b=seed>>11;}
}
int main() {
    for(uint32_t initial : {0x12345678u,0xdeadbeefu}) {
        uint32_t seed=initial;
        std::vector<uint8_t> p,c,m,v,s(0x20000),audio(0x90000);
        fill(p,0x500000,seed);fill(c,0x4000000,seed);fill(m,0x20000,seed);fill(v,0x1000000,seed);
        m.resize(0x80000,0);
        kof2002_prot_device().kof2002_decrypt_68k(p.data(),p.size());
        cmc_prot_device cmc;
        cmc.cmc50_m1_decrypt(m.data(),m.size(),audio.data(),audio.size());
        cmc.cmc50_gfx_decrypt(c.data(),c.size(),0xec);
        cmc.sfix_decrypt(c.data(),c.size(),s.data(),s.size());
        pcm2_prot_device().swap(v.data(),v.size(),0);
        printf("%08x:",initial);
        for(auto *r : {&p,&c,&s,&m,&v}) printf(" %08lx",static_cast<unsigned long>(crc32(0,r->data(),r->size())));
        puts("");
    }
}
'''
    cpp = args.output/'reference.cpp'
    cpp.write_text(header + tables + '\n'.join(methods) + driver)
    exe = (args.output/'reference.exe').resolve()
    subprocess.run([args.cxx, '-std=c++20', '-O2', str(cpp), '-lz', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
