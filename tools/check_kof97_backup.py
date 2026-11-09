# SPDX-License-Identifier: GPL-3.0-only
"""Isolated Neo Geo native owner: backup/settings preservation and real menu rows."""
from pathlib import Path
import argparse,os,subprocess,shutil,json,hashlib
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for key in ['exe','fake-tunnel','seed-backup','rom','out']:p.add_argument('--'+key,type=Path,required=True)
p.add_argument("--game",choices=["kof97","kof2002","kf2k2mp2","kof98"],default="kof97")
a=p.parse_args();root=a.out.resolve();assert not root.exists() and root.is_relative_to((ROOT/'build').resolve());root.mkdir(parents=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
shutil.copy2(a.exe,root/(a.game+'.exe'));shutil.copy2(a.fake_tunnel,root/'cps-tunel.exe')
profile=root/'profile';profile.mkdir();before=a.seed_backup.read_bytes();assert len(before)==65536;(profile/'backup.bin').write_bytes(before)
settings=b'[features]\npreset=pure\n# keep this player note\n';(profile/'settings.ini').write_bytes(settings)
home=root/'home-Iñaki';home.mkdir();prefs='[online]\nservice=off\nname=Peña\n';(home/'launcher.ini').write_text(prefs,encoding='utf-8')
env=dict(os.environ);env.update(PATH='C:/msys64/ucrt64/bin;'+env['PATH'],CPS2RECOMP_HIDDEN_WINDOW='1',SDL_AUDIO_DRIVER='dummy',CPS2RECOMP_HOME=str(home),FORKADE_FAKE_CODE='A'*80,FORKADE_FAKE_MODE='normal',FORKADE_FAKE_ROOM=str(root),FORKADE_FAKE_RECORD_PID='1')
args=[root/(a.game+'.exe'),'--rom',a.rom,'--forkade-online','--settings',profile/'settings.ini','--eeprom',profile/'eeprom.bin','--window-events','2:host;80:copy;90:names;100:names;110:streaks;120:streaks;150:host;152:q','--shot-every','70','--shot-dir',root]
(root/'command.json').write_text(json.dumps(list(map(str,args)),indent=2)+'\n',encoding='utf-8')
with (root/'run.txt').open('wb') as f:q=subprocess.run(list(map(str,args)),env=env,stdout=f,stderr=subprocess.STDOUT,timeout=40,creationflags=subprocess.CREATE_NO_WINDOW)
text=(root/'run.txt').read_text(encoding='utf-8',errors='replace');assert q.returncode==0 and 'hosting=1 status=HOSTING' in text and 'Stopped hosting.' in text and 'copied=1' in text,text
assert (profile/'backup.bin').read_bytes()==before;assert (profile/'settings.ini').read_bytes()==settings;assert (home/'launcher.ini').read_text(encoding='utf-8')==prefs;assert len(list(root.glob('spawn-*.txt')))==1
(root/'report.json').write_text(json.dumps({'passed':True,'exe_sha256':sha(root/(a.game+'.exe')),'recipe_sha256':sha(Path(__file__)),'backup_sha256':hashlib.sha256(before).hexdigest(),'backup_bytes_preserved':True,'game_settings_preserved':True,'unicode_launcher_preferences_preserved':True,'owned_tunnel_instances':1,'exit_code':q.returncode},indent=2)+'\n',encoding='utf-8');print('PASS native backup/settings/code/toggles',flush=True)
