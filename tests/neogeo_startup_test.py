#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise actual executable discovery with invalid synthetic ZIPs; no ROMs required."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', required=True, type=Path)
    parser.add_argument('--game', required=True)
    parser.add_argument('--scratch', required=True, type=Path)
    args = parser.parse_args()
    original = args.exe.resolve()
    scratch = args.scratch.resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='startup-', dir=scratch)).resolve()
    assert root.is_relative_to(scratch)
    # Retain fixtures in the build directory for diagnosis. No user files are modified.
    name = args.game + '.zip'
    cases = [
        ('collection-ROM', ['exe:ROM'], 'exe:ROM', None),
        ('lowercase-rom', ['exe:rom'], 'exe:rom', None),
        ('legacy-roms', ['exe:roms'], 'exe:roms', None),
        ('beside-exe', ['exe:'], 'exe:', None),
        ('cwd-roms', ['cwd:roms'], 'cwd:roms', None),
        ('cwd-zip', ['cwd:'], 'cwd:', None),
        ('roms-before-ROM', ['exe:roms', 'exe:ROM', 'cwd:roms'], 'exe:roms', None),
        ('exe-before-cwd', ['exe:ROM', 'cwd:roms', 'cwd:'], 'exe:ROM', None),
        ('explicit-invalid-wins', ['exe:ROM'], None, 'missing-explicit.zip'),
        ('missing-set', [], None, None),
    ]
    for label, layouts, expected, explicit in cases:
        exe_dir, cwd = root/label/'program', root/label/'unrelated-cwd'
        exe_dir.mkdir(parents=True); cwd.mkdir()
        exe = exe_dir/original.name
        shutil.copy2(original, exe)
        for dll in original.parent.glob('*.dll'):
            shutil.copy2(dll, exe_dir/dll.name)
        def candidate(layout):
            place, subdir = layout.split(':', 1)
            return (exe_dir if place == 'exe' else cwd)/subdir/name
        for layout in layouts:
            path = candidate(layout); path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'not a zip: synthetic discovery fixture')
        command = [str(exe), '--headless', '--frames', '1']
        if explicit:
            command += ['--rom', str(cwd/explicit)]
        run = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=30)
        output = (run.stdout + run.stderr).replace('\\', '/')
        assert run.returncode == 1, (label, run.returncode, output)
        if expected:
            # The real loader names the selected invalid ZIP: selection, not a mirrored helper, is tested.
            place, subdir = expected.split(':', 1)
            path = candidate(expected).as_posix() if place == 'exe' else (Path(subdir)/name).as_posix()
            # Windows accepts ROM and rom as the same directory; case-sensitive hosts do not.
            assert os.path.normcase(path) in os.path.normcase(output) and 'is not a zip file' in output, (label, path, output)
        else:
            path = (cwd/explicit).as_posix() if explicit else (exe_dir/'roms'/name).as_posix()
            assert path in output and 'there is no' in output, (label, path, output)
        print('PASS', label)
    print('Startup discovery passed; fixtures:', root)


if __name__ == '__main__':
    main()
