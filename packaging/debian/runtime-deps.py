#!/usr/bin/env python3
"""Resolve complete ELF closure; fail rather than emit incomplete dependencies.
NKSHOT_PACKAGE_MAP is for the extracted official-deb test toolchain only.
"""
import json, os, pathlib, re, subprocess, sys
stage = pathlib.Path(sys.argv[1])
private = stage / 'opt/nkshot/lib/x86_64-linux-gnu'
env = dict(os.environ, LD_LIBRARY_PATH=str(private))
package_map = json.load(open(os.environ['NKSHOT_PACKAGE_MAP'])) if os.getenv('NKSHOT_PACKAGE_MAP') else None
packages = {}
for p in stage.rglob('*'):
    if p.is_symlink() or not p.is_file(): continue
    with p.open('rb') as f:
        if f.read(4) != b'\x7fELF': continue
    result = subprocess.run(['ldd', str(p)], env=env, text=True, capture_output=True)
    if result.returncode or 'not found' in result.stdout:
        raise SystemExit(f'Unresolved dependency in {p}: {result.stdout} {result.stderr}')
    for path in re.findall(r'(?:=>\s+|^\s*)(/\S+)', result.stdout, re.M):
        if path.startswith(str(stage)): continue
        resolved = str(pathlib.Path(path).resolve())
        if package_map:
            match = package_map.get(path) or package_map.get(resolved)
            if not match: raise SystemExit(f'No package ownership for {path}')
            name, version = match
        else:
            owners = subprocess.check_output(['dpkg-query','-S',resolved], text=True).splitlines()
            name = owners[0].split(': ')[0]
            version = subprocess.check_output(['dpkg-query','-W','-f=${Version}',name], text=True)
        packages[name.split(':')[0]] = version
required = ['qml6-module-qtcore','qml6-module-qt5compat-graphicaleffects','qml6-module-qtquick-window','qml6-module-qtquick','qml6-module-qtquick-controls','qml6-module-qtquick-dialogs',
 'qml6-module-qtquick-layouts','qml6-module-qtquick-shapes','qml6-module-qtquick-templates',
 'qml6-module-qtquick-effects','qml6-module-qtqml-workerscript','qml6-module-qtmultimedia',
 'qml6-module-org-kde-kirigami','qt6-wayland','qt6-svg-plugins','qt6-image-formats-plugins']
print(', '.join([f'{p} (>= {v})' for p,v in sorted(packages.items())] + [p for p in required if p not in packages]))
