# SPDX-License-Identifier: GPL-3.0-only
"""KOF95 through ForkAde's tunnel (cps-tunel), as the launcher plays a match: the host's program waits on 7845 behind
`cps-tunel host`; the other player and, some seconds later, a spectator each go through their own `cps-tunel join CODE`.
All hidden and headless, each with a nickname; the players say a line each (with accents).
Checked: the players agree on every frame, the late spectator's frames equal the host's, the chat crossed, the host
admitted the spectator. Same as ForkAde's tools/mvc2/session_check.py, for the Neo Geo program's flags.
Usage: check_kof95_tunnel.py --program EXE --rom ZIP --tunnel cps-tunel.exe --out DIR [--frames 3600]
       [--spectator-after 20] [--relay-only]"""
from pathlib import Path
import argparse, json, os, queue, subprocess, sys, threading, time

p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
p.add_argument('--program', type=Path, required=True)
p.add_argument('--rom', required=True)
p.add_argument('--tunnel', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
p.add_argument('--frames', type=int, default=3600)
p.add_argument('--spectator-after', type=float, default=20)
p.add_argument('--relay-only', action='store_true', help="every tunnel through n0's relays")
a = p.parse_args()
out = (a.out / ('kof95' + ('-relay' if a.relay_only else '-direct'))).resolve()
assert not out.exists(); out.mkdir(parents=True)
program = a.program.resolve()
hidden = 0x08000000
children = []
env = dict(os.environ); env.update(PATH='C:/msys64/ucrt64/bin;' + env['PATH'], SDL_AUDIO_DRIVER='dummy', NEOGEO_HIDDEN_WINDOW='1',
                                   CPS2RECOMP_HIDDEN_WINDOW='1')
env.pop('CPS2RECOMP_HOME', None)


def tunnel(name, *args):
    child = subprocess.Popen([str(a.tunnel.resolve()), *args, '--machine', *(['--relay-only'] if a.relay_only else [])],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, creationflags=hidden)
    children.append(child)
    lines = queue.Queue()
    log = open(out / f'tunnel-{name}.log', 'w', encoding='utf-8')

    def read():
        for raw in child.stdout:
            text = raw.decode(errors='replace').strip(); log.write(text + '\n'); log.flush()
            try: lines.put(json.loads(text))
            except ValueError: pass
    threading.Thread(target=read, daemon=True).start()
    return lines


def wait_for(lines, event, seconds=180):
    end = time.time() + seconds
    while time.time() < end:
        try: line = lines.get(timeout=1)
        except queue.Empty: continue
        if line.get('event') == event: return line.get('value')
        if line.get('event') == 'ERROR': sys.exit(f'tunnel: {line.get("value")}')
    sys.exit(f'tunnel: no {event} in {seconds} s')


def side(name, *args):
    (out / name).mkdir()
    cmd = [str(program), '--rom', a.rom, '--headless', '--frames', str(a.frames), '--settings', str(out / name / 'settings.ini'),
           '--eeprom', str(out / name / 'eeprom.bin'), '--hash-log', str(out / f'{name}.hash'), '--chat-log', str(out / name / 'chat'), *args]
    (out / f'{name}.command.json').write_text(json.dumps(cmd, ensure_ascii=False), encoding='utf-8')
    child = subprocess.Popen(cmd, stdout=open(out / f'{name}.log', 'wb'), stderr=subprocess.STDOUT, env=env, creationflags=hidden)
    children.append(child)
    return child


report = {'game': 'kof95', 'frames': a.frames, 'relay_only': a.relay_only}
started = time.time()
try:
    host = side('host', '--host', '7845', '--spectators', '6', '--nick', 'Daniel', '--net-say', '600:hola desde el host')
    hosting = tunnel('host', 'host', '--game-port', '7845')
    code = wait_for(hosting, 'CODE')
    joining = tunnel('join', 'join', code)
    port = wait_for(joining, 'LISTEN')
    join = side('join', '--join', f'127.0.0.1:{port}', '--nick', 'Iñaki', '--net-say', '700:suerte, ñandú')
    time.sleep(a.spectator_after)
    watching = tunnel('spectator', 'join', code)
    spectator_port = wait_for(watching, 'LISTEN')
    report['spectator_started_s'] = round(time.time() - started, 1)
    spectator = side('spectator', '--watch', f'127.0.0.1:{spectator_port}', '--nick', 'Rodrigo')
    limit = time.time() + a.frames / 20 + 300
    for child in (spectator, join, host):
        try: child.wait(timeout=max(1, limit - time.time()))
        except subprocess.TimeoutExpired: child.kill(); child.wait()
    report['exit'] = {'host': host.returncode, 'join': join.returncode, 'spectator': spectator.returncode}
finally:
    for child in children:
        if child.poll() is None: child.kill()


def frames(path):
    seen = {}
    if path.exists():
        for line in path.read_text().splitlines():
            parts = line.split()
            if len(parts) == 2: seen[int(parts[0])] = parts[1]
    return seen


def text(name): return (out / f'{name}.log').read_text(encoding='utf-8', errors='replace')
def chat(name): return '\n'.join(f.read_text(encoding='utf-8', errors='replace') for f in (out / name / 'chat').glob('*'))


host_f, join_f, spec_f = frames(out / 'host.hash'), frames(out / 'join.hash'), frames(out / 'spectator.hash')
last = min(max(host_f, default=0), max(join_f, default=0)) - 10
common = sorted(f for f in set(host_f) & set(join_f) if f <= last)
report['players_frames'] = {'common': len(common), 'different': sum(host_f[f] != join_f[f] for f in common)}
watched = sorted(f for f in spec_f if f in host_f and f <= last)
report['spectator_frames'] = {'from': min(spec_f) if spec_f else None, 'shown': len(watched),
                              'different': sum(spec_f[f] != host_f[f] for f in watched)}
report['chat'] = {'host_got': chat('host').count('suerte, ñandú'), 'join_got': chat('join').count('hola desde el host')}
report['spectator_admitted'] = 'Rodrigo' in text('host') and 'watch' in text('spectator').lower()
report['said'] = {n: [l for l in text(n).splitlines() if l.startswith('online:')][:10] for n in ('host', 'join', 'spectator')}
report['passed'] = (all(v == 0 for v in report.get('exit', {'x': 1}).values()) and
                    report['players_frames']['common'] >= a.frames - 60 and report['players_frames']['different'] == 0 and
                    report['spectator_frames']['shown'] > 300 and report['spectator_frames']['different'] == 0 and
                    report['chat'] == {'host_got': 1, 'join_got': 1} and report['spectator_admitted'])
(out / 'report.json').write_text(json.dumps(report, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
print(json.dumps(report, indent=2, ensure_ascii=False))
sys.exit(0 if report['passed'] else 1)
