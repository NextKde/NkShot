#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Explicit, network-using installation. Never invoked by the inference worker."""
import argparse
import hashlib
import os
import shlex
import json
from pathlib import Path, PurePosixPath
import shutil
import stat
import subprocess
import sys
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent

def download(record, folder, cache):
    target = folder / record['name']
    digest = hashlib.sha256()
    count = 0
    cached = cache / record['name'] if cache else None
    source = cached.open('rb') if cached and cached.is_file() else urllib.request.urlopen(record['url'], timeout=90)
    with source as response, target.open('xb') as stream:
        while chunk := response.read(1024 * 1024):
            count += len(chunk)
            if count > record['bytes']:
                raise RuntimeError('Download exceeds manifest size')
            digest.update(chunk)
            stream.write(chunk)
    if count != record['bytes'] or digest.hexdigest() != record['sha256']:
        raise RuntimeError('Download does not match manifest')

def install(destination, cache=None):
    # mkdir(exist_ok=False) also rejects existing files, directories and symlinks.
    destination.mkdir(parents=True, exist_ok=False, mode=0o700)
    marker = destination / 'INSTALLATION-INCOMPLETE'
    marker.write_text('Installation is incomplete. No previous directory was overwritten.\n')
    manifest = json.loads((HERE / 'manifest.json').read_text())
    downloads = destination / 'downloads'
    downloads.mkdir()
    for record in manifest['files']:
        print('Downloading verified model component:', record['name'], flush=True)
        download(record, downloads, cache)
    venv = destination / 'venv'
    subprocess.run([sys.executable, '-m', 'venv', str(venv)], check=True)
    python = venv / 'bin/python'
    subprocess.run([str(python), '-m', 'pip', 'install', '--disable-pip-version-check',
        '--only-binary=:all:', '--index-url', 'https://pypi.org/simple',
        *[name + '==' + version for name, version in manifest['packages'].items()]], check=True)
    model_root = destination / 'models'
    en, zh = model_root / 'en_zh', model_root / 'zh_en'
    en.mkdir(parents=True)
    zh.mkdir()
    prefix = 'translate-en_zh-1_9/'
    with zipfile.ZipFile(downloads / 'en_zh.argosmodel') as archive:
        entries = archive.infolist()
        if len(entries) > 10000 or sum(item.file_size for item in entries) > 1024 ** 3:
            raise RuntimeError('Archive exceeds safety limits')
        seen = set()
        for item in entries:
            path = PurePosixPath(item.filename)
            kind = stat.S_IFMT(item.external_attr >> 16)
            if (path.is_absolute() or '..' in path.parts or '\\' in item.filename
                or '\x00' in item.filename or kind not in (0, stat.S_IFREG, stat.S_IFDIR)
                or item.filename in seen or not item.filename.startswith(prefix)):
                raise RuntimeError('Unsafe model archive entry')
            seen.add(item.filename)
        # Only inference assets and original attribution; no unused Stanza models.
        for item in entries:
            relative = item.filename[len(prefix):]
            if not relative or item.is_dir():
                continue
            if not (relative.startswith('model/') or relative in ('sentencepiece.model', 'README.md', 'metadata.json')):
                continue
            relative = {'README.md': 'UPSTREAM-README.md', 'metadata.json': 'UPSTREAM-metadata.json'}.get(relative, relative)
            output = en / relative
            output.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(item) as source, output.open('xb') as target:
                shutil.copyfileobj(source, target)
    original = json.loads((en / 'UPSTREAM-metadata.json').read_text())
    if original.get('from_code') != 'en' or original.get('to_code') != 'zh':
        raise RuntimeError('Wrong model direction')
    subprocess.run([str(python), '-I', str(HERE / 'convert-tiny.py'), str(downloads), str(zh)], check=True)
    shutil.copyfile(downloads / 'tiny-vocab.spm', zh / 'sentencepiece.model')
    shutil.copyfile(downloads / 'tiny-README.md', zh / 'UPSTREAM-README.md')
    for direction, folder in [('en_zh', en), ('zh_en', zh)]:
        (folder / 'metadata.json').write_text(json.dumps({'runtimeSchema': 1, 'direction': direction}) + '\n')
    for name in ['translate.py', 'manifest.json', 'NOTICE.txt', 'LICENSE-tiny-Apache-2.0.txt', 'LICENSE-Argos-MIT.txt', 'LICENSE-model-CC-BY-4.0.html']:
        shutil.copyfile(HERE / name, destination / name)
    # Validate both local models while the incomplete marker still blocks the UI.
    subprocess.run([str(python), '-I', str(HERE / 'verify-models.py'), str(model_root)], check=True, timeout=60)
    launcher = destination / 'run-nkshot'
    launcher.write_text('#!/bin/sh\n' + ''.join('export NKSHOT_TRANSLATION_' + key + '=' + shlex.quote(str(value)) + '\n' for key, value in [('PYTHON', python), ('RUNNER', destination / 'translate.py'), ('MODEL_DIR', model_root)]) + 'exec nkshot "$@"\n')
    launcher.chmod(0o700)
    marker.unlink()
    print('\nInstallation complete. Default installation is detected automatically by NkShot.')
    print('For a custom location, launch: ' + str(launcher))
    print('Diagnostic configuration:')
    for key, value in [('PYTHON', python), ('RUNNER', destination / 'translate.py'), ('MODEL_DIR', model_root)]:
        print('NKSHOT_TRANSLATION_' + key + '=' + str(value))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    default = Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'nkshot/translation'
    parser.add_argument('--destination', default=str(default), help='New absolute directory (default: XDG data/nkshot/translation); never overwrites existing paths')
    parser.add_argument('--cache', type=Path, help='Optional local download cache; every byte is still verified against manifest')
    args = parser.parse_args()
    target = Path(args.destination).expanduser()
    if not target.is_absolute():
        parser.error('destination must be absolute')
    try:
        install(target, args.cache)
    except Exception as error:
        print('Installation failed (' + type(error).__name__ + '). Existing destinations are not overwritten. An incomplete new directory may remain.', file=sys.stderr)
        sys.exit(1)
