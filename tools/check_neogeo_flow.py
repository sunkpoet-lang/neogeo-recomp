# SPDX-License-Identifier: GPL-3.0-only
"""Per-game Neo Geo local acceptance; immutable outputs, no deployment or player-data writes."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse,hashlib,json,os,shutil,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for k in ['forkade-root','baseline','rom-dir','out']:p.add_argument('--'+k,type=Path,required=True)
p.add_argument("--game",choices=["kof97","kof2002","kf2k2mp2","kof98"],required=True)
a=p.parse_args();game=a.game;out=a.out.resolve();product=a.forkade_root.resolve();runtime=Path('C:/msys64/ucrt64/bin')
assert not out.exists() and out.is_relative_to((ROOT/'build').resolve());out.mkdir(parents=True)
sys.path.insert(0,str(product));from tools.ki_setup.game_setup import OwnedJob
job=OwnedJob();sys.path.insert(0,str(product/'tools/cps'));from recovery_proxy import OutageProxy
rom=a.rom_dir/(game+'.zip');exe=out/(game+'.exe');shutil.copy2(ROOT/'build'/game/(game+'.exe'),exe)
env=dict(os.environ);env.update(PATH=str(runtime)+';'+env['PATH'],CPS2RECOMP_HIDDEN_WINDOW='1',SDL_AUDIO_DRIVER='dummy');env.pop('CPS2RECOMP_HOME',None)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,data):p.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
from pe_icons import icons
old_icons=icons(a.baseline);new_icons=icons(exe);assert old_icons==new_icons
save(out/'icon.json',{'passed':True,'embedded_icon_resources_identical':True,'resources':new_icons})
records=[{'case':'embedded-icon-resources-identical','passed':True}];save(out/'provenance.json',{'base':subprocess.check_output(['git','-C',ROOT,'rev-parse','HEAD']).decode().strip(),'exe_sha256':sha(exe),'baseline_sha256':sha(a.baseline),'recipe_sha256':sha(Path(__file__)),'compiler':subprocess.check_output([runtime/'g++.exe','--version']).decode().splitlines()[0]})
files=subprocess.check_output(['git','-C',ROOT,'ls-files','-co','--exclude-standard','-z']).decode().split('\0');source={f:sha(ROOT/f) for f in set(files) if (ROOT/f).is_file() and (Path(f).suffix in ['.cpp','.h','.cfg','.cmake'] or f=='CMakeLists.txt')};save(out/'source.json',source)
def run(label,args,timeout=300):
 print('CHECK '+label,flush=True)
 with (out/(label+'.txt')).open('wb') as f:q=subprocess.run(list(map(str,args)),env=env,stdout=f,stderr=subprocess.STDOUT,timeout=timeout,creationflags=subprocess.CREATE_NO_WINDOW)
 save(out/(label+'-exit.json'),{'exit_code':q.returncode});assert q.returncode==0,(label,(out/(label+'.txt')).read_text(encoding='utf-8',errors='replace')[-2000:]);records.append({'case':label,'passed':True});print('PASS '+label,flush=True)
def profile(d):return ['--settings',d/'settings.ini','--eeprom',d/'eeprom.bin']
def epochs(p):
 r={1:{}};e=1
 for line in p.read_text(encoding='utf-8').splitlines():
  if line.startswith('# epoch '):e=int(line.split()[-1]);r.setdefault(e,{});continue
  v=line.split()
  if v:r[e][int(v[0])]=tuple(v[1:])
 return r
def wait(d,phrase,q,seconds=20):
 until=time.monotonic()+seconds
 while phrase not in (d/'run.txt').read_text(encoding='utf-8',errors='replace'):
  assert q.poll() is None and time.monotonic()<until,(phrase,(d/'run.txt').read_text(encoding='utf-8',errors='replace')[-1000:]);time.sleep(.03)
def spawn(d,program,flags,frames,children,logs,extra=None,gui=False):
 d.mkdir();f=(d/'run.txt').open('wb');logs.append(f)
 args=[program,'--rom',rom,*([] if gui else ['--headless']),*flags,'--frames',str(frames),*profile(d)]
 if program==exe:args+=['--gekko-checkpoint-log',d/'state.hash','--gekko-flow-log',d/'flow.txt']
 save(d/'command.json',list(map(str,args)));current=dict(env);current.update(extra or {})
 q=subprocess.Popen(list(map(str,args)),env=current,stdout=f,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW);children.append(q);return q
def cleanup(children,logs):
 for q in children:
  if q.poll() is None:q.kill();q.wait()
 for f in logs:f.close()
def compare(d,epoch=1,min_count=300):
 states=[epochs(d/s/'state.hash')[epoch] for s in ['host','guest']];flows=[epochs(d/s/'flow.txt')[epoch] for s in ['host','guest']]
 end=min(max(s) for s in states)-32;keys={f for f in states[0].keys()&states[1].keys() if f<=end};assert len(keys)>=min_count,(d,len(keys))
 assert all(states[0][f]==states[1][f] and flows[0][f]==flows[1][f] for f in keys),str(d)+' checkpoint mismatch'
 return len(keys)
run('menu-unit-build',[runtime/'g++.exe','-std=c++20',ROOT/'tests/kof97_menu_test.cpp',ROOT/'port/menu.cpp',ROOT/'recomp-common/menu.cpp','-o',out/'menu.exe']);run('menu-unit',[out/'menu.exe'])
run('score-unit-build',[runtime/'g++.exe','-std=c++20','-Wall','-Wextra','-Werror',ROOT/('tests/'+game+'_match_score_test.cpp'),'-o',out/'score.exe']);run('score-unit',[out/'score.exe'])
run('recovery-unit-build',[runtime/'g++.exe','-std=c++20','-Wall','-Wextra','-Werror','-I',ROOT/'port',product/'tests/recovery.cpp','-o',out/'recovery.exe']);run('recovery-unit',[out/'recovery.exe'])
run('fake-build',[runtime/'g++.exe','-std=c++17','-Wall','-Wextra','-Werror','-I',product/'launcher','-I',ROOT/'recomp-common/launcher/src',product/'tests/broker_fake_tunnel.cpp','-lws2_32','-lbcrypt','-o',out/'cps-tunel.exe'])
script='300:coin1:8,420:start1:8,900:a1:6,1300:start1:8,1800:right1:60,1900:a1:6,2000:b1:6,2100:c1:6,2200:d1:6,2600:down1:30,2700:a1:6,3500:left1:80,3600:c1:6,4200:start1:8,4300:coin2:8,4400:start2:8,5000:a1:6,5100:b2:6'
for side,program in [('before',a.baseline),('after',exe)]:
 d=out/side;d.mkdir();run('offline-'+side,[program,'--rom',rom,'--headless','--frames','6000','--script',script,*profile(d),'--hash-log',d/'state.hash','--audio-dump',d/'audio.wav'])
for f in ['state.hash','audio.wav']:assert (out/'before'/f).read_bytes()==(out/'after'/f).read_bytes()
assert len((out/'after/state.hash').read_text(encoding='utf-8').splitlines())==6000
records.append({'case':'offline6000-state-audio-identical','passed':True})
trace='1600:coin1:8,1620:coin2:8,1800:start1:8,1820:start2:8,'+','.join(f'{n}:a{p}:3' for n in range(2300,4300,100) for p in [1,2])+',2400:right1:3,2400:right2:3,2600:right1:3,2600:right2:3,4500:right1:30000,16100:coin2:8,16200:start2:8,'+','.join(f'{n}:a{p}:3' for n in range(16700,18700,100) for p in [1,2])+',16800:right1:3,16800:right2:3,17000:right1:3,17000:right2:3,'+','.join(f'{n}:d1:3' for n in range(4600,34000,35))
d=out/'stress';d.mkdir();run('stress6000',[exe,'--rom',rom,'--headless','--forkade-online','--stress','--frames','6000','--script',trace,*profile(d),'--gekko-checkpoint-log',d/'state.hash','--gekko-flow-log',d/'flow.txt'],timeout=600)
flow=epochs(d/'flow.txt')[1];assert max(flow)>=5990 and any(v[-1]=='1' for v in flow.values())
records.append({'case':'stress6000-human-fight-armed','passed':True})
d=out/'teams';d.mkdir();run('two-team-matches',[exe,'--rom',rom,'--headless','--forkade-online','--observe-flow','--frames','34000','--script',trace,*profile(d),'--gekko-flow-log',d/'flow.txt'],timeout=360)
flow=epochs(d/'flow.txt')[1];streak=[int(v[2]) for v in flow.values()];assert len(flow)==34000 and max(streak)==2 and min(streak)==0
records.append({'case':'two-real-team-matches-streak2','passed':True,'observer_frames':34000,'last_streak':streak[-1]})
for n in range(20):
 d=out/('pair-%02d'%n);d.mkdir();children=[];logs=[]
 try:
  h=spawn(d/'host',exe,['--forkade-online','--host','8910','--nick','Host-One','--script','50:start1:3','--net-say','80:Hello'],440,children,logs)
  wait(d/'host','WAITING FOR A CHALLENGER',h)
  if n==0:time.sleep(1)
  g=spawn(d/'guest',exe,['--forkade-online','--join','127.0.0.1:8910','--nick','Guest-Two','--script','100:start1:3'],400,children,logs)
  g.wait(timeout=45);h.wait(timeout=20);save(d/'exits.json',[h.returncode,g.returncode]);assert h.returncode==g.returncode==0
 finally:cleanup(children,logs)
 count=compare(d);records.append({'case':'pair'+str(n+1),'passed':True,'matching_checkpoints':count});save(out/'progress.json',records);print('PASS pair '+str(n+1)+'/20',flush=True)
 if n==0:assert 'arcade reset after ' in (d/'host/run.txt').read_text(encoding='utf-8')
def outage(mode,port,frames=3720,native=False):
 label='native-owner' if native else mode;d=out/label;d.mkdir();children=[];logs=[];proxy=OutageProxy(port,7845 if native else port+1,mode)
 began=time.monotonic();print('CHECK outage '+label,flush=True)
 try:
  hostflags=['--forkade-online','--host',str(port+1),'--nick','Host-One','--net-say','60:Before gap','--chat-log',d/'host-chat']
  extra=None
  if native:
   hostflags=['--forkade-online','--forkade-service','off','--nick','Host-One','--net-say','60:Before gap','--chat-log',d/'host-chat','--window-events','2:host;180:copy;220:names;260:names;270:streaks;290:streaks;2300:host;2400:q','--shot-every','200','--shot-dir',d]
   home=d/'home-Iñaki';home.mkdir();(home/'launcher.ini').write_text('[online]\nservice=off\nname=Host-One\n',encoding='utf-8');extra={'CPS2RECOMP_HOME':str(home),'FORKADE_FAKE_CODE':'A'*80,'FORKADE_FAKE_MODE':'normal','FORKADE_FAKE_ROOM':str(d),'FORKADE_FAKE_LIFETIME_MS':'120000','FORKADE_FAKE_RECORD_PID':'1'}
  h=spawn(d/'host',exe,hostflags,0 if native else frames+30,children,logs,extra,native);wait(d/'host','WAITING FOR A CHALLENGER',h)
  g=spawn(d/'guest',exe,['--forkade-online','--join','127.0.0.1:'+str(port),'--nick','Guest-Two','--net-say','180:Pending across gap','--chat-log',d/'guest-chat','--script','50:start1:3'],frames,children,logs)
  g.wait(timeout=160 if mode!='forever' else 100);h.wait(timeout=60 if native else 30);codes=[h.returncode,g.returncode];save(d/'exits.json',codes)
 finally:cleanup(children,logs);proxy.close()
 assert not proxy.errors,proxy.errors
 text=[(d/s/'run.txt').read_text(encoding='utf-8',errors='replace') for s in ['host','guest']];assert proxy.stats['triggered']
 if mode=='forever':assert 3 in codes and any('Reconnection timed out after 60 seconds' in t for t in text)
 else:
  assert codes==[0,0],(codes,text)
  assert all('recovery playing epoch 2' in t and 'desync' not in t.lower() for t in text)
  count=compare(d,2,450 if native else 3600)
  initial=epochs(d/'guest/state.hash');keys=set(initial[1])&set(initial[2]);end=min(max(initial[1]),max(initial[2]))-32;keys={f for f in keys if f<=end};assert keys and all(initial[1][f]==initial[2][f] for f in keys)
  msgs=[json.loads(l) for f in (d/'host-chat').glob('*.jsonl') for l in f.read_text(encoding='utf-8').splitlines()];assert sum(r.get('text')=='Pending across gap' and r.get('dir')=='received' for r in msgs)==1
  assert proxy.stats['stale_injections']>=3
  if native:
   assert len(list(d.glob('spawn-*.txt')))==1
   assert 'Waiting for the same player. Room kept open.' in text[0] and 'Stopped hosting.' in text[0]
 result={'case':label,'passed':True,'elapsed_seconds':round(time.monotonic()-began,2),'proxy':proxy.stats,'exits':codes}
 if mode!='forever':result['matching_recovered_checkpoints']=count
 save(d/'report.json',result);print('PASS outage '+label,flush=True);return result
with ThreadPoolExecutor(max_workers=2) as pool:
 one=pool.submit(outage,'both',8920);two=pool.submit(outage,'one-way',8922);records.extend([one.result(),two.result()])
with ThreadPoolExecutor(max_workers=2) as pool:
 one=pool.submit(outage,'forever',8924);two=pool.submit(outage,'both',8926,2300,True);records.extend([one.result(),two.result()])
# Verify default wire in both directions without giving the old EXE new flags.
for old_host in [False,True]:
 d=out/('old-host' if old_host else 'old-guest');d.mkdir();children=[];logs=[]
 try:
  h=spawn(d/'host',a.baseline if old_host else exe,['--host','8930'],500,children,logs);wait(d/'host','Waiting for the other player',h)
  g=spawn(d/'guest',exe if old_host else a.baseline,['--join','127.0.0.1:8930'],480,children,logs);g.wait(timeout=40);h.wait(timeout=30);assert h.returncode==g.returncode==0
 finally:cleanup(children,logs)
 assert all('online: playing' in (d/s/'run.txt').read_text(encoding='utf-8') and 'desync' not in (d/s/'run.txt').read_text(encoding='utf-8').lower() for s in ['host','guest'])
 records.append({'case':d.name+'-legacy-wire','passed':True})
assert sha(exe)==json.loads((out/'provenance.json').read_text(encoding='utf-8'))['exe_sha256']
assert all(sha(ROOT/f)==value for f,value in source.items())
save(out/'report.json',{'passed':True,'game':game,'checks':records,'source_unchanged':True,'limits':['Loopback peers; synthetic native owner tunnel, no Internet/Iroh-game end-to-end','Only this game is certified by this execution','Installed beta and source pins unchanged']})
print('PASS '+game+' acceptance '+str(out),flush=True)
