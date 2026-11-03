#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Synthetic KOF98 goldens from unmodified pinned MAME decrypt_68k body."""
import argparse,subprocess
from pathlib import Path
from mame_decrypt_reference import function
p=argparse.ArgumentParser();p.add_argument('--mame-file',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--cxx',required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
header='''#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
#include <zlib.h>
class kof98_prot_device {uint16_t m_default_rom[2]; public:void decrypt_68k(uint8_t*,uint32_t);};
'''
driver='''int main(){for(uint32_t seed:{0x12345678u,0xdeadbeefu}){uint32_t r=seed;std::vector<uint8_t> p(0x600000);for(auto &b:p){r^=r<<13;r^=r>>17;r^=r<<5;b=r>>11;}kof98_prot_device().decrypt_68k(p.data(),p.size());printf("%08x: %08lx\\n",seed,(unsigned long)crc32(0,p.data(),p.size()));}}
'''
cpp=a.out/'reference.cpp';cpp.write_text(header+function(a.mame_file.read_text(),'kof98_prot_device::decrypt_68k')+driver)
exe=(a.out/'reference.exe').resolve();subprocess.run([a.cxx,'-std=c++20','-O2',str(cpp),'-lz','-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
