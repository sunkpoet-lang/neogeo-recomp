"""Rebuild embedded recorder artwork from the shipped PNGs (Pillow, development only)."""
from pathlib import Path
import argparse,hashlib,json,zlib
from PIL import Image
root=Path(__file__).resolve().parents[1];assets=root/'assets/recorder-inputs'
parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
provenance=json.loads((assets/'provenance.json').read_text());names=[f'{button}-{pressed}' for button in 'abcd' for pressed in (0,1)]+[f'stick-{d}' for d in range(1,10)]
payload=bytearray();entries=[]
for name in names:
 path=assets/(name+'.png');assert hashlib.sha256(path.read_bytes()).hexdigest()==provenance['sprites'][name],name
 with Image.open(path) as image:
  rgba=image.convert('RGBA');raw=rgba.tobytes();packed=zlib.compress(raw,9);assert zlib.decompress(packed)==raw
  entries.append((rgba.width,rgba.height,len(payload),len(packed)));payload.extend(packed)
text='// Original recorder-rendered RGBA assets; see assets/recorder-inputs.\n#pragma once\n#include <cstdint>\nnamespace port::recorder_art {\nstruct Entry{int width,height,offset,bytes;};\ninline constexpr Entry entries[]={\n'+''.join(' {%d,%d,%d,%d},\n'%entry for entry in entries)+'};\ninline constexpr uint8_t packed[]={\n'+''.join(' '+','.join('0x%02x'%b for b in payload[i:i+24])+',\n' for i in range(0,len(payload),24))+'};\n}\n'
target=root/'port/recorder_input_art.h'
if args.check:assert target.read_bytes()==text.encode(),'embedded artwork differs; regenerate with this tool'
else:target.write_bytes(text.encode())
print('Recorder PNG provenance and embedded RGBA verified' if args.check else 'Recorder RGBA header regenerated')
