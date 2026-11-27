# SPDX-License-Identifier: GPL-3.0-only
"""KOF95 spectators, chat and names on one PC (hidden, headless): a host/guest match, two spectators joining late
(after 4 s and 12 s), lines said both ways (with accents) and by a spectator. Checks: the players agree frame by frame,
each spectator's frames equal the players' from where it joined, every player line reaches the other player once,
spectators get the players' lines, and the host's log names everyone. Also the same players' frames with no spectators.
Usage: check_kof95_spectators.py EXE ROM OUT [FRAMES]"""
from pathlib import Path
import json, os, subprocess, sys, time
exe, rom, out = Path(sys.argv[1]).resolve(), sys.argv[2], Path(sys.argv[3]).resolve()
frames = int(sys.argv[4]) if len(sys.argv) > 4 else 1800
assert not out.exists(); out.mkdir(parents=True)
env = dict(os.environ); env.update(PATH='C:/msys64/ucrt64/bin;' + env['PATH'], SDL_AUDIO_DRIVER='dummy', NEOGEO_HIDDEN_WINDOW='1', CPS2RECOMP_HIDDEN_WINDOW='1')
env.pop('CPS2RECOMP_HOME', None)
NO_WINDOW = 0x08000000

def run(case, port, spectators):
    d = out / case; d.mkdir(); procs = []; logs = []
    def side(name, *args):
        (d / name).mkdir(); f = (d / (name + '.log')).open('wb'); logs.append(f)
        cmd = [str(exe), '--rom', rom, '--headless', '--frames', str(frames), '--settings', str(d / name / 'settings.ini'),
               '--eeprom', str(d / name / 'eeprom.bin'), '--hash-log', str(d / (name + '.hash')), '--chat-log', str(d / name / 'chat'), *args]
        (d / (name + '.command.json')).write_text(json.dumps(cmd, ensure_ascii=False), encoding='utf-8')
        procs.append(subprocess.Popen(cmd, env=env, stdout=f, stderr=subprocess.STDOUT, creationflags=NO_WINDOW))
    try:
        side('host', '--host', str(port), '--spectators', '6', '--nick', 'Daniel', '--net-say', '200:hola, ¿revancha?', '--net-say', '900:¡bien jugado!')
        time.sleep(2)
        side('guest', '--join', f'127.0.0.1:{port}', '--nick', 'Iñaki', '--net-say', '260:sí, otra', '--net-say', '1000:gg')
        if spectators:
            time.sleep(4); side('spec1', '--watch', f'127.0.0.1:{port}', '--nick', 'Rodrigo', '--net-say', '1200:qué partidazo')
            time.sleep(8); side('spec2', '--watch', f'127.0.0.1:{port}', '--nick', 'Ana')
        for p in procs: p.wait(timeout=frames / 20 + 180)
    finally:
        for p in procs:
            if p.poll() is None: p.kill(); p.wait()
        for f in logs: f.close()
    def hashes(name):
        r = {}
        for line in (d / (name + '.hash')).read_text(encoding='utf-8').splitlines():
            v = line.split()
            if len(v) >= 2: r[int(v[0])] = v[1]
        return r
    def text(name): return (d / (name + '.log')).read_text(encoding='utf-8', errors='replace')
    def chat(name): return '\n'.join(p.read_text(encoding='utf-8', errors='replace') for p in (d / name / 'chat').glob('*'))
    result = {'case': case, 'exits': [p.returncode for p in procs]}
    h, g = hashes('host'), hashes('guest')
    agreed = {f: v for f, v in h.items() if g.get(f) == v}
    result['players'] = {'host_frames': len(h), 'agreed': len(agreed), 'disagreed': sum(1 for f in h if f in g and g[f] != h[f])}
    ok = all(c == 0 for c in result['exits']) and result['players']['disagreed'] == 0 and len(agreed) >= frames - 60
    if spectators:
        for name in ['spec1', 'spec2']:
            s = hashes(name); common = [f for f in s if f in agreed]
            same = sum(1 for f in common if s[f] == agreed[f])
            result[name] = {'frames': len(s), 'compared': len(common), 'equal': same, 'first': min(s) if s else None}
            ok = ok and len(common) > 300 and same == len(common) and 'watch' in text(name).lower()
        result['chat'] = {
            'host_got_guest_lines': [chat('host').count(t) for t in ['sí, otra', 'gg']],
            'guest_got_host_lines': [chat('guest').count(t) for t in ['hola, ¿revancha?', '¡bien jugado!']],
            'players_got_spectator_line': [chat(n).count('qué partidazo') for n in ['host', 'guest']],
            'spectators_got_players_lines': [chat(n).count('¡bien jugado!') for n in ['spec1', 'spec2']],
            'host_log_names': [n in text('host') for n in ['Rodrigo', 'Ana']],
        }
        c = result['chat']
        ok = ok and c['host_got_guest_lines'] == [1, 1] and c['guest_got_host_lines'] == [1, 1] and all(c['host_log_names'])
    result['passed'] = bool(ok)
    (d / 'result.json').write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False), flush=True)
    return result, agreed

with_spec, agreed2 = run('with-spectators', 9650, True)
without, agreed0 = run('no-spectators', 9652, False)
common = set(agreed2) & set(agreed0)
same = all(agreed2[f] == agreed0[f] for f in common)
report = {'passed': with_spec['passed'] and without['passed'] and same and len(common) >= frames - 60,
          'players_same_with_and_without_spectators': same, 'compared_frames': len(common), 'frames': frames,
          'exe': str(exe), 'limits': ['one PC, loopback; no tunnel relay or Internet']}
(out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
print('PASSED' if report['passed'] else 'FAILED', out)
sys.exit(0 if report['passed'] else 1)
