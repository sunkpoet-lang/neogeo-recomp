# SPDX-License-Identifier: GPL-3.0-only
# Neo Geo source identity; follows the format-2 source-manifest approach of the CPS fork.
from pathlib import Path
import argparse,hashlib
p=argparse.ArgumentParser();p.add_argument('--root',required=True);p.add_argument('--game',required=True);p.add_argument('--output',required=True);p.add_argument('--abi',required=True);a=p.parse_args();root=Path(a.root)
patterns=(root/'replay_identity.txt').read_text().splitlines();rows=['neogeo-replay-identity-1','state 1','abi '+a.abi,'game '+a.game]
for pattern in patterns:
 if not pattern or pattern.startswith('#'):continue
 optional=pattern.startswith('?');pattern=pattern.lstrip('?').replace('@GAME@',a.game)
 files=sorted(f for f in root.glob(pattern) if f.is_file())
 if not files and not optional:raise SystemExit('Identity pattern has no files: '+pattern)
 for f in files:rows.append(f.relative_to(root).as_posix()+' '+hashlib.sha256(f.read_bytes().replace(bytes([13,10]),bytes([10]))).hexdigest())
value=hashlib.sha256(('\n'.join(rows)+'\n').encode()).hexdigest();out=Path(a.output);out.parent.mkdir(parents=True,exist_ok=True);out.write_text('#pragma once\n#define NEOGEO_REPLAY_IDENTITY "'+value+'"\n',encoding='utf-8')
