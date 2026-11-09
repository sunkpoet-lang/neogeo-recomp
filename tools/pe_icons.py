# SPDX-License-Identifier: GPL-3.0-only
"""Read embedded Windows icon resource fingerprints without executing a PE."""
import struct,hashlib
def icons(path):
 b=path.read_bytes();pe=struct.unpack_from('<I',b,0x3c)[0];assert b[pe:pe+4]==b'PE\0\0'
 n=struct.unpack_from('<H',b,pe+6)[0];size=struct.unpack_from('<H',b,pe+20)[0];opt=pe+24;magic=struct.unpack_from('<H',b,opt)[0];dd=opt+(112 if magic==0x20b else 96)
 rva=struct.unpack_from('<I',b,dd+16)[0];sections=[]
 for i in range(n):
  s=opt+size+i*40;vs,va,rs,rp=struct.unpack_from('<IIII',b,s+8);sections.append((va,max(vs,rs),rp))
 def at(v):return next(rp+v-va for va,size,rp in sections if va<=v<va+size)
 base=at(rva);result={}
 def walk(offset,keys=()):
  d=base+offset;named,ids=struct.unpack_from('<HH',b,d+12)
  for i in range(named+ids):
   name,child=struct.unpack_from('<II',b,d+16+i*8)
   key=name if not name&0x80000000 else 'named';path=keys+(key,)
   if child&0x80000000:walk(child&0x7fffffff,path)
   elif path[0] in [3,14]:
    rv,length=struct.unpack_from('<II',b,base+child);data=b[at(rv):at(rv)+length];result[str(path)]=hashlib.sha256(data).hexdigest()
 walk(0);assert any(k.startswith('(14,') for k in result) and len(result)>=8;return result
