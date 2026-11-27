# SPDX-License-Identifier: GPL-3.0-only
"""Automatic versus replays and round markers for the KOF games, headless: a scripted two-human versus records one
.fareplay by itself; played back to its end, it reports round markers and no checksum difference; a one-player game
against the computer records nothing. Usage: check_kof_auto_replays.py OUT GAME [GAME...]
(programs from build/<game>/<game>.exe, sets from D:/CPS2/ROM; the versus scripts are the KOF family result probes')"""
from pathlib import Path
import json, os, subprocess, sys
ROOT = Path(__file__).resolve().parents[1]
out = Path(sys.argv[1]).resolve(); games = sys.argv[2:]
assert not out.exists(); out.mkdir(parents=True)
env = dict(os.environ); env.update(PATH='C:/msys64/ucrt64/bin;' + env['PATH'], SDL_AUDIO_DRIVER='dummy', NEOGEO_HIDDEN_WINDOW='1')
env.pop('CPS2RECOMP_HOME', None)
# Two-human versus scripts (player 1 holds forward and presses buttons): coins and starts for both, then fighting.
fight = ','.join(f'{n}:a{p}:3' for n in range(2300, 4300, 100) for p in [1, 2]) + ',4500:right1:30000,' + \
        ','.join(f'{n}:d1:3' for n in range(4600, 16000, 35))
VERSUS = '1600:coin1:8,1620:coin2:8,1800:start1:8,1820:start2:8,2400:right1:3,2400:right2:3,2600:right1:3,2600:right2:3,' + fight
CPU = '1600:coin1:8,1800:start1:8,' + ','.join(f'{n}:a1:3' for n in range(2300, 4300, 100)) + ',4500:right1:5000'
report = {}
for game in games:
    exe = ROOT / 'build' / game / f'{game}.exe'; rom = f'D:/CPS2/ROM/{game}.zip'; r = report[game] = {}
    for kind, script, frames in (('versus', VERSUS, 14000), ('cpu', CPU, 7000)):
        d = out / game / kind; (d / 'replays').mkdir(parents=True)
        args = [str(exe), '--headless', '--rom', rom, '--frames', str(frames), '--settings', str(d / 's.ini'), '--eeprom', str(d / 'e.bin'),
                '--script', script, '--replay-dir', str(d / 'replays')]
        q = subprocess.run(args, env=env, capture_output=True, timeout=1800, creationflags=0x08000000)
        files = sorted((d / 'replays').glob('*.fareplay'))
        r[kind] = {'exit': q.returncode, 'replays': [f.name for f in files]}
        for f in files:
            p = d / ('play-' + f.stem); p.mkdir()
            q = subprocess.run([str(exe), '--headless', '--rom', rom, '--frames', '100000', '--settings', str(p / 's.ini'), '--eeprom', str(p / 'e.bin'),
                                '--replay', str(f)], env=env, capture_output=True, timeout=1800, creationflags=0x08000000)
            text = q.stderr.decode(errors='replace')
            (p / 'stderr.txt').write_text(text, encoding='utf-8')
            r[kind].setdefault('played', []).append({'file': f.name, 'exit': q.returncode, 'bytes': f.stat().st_size,
                                                     'markers': [l.split()[-1] for l in text.splitlines() if 'round marker' in l],
                                                     'differs': 'differs' in text})
    v = r['versus'].get('played', [])
    r['passed'] = (r['versus']['exit'] == 0 and r['cpu']['exit'] == 0 and len(v) >= 1 and not r['cpu']['replays'] and
                   all(x['exit'] == 0 and not x['differs'] for x in v) and any(x['markers'] for x in v))
    print(game, json.dumps(r), flush=True)
report_ok = all(r['passed'] for r in report.values())
(out / 'report.json').write_text(json.dumps({'passed': report_ok, 'games': report}, indent=2) + '\n', encoding='utf-8')
print('PASSED' if report_ok else 'FAILED')
sys.exit(0 if report_ok else 1)
