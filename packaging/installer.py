#!/usr/bin/env python3
"""NkShot local installer. Detects dependencies; never installs them and never invokes sudo."""
import sys, os, json, re, shutil, subprocess, tempfile, pathlib, platform, fcntl, uuid, signal
from datetime import datetime, timezone

APP = 'org.nextkde.nkshot'
REPO = 'https://github.com/NextKde/NkShot.git'
SLOT_DIR = 'nkshot-app'
BINARY = 'nkshot'
DESKTOP_NAME = 'org.nextkde.nkshot.desktop'
QML_STYLE = 'org.kde.breeze'

CODES = {'INVALID_ARGUMENT':2, 'UNSUPPORTED_PLATFORM':10, 'DEPENDENCY_UNSATISFIED':11,
         'PERMISSION_DENIED':12, 'INSTALL_BUSY':13, 'DOWNLOAD_FAILED':20,
         'INTEGRITY_FAILED':21, 'BUILD_FAILED':30, 'INSTALL_FAILED':31,
         'UNINSTALL_FAILED':32, 'INTERNAL_ERROR':99, 'CANCELLED':130}

# Files the running application must find under the XDG user data/lib prefixes.
# Every entry is linked from ~/.local through the slot's `current` symlink.
MIRRORED = [
    'share/applications/' + DESKTOP_NAME,
    'share/icons/hicolor/scalable/apps/nkshot.svg',
    'share/knotifications6/nkshot.notifyrc',
    'share/config.kcfg/nkshot.kcfg',
    'share/metainfo/org.nextkde.nkshot.appdata.xml',
    'share/dbus-1/services/org.nextkde.nkshot.service',
    'share/dbus-1/interfaces/org.nextkde.nkshot.xml',
    'share/kglobalaccel/' + DESKTOP_NAME,
    'lib/systemd/user/app-org.nextkde.nkshot.service',
]

PROBE_CMAKE = r"""cmake_minimum_required(VERSION 3.21)
project(NkShotDependencyProbe LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(_missing "${CMAKE_BINARY_DIR}/missing.txt")
file(WRITE "${_missing}" "")

find_package(ECM 6.13 QUIET NO_MODULE)
if(NOT ECM_FOUND)
  file(APPEND "${_missing}" "ecm|>=6.13.0||extra-cmake-modules\n")
else()
  set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
endif()

find_package(Qt6 6.8 CONFIG QUIET COMPONENTS Core Concurrent Qml Quick QuickControls2 QuickDialogs2 QuickTemplates2 Widgets DBus PrintSupport WaylandClient Multimedia)
if(NOT Qt6_FOUND)
  file(APPEND "${_missing}" "qt6|>=6.8.0||Qt6 (Core Concurrent Qml Quick QuickControls2 QuickDialogs2 QuickTemplates2 Widgets DBus PrintSupport WaylandClient Multimedia)\n")
endif()

# Qt < 6.10 builds Wayland only, so the private X11 headers are not required there.
if(Qt6Gui_VERSION VERSION_GREATER_EQUAL 6.10.0)
  find_package(Qt6GuiPrivate 6.10 QUIET NO_MODULE)
  if(NOT Qt6GuiPrivate_FOUND)
    file(APPEND "${_missing}" "qt6-gui-private|>=6.10.0||Qt6 Gui private headers (X11 backend)\n")
  endif()
endif()

foreach(_component CoreAddons WidgetsAddons DBusAddons Notifications Config I18n KIO WindowSystem GlobalAccel XmlGui GuiAddons KirigamiPlatform StatusNotifierItem Prison Crash)
  find_package(KF6${_component} 6.13 QUIET)
  if(NOT KF6${_component}_FOUND)
    string(TOLOWER "${_component}" _lower)
    file(APPEND "${_missing}" "kf6-${_lower}|>=6.13.0||KF6${_component}\n")
  endif()
endforeach()

find_package(Wayland QUIET COMPONENTS Client)
if(NOT Wayland_FOUND)
  file(APPEND "${_missing}" "wayland|present||Wayland client libraries\n")
endif()

find_package(PlasmaWaylandProtocols QUIET)
if(NOT PlasmaWaylandProtocols_FOUND)
  file(APPEND "${_missing}" "plasma-wayland-protocols|present||PlasmaWaylandProtocols\n")
endif()

find_package(LayerShellQt QUIET)
if(NOT LayerShellQt_FOUND)
  file(APPEND "${_missing}" "layer-shell-qt|present||LayerShellQt\n")
endif()

# KQuickImageEditor is vendored under third_party/, so only its own requirement,
# Google Highway, has to be present on the system.
find_package(HWY 1.3.0 QUIET)
if(NOT HWY_FOUND)
  file(APPEND "${_missing}" "highway|>=1.3.0||Google Highway (libhwy), needed by the vendored KQuickImageEditor\n")
endif()

find_package(XCB QUIET COMPONENTS XFIXES IMAGE UTIL CURSOR RANDR)
if(NOT XCB_FOUND)
  file(APPEND "${_missing}" "xcb|present||XCB (XFIXES IMAGE UTIL CURSOR RANDR)\n")
endif()

find_package(KPipeWire 6.4.0 QUIET)
if(NOT KPipeWire_FOUND)
  file(APPEND "${_missing}" "kpipewire|>=6.4.0||KPipeWire\n")
endif()

file(WRITE "${CMAKE_BINARY_DIR}/qmake.txt" "")
if(TARGET Qt6::qmake)
  get_target_property(_qmake Qt6::qmake IMPORTED_LOCATION)
  file(WRITE "${CMAKE_BINARY_DIR}/qmake.txt" "${_qmake}")
endif()

add_executable(probe_cxx20 main.cpp)
"""

action = 'unknown'
stage = 'arguments'
log_path = None
log = None
lock_handle = None

# Package names per distribution family. Every entry below was checked against
# the distribution's own package index (Arch `extra`, Debian madison /
# packages.debian.org, Fedora mdapi). A family left out means "not verified":
# the report then falls back to a search hint instead of guessing a name.
PACKAGE_NAMES = {
    'cmake': {'arch': ['cmake'], 'debian': ['cmake'], 'fedora': ['cmake']},
    'git': {'arch': ['git'], 'debian': ['git'], 'fedora': ['git']},
    'pkg-config': {'arch': ['pkgconf'], 'debian': ['pkg-config'], 'fedora': ['pkgconf']},
    'cxx': {'arch': ['gcc'], 'debian': ['g++'], 'fedora': ['gcc-c++']},
    'make': {'arch': ['make'], 'debian': ['make'], 'fedora': ['make']},
    'ecm': {'arch': ['extra-cmake-modules'], 'debian': ['extra-cmake-modules'],
            'fedora': ['extra-cmake-modules']},
    'qt6': {'arch': ['qt6-base', 'qt6-declarative', 'qt6-wayland', 'qt6-multimedia'],
            'debian': ['qt6-base-dev', 'qt6-declarative-dev', 'qt6-wayland-dev', 'qt6-multimedia-dev'],
            'fedora': ['qt6-qtbase-devel', 'qt6-qtdeclarative-devel', 'qt6-qtwayland-devel',
                       'qt6-qtmultimedia-devel']},
    'qt6-gui-private': {'arch': ['qt6-base'], 'debian': ['qt6-base-private-dev'],
                        'fedora': ['qt6-qtbase-private-devel']},
    'kf6-coreaddons': {'arch': ['kcoreaddons'], 'debian': ['libkf6coreaddons-dev'],
                       'fedora': ['kf6-kcoreaddons-devel']},
    'kf6-widgetsaddons': {'arch': ['kwidgetsaddons'], 'debian': ['libkf6widgetsaddons-dev'],
                          'fedora': ['kf6-kwidgetsaddons-devel']},
    'kf6-dbusaddons': {'arch': ['kdbusaddons'], 'debian': ['libkf6dbusaddons-dev'],
                       'fedora': ['kf6-kdbusaddons-devel']},
    'kf6-notifications': {'arch': ['knotifications'], 'debian': ['libkf6notifications-dev'],
                          'fedora': ['kf6-knotifications-devel']},
    'kf6-config': {'arch': ['kconfig'], 'debian': ['libkf6config-dev'], 'fedora': ['kf6-kconfig-devel']},
    'kf6-i18n': {'arch': ['ki18n'], 'debian': ['libkf6i18n-dev'], 'fedora': ['kf6-ki18n-devel']},
    'kf6-kio': {'arch': ['kio'], 'debian': ['libkf6kio-dev'], 'fedora': ['kf6-kio-devel']},
    'kf6-windowsystem': {'arch': ['kwindowsystem'], 'debian': ['libkf6windowsystem-dev'],
                         'fedora': ['kf6-kwindowsystem-devel']},
    'kf6-globalaccel': {'arch': ['kglobalaccel'], 'debian': ['libkf6globalaccel-dev'],
                        'fedora': ['kf6-kglobalaccel-devel']},
    'kf6-xmlgui': {'arch': ['kxmlgui'], 'debian': ['libkf6xmlgui-dev'], 'fedora': ['kf6-kxmlgui-devel']},
    'kf6-guiaddons': {'arch': ['kguiaddons'], 'debian': ['libkf6guiaddons-dev'],
                      'fedora': ['kf6-kguiaddons-devel']},
    # Debian's Kirigami 6 development package name could not be verified.
    'kf6-kirigamiplatform': {'arch': ['kirigami'], 'fedora': ['kf6-kirigami-devel']},
    'kf6-statusnotifieritem': {'arch': ['kstatusnotifieritem'],
                               'debian': ['libkf6statusnotifieritem-dev'],
                               'fedora': ['kf6-kstatusnotifieritem-devel']},
    'kf6-prison': {'arch': ['prison'], 'debian': ['libkf6prison-dev'], 'fedora': ['kf6-prison-devel']},
    'kf6-crash': {'arch': ['kcrash'], 'debian': ['libkf6crash-dev'], 'fedora': ['kf6-kcrash-devel']},
    'wayland': {'arch': ['wayland'], 'debian': ['libwayland-dev'], 'fedora': ['wayland-devel']},
    'plasma-wayland-protocols': {'arch': ['plasma-wayland-protocols'],
                                 'debian': ['plasma-wayland-protocols'],
                                 'fedora': ['plasma-wayland-protocols-devel']},
    'layer-shell-qt': {'arch': ['layer-shell-qt'], 'debian': ['liblayershellqtinterface-dev'],
                       'fedora': ['layer-shell-qt-devel']},
    'xcb': {'arch': ['libxcb'], 'debian': ['libxcb1-dev'], 'fedora': ['libxcb-devel']},
    'kpipewire': {'arch': ['kpipewire'], 'debian': ['libkpipewire-dev'], 'fedora': ['kpipewire-devel']},
    'highway': {'arch': ['highway'], 'debian': ['libhwy-dev'], 'fedora': ['highway-devel']},
    'tesseract': {'arch': ['tesseract'], 'debian': ['libtesseract-dev'], 'fedora': ['tesseract-devel']},
}

INSTALL_COMMANDS = {'arch': 'sudo pacman -S', 'debian': 'sudo apt install', 'fedora': 'sudo dnf install'}
SEARCH_COMMANDS = {'arch': 'pacman -Ss', 'debian': 'apt search', 'fedora': 'dnf search'}


def release_family():
    """Map the running distribution onto a package-manager family."""
    release = {}
    try:
        for line in pathlib.Path('/etc/os-release').read_text().splitlines():
            if '=' in line:
                key, value = line.split('=', 1)
                release[key] = value.strip('"')
    except OSError:
        return None
    candidates = [release.get('ID', '')] + release.get('ID_LIKE', '').split()
    known = {
        'arch': {'arch', 'archarm', 'manjaro', 'endeavouros', 'cachyos', 'artix'},
        'debian': {'debian', 'ubuntu', 'linuxmint', 'pop', 'elementary', 'zorin', 'kali', 'devuan'},
        'fedora': {'fedora', 'rhel', 'centos', 'rocky', 'almalinux'},
    }
    for candidate in candidates:
        for family, members in known.items():
            if candidate in members:
                return family
    return None


def describe_missing(missing):
    """Attach verified package names and a copy-pasteable install command."""
    family = release_family()
    packages = []
    for item in missing:
        names = PACKAGE_NAMES.get(item['id'], {}).get(family) if family else None
        item['packages'] = names or []
        for name in item['packages']:
            if name not in packages:
                packages.append(name)
    details = {'dependencies': missing}
    if family is None:
        details['searchHint'] = ('Unrecognised distribution. Install the development packages that '
                                 'provide the components listed above.')
        return details
    details['distribution'] = family
    if packages:
        details['installCommand'] = INSTALL_COMMANDS[family] + ' ' + ' '.join(packages)
    unresolved = [item['name'] for item in missing if not item['packages']]
    if unresolved:
        details['searchHint'] = (SEARCH_COMMANDS[family] + ' <keyword> — no verified package name for: '
                                 + '; '.join(unresolved))
    return details



class Failure(Exception):
    def __init__(self, code, message, details):
        self.code, self.message, self.details = code, message, details


def emit(**fields):
    print(json.dumps(dict(schemaVersion=1, type='result', appId=APP, action=action, **fields)), flush=True)


def fail(code, message, **details):
    raise Failure(code, message, details)


def progress(name):
    global stage
    stage = name
    print(json.dumps(dict(schemaVersion=1, type='progress', appId=APP, action=action, stage=name)), flush=True)


def run(args, cwd=None, code='DEPENDENCY_UNSATISFIED', timeout=1800):
    try:
        completed = subprocess.run(args, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   text=True, timeout=timeout)
        if log:
            log.write(completed.stdout)
            log.flush()
        if completed.returncode:
            fail(code, 'Command failed; see the installation log.', command=args[0])
        return completed.stdout.strip()
    except subprocess.TimeoutExpired:
        fail(code, 'Command timed out.', command=args[0])


def atomic_json(path, value):
    tmp = path.with_name(path.name + '.tmp-' + uuid.uuid4().hex)
    tmp.write_text(json.dumps(value, indent=2) + '\n')
    os.replace(tmp, path)


def atomic_link(path, target):
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + '.tmp-' + uuid.uuid4().hex)
    tmp.symlink_to(target)
    os.replace(tmp, path)


def dependencies(work):
    """Report what is missing. This function never installs anything."""
    missing = []

    def add(identifier, required, installed=None, reason=None, name=None):
        missing.append(dict(id=identifier, name=name or identifier, required=required,
                            installed=installed,
                            reason=reason or ('missing' if installed is None else 'version_too_old')))

    def version(tool, identifier, required, args):
        if not shutil.which(tool):
            add(identifier, '>=' + required)
            return
        try:
            value = run([tool] + args)
        except Failure:
            add(identifier, '>=' + required, 'unreadable')
            return
        match = re.search(r'(\d+)\.(\d+)(?:\.(\d+))?', value)
        if not match or tuple(int(x or 0) for x in match.groups()) < tuple(map(int, required.split('.'))):
            add(identifier, '>=' + required, value.splitlines()[0] if value else 'unknown')

    version('cmake', 'cmake', '3.21.0', ['--version'])
    for tool, identifier in (('git', 'git'), ('pkg-config', 'pkg-config'),
                             ('c++', 'cxx'), ('make', 'make')):
        if not shutil.which(tool):
            add(identifier, 'present')

    if shutil.which('pkg-config'):
        if subprocess.run(['pkg-config', '--exists', 'tesseract'],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode:
            add('tesseract', 'present', name='libtesseract (OCR), via pkg-config')

    if not all(shutil.which(x) for x in ('cmake', 'c++', 'make')):
        return missing

    probe = work / 'probe'
    probe.mkdir(parents=True, exist_ok=True)
    (probe / 'CMakeLists.txt').write_text(PROBE_CMAKE)
    (probe / 'main.cpp').write_text('#include <span>\nint main() { int values[1]{}; return std::span<int>(values)[0]; }\n')
    try:
        run(['cmake', '-S', str(probe), '-B', str(probe / 'build'), '-G', 'Unix Makefiles'])
        run(['cmake', '--build', str(probe / 'build')])
    except Failure:
        add('cxx20', 'working configure and compile', 'probe failed', 'version_too_old',
            name='C++20 + CMake toolchain')
        return missing

    report = probe / 'build' / 'missing.txt'
    if report.exists():
        for line in report.read_text().splitlines():
            if line.count('|') != 3:
                continue
            identifier, required, installed, name = line.split('|', 3)
            add(identifier, required, installed or None,
                'version_too_old' if installed else 'missing', name=name)
    return missing


def quote_exec_path(path):
    """Quote a filesystem path for a desktop Exec key (percent signs are literal there)."""
    value = path.replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$')
    return '"' + value.replace('%', '%%') + '"'


def rewrite_desktop_exec(entry, launcher):
    """Point every Exec/TryExec of the generated desktop file at the installed slot."""
    quoted = quote_exec_path(launcher)
    wrapper = 'env QT_QUICK_CONTROLS_STYLE=' + QML_STYLE + ' ' + quoted

    def replace_exec(match):
        tokens = match.group(2).split()
        arguments = ' '.join(tokens[1:])
        return match.group(1) + wrapper + ((' ' + arguments) if arguments else '')

    text = entry.read_text()
    text = re.sub(r'^(Exec=)(.*)$', replace_exec, text, flags=re.M)
    text = re.sub(r'^(TryExec=)(.*)$', lambda _: 'TryExec=' + quoted, text, flags=re.M)
    entry.write_text(text)


def rewrite_service_exec(service, key, launcher, arguments):
    quoted = '"' + launcher.replace('\\', '\\\\').replace('"', '\\"') + '"'
    text = service.read_text()
    service.write_text(re.sub(r'^(%s=).*$' % re.escape(key),
                              lambda _: key + '=' + quoted + arguments,
                              text, flags=re.M))


def validate_local_source(source):
    top = run(['git', '-C', str(source), 'rev-parse', '--show-toplevel'], code='INTEGRITY_FAILED')
    if pathlib.Path(top).resolve() != source:
        fail('INVALID_ARGUMENT', '--source-dir must point to the repository root.')
    for name in ['CMakeLists.txt', 'src/CMakeLists.txt', 'desktop/' + DESKTOP_NAME + '.cmake', 'packaging/installer.py']:
        if not (source / name).is_file():
            fail('INTEGRITY_FAILED', 'Required local source file is missing.', path=name)
    revision = run(['git', '-C', str(source), 'rev-parse', 'HEAD'], code='INTEGRITY_FAILED')
    if not re.fullmatch(r'[0-9a-f]{40}', revision):
        fail('INTEGRITY_FAILED', 'Local source has no committed revision.')
    dirty = bool(run(['git', '-C', str(source), 'status', '--porcelain', '--untracked-files=no'],
                     code='INTEGRITY_FAILED'))
    return revision, dirty


def main():
    global action, log_path, log, lock_handle
    args = sys.argv[1:]
    if not args or args[0] not in ['check', 'install', 'status', 'uninstall']:
        fail('INVALID_ARGUMENT', 'Expected check, install, status or uninstall.')
    action = args.pop(0)
    revision = None
    local_source = None
    if args:
        if action == 'install' and len(args) == 2 and args[0] == '--version':
            revision = args[1]
        elif action == 'install' and len(args) == 2 and args[0] == '--source-dir':
            local_source = pathlib.Path(args[1]).expanduser().resolve()
            if not local_source.is_dir():
                fail('INVALID_ARGUMENT', '--source-dir must be an existing repository directory.')
        else:
            fail('INVALID_ARGUMENT', 'Unexpected arguments.')
    if action == 'install' and local_source is None and (revision is None or not re.fullmatch(r'[0-9a-f]{40}', revision)):
        fail('INVALID_ARGUMENT', '--version must be the exact 40-character Git commit SHA.')
    if sys.version_info < (3, 9):
        fail('DEPENDENCY_UNSATISFIED', 'Python 3.9 or newer is required.',
             dependencies=[dict(id='python3', required='>=3.9', installed=platform.python_version(), reason='version_too_old')])
    if os.geteuid() == 0:
        fail('PERMISSION_DENIED', 'Run as the target desktop user, not root.')
    if platform.machine() not in ['x86_64', 'aarch64']:
        fail('UNSUPPORTED_PLATFORM', 'Supported architectures: x86_64, aarch64.')

    home = pathlib.Path.home()

    def xdg(name, fallback):
        value = pathlib.Path(os.environ.get(name, str(home / fallback)))
        if not value.is_absolute():
            fail('INVALID_ARGUMENT', name + ' must be an absolute path.')
        return value

    data = xdg('XDG_DATA_HOME', '.local/share')
    state = xdg('XDG_STATE_HOME', '.local/state') / 'nkshot-installer'
    root = data / SLOT_DIR
    current = root / 'current'
    versions = root / 'versions'
    manifest = root / 'manifest.json'

    state.mkdir(parents=True, exist_ok=True)
    lock_handle = (state / 'lock').open('a')
    try:
        fcntl.flock(lock_handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        fail('INSTALL_BUSY', 'Another NkShot installer is running.')
    log_path = str(state / ('install-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S') + '-' + uuid.uuid4().hex[:8] + '.log'))
    log = open(log_path, 'w')
    os.chmod(log_path, 0o600)

    installed = json.loads(manifest.read_text()) if manifest.exists() else None
    if action == 'status':
        emit(success=True, code='OK', installed=bool(installed and current.exists()),
             version=installed.get('version') if installed else None,
             dirty=installed.get('dirty') if installed else None,
             path=str(current) if current.exists() else None, logPath=log_path)
        return

    if action == 'uninstall':
        progress('uninstall')
        changed = False
        if installed:
            for relative in installed.get('links', []):
                link = home / relative
                if link.is_symlink() and str(link.readlink()).startswith(str(root) + '/'):
                    link.unlink()
                    changed = True
            if root.is_symlink():
                fail('UNINSTALL_FAILED', 'Installation root must not be a symlink.')
            if root.exists():
                shutil.rmtree(root)
                changed = True
        emit(success=True, code='OK', changed=changed, dataPreserved=True, logPath=log_path)
        return

    progress('check')
    with tempfile.TemporaryDirectory(prefix='check-', dir=state) as temporary:
        missing = dependencies(pathlib.Path(temporary))
    if missing:
        fail('DEPENDENCY_UNSATISFIED',
             'Required dependencies are missing or incompatible. NkShot does not install dependencies; '
             'install them with your package manager and re-run.', **describe_missing(missing))
    if action == 'check':
        emit(success=True, code='OK', distribution=release_family(),
             architecture=platform.machine(), logPath=log_path)
        return

    progress('source')
    dirty = False
    if local_source is not None:
        revision, dirty = validate_local_source(local_source)
    # A pinned release revision is immutable, so reinstalling it is a no-op. A local
    # working tree keeps the same HEAD while its files change, so it always rebuilds.
    if local_source is None and installed and installed.get('version') == revision \
            and (current / 'bin' / BINARY).is_file():
        emit(success=True, code='ALREADY_INSTALLED', version=revision, changed=False,
             desktopId=DESKTOP_NAME, logPath=log_path)
        return

    def managed(path):
        return path.is_symlink() and str(path.readlink()).startswith(str(root) + '/')

    for relative in MIRRORED + ['bin/' + BINARY]:
        existing = home / relative
        if (existing.exists() or existing.is_symlink()) and not managed(existing):
            fail('INSTALL_FAILED',
                 'An unmanaged file already exists at this location; remove it first, it will not be overwritten.',
                 path=str(existing))

    if root.is_symlink():
        fail('INSTALL_FAILED', 'Installation root must not be a symlink.')
    root.mkdir(parents=True, exist_ok=True)
    versions.mkdir(exist_ok=True)

    with tempfile.TemporaryDirectory(prefix='stage-', dir=root) as temporary:
        work = pathlib.Path(temporary)
        source = local_source if local_source is not None else work / 'source'
        build = work / 'build'
        payload = work / 'payload'

        if local_source is None:
            progress('download')
            run(['git', 'init', str(source)], code='DOWNLOAD_FAILED')
            run(['git', '-C', str(source), 'remote', 'add', 'origin', REPO], code='DOWNLOAD_FAILED')
            run(['git', '-C', str(source), 'fetch', '--depth=1', 'origin', revision], code='DOWNLOAD_FAILED')
            run(['git', '-C', str(source), 'checkout', '--detach', 'FETCH_HEAD'], code='DOWNLOAD_FAILED')
            if run(['git', '-C', str(source), 'rev-parse', 'HEAD']) != revision:
                fail('INTEGRITY_FAILED', 'Source revision mismatch.')

        progress('build')
        run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Unix Makefiles',
             '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_INSTALL_LIBDIR=lib',
             '-DBUILD_TESTING=OFF', '-DNKSHOT_ENABLE_UI_CAPTURE=OFF'], code='BUILD_FAILED')
        run(['cmake', '--build', str(build), '--parallel', str(min(4, os.cpu_count() or 1))], code='BUILD_FAILED')

        progress('install')
        run(['cmake', '--install', str(build), '--prefix', str(payload)], code='INSTALL_FAILED')
        binary = payload / 'bin' / BINARY
        if not binary.is_file():
            fail('INSTALL_FAILED', 'Staged executable is missing.')
        if shutil.which('strip'):
            run(['strip', '--strip-unneeded', str(binary)], code='INSTALL_FAILED')
        desktop = payload / 'share' / 'applications' / DESKTOP_NAME
        if not desktop.is_file():
            fail('INSTALL_FAILED', 'Staged desktop entry is missing.')

        launcher = str(current / 'bin' / BINARY)
        rewrite_desktop_exec(desktop, launcher)
        dbus_service = payload / 'share' / 'dbus-1' / 'services' / 'org.nextkde.nkshot.service'
        if dbus_service.is_file():
            rewrite_service_exec(dbus_service, 'Exec', launcher, ' --dbus')
        systemd_unit = payload / 'lib' / 'systemd' / 'user' / 'app-org.nextkde.nkshot.service'
        if systemd_unit.is_file():
            rewrite_service_exec(systemd_unit, 'ExecStart', launcher, ' --dbus')
        kglobalaccel = payload / 'share' / 'kglobalaccel' / DESKTOP_NAME
        if kglobalaccel.is_symlink() or kglobalaccel.exists():
            kglobalaccel.unlink()
        kglobalaccel.parent.mkdir(parents=True, exist_ok=True)
        kglobalaccel.symlink_to(pathlib.Path('../applications') / DESKTOP_NAME)

        target = versions / (revision + ('' if not dirty else '-dirty') + '-' + uuid.uuid4().hex[:8])
        os.replace(payload, target)

        links = [relative for relative in MIRRORED
                 if (target / relative).exists() or (target / relative).is_symlink()]
        links += [str(locale.relative_to(target))
                  for locale in sorted((target / 'share' / 'locale').glob('*/LC_MESSAGES/nkshot.mo'))]
        links.append('bin/' + BINARY)

        previous = current.readlink() if current.is_symlink() else None
        created = []
        try:
            atomic_link(current, target)
            for relative in links:
                atomic_link(home / relative, current / relative)
                created.append(home / relative)
            atomic_json(manifest, dict(appId=APP, version=revision, dirty=dirty,
                                       previous=str(previous) if previous else None,
                                       sourceType='local' if local_source is not None else 'remote',
                                       links=links, slots=str(versions),
                                       installedAt=datetime.now(timezone.utc).isoformat(timespec='seconds')))
        except BaseException:
            for path in created:
                if path.is_symlink():
                    path.unlink()
            if previous is not None:
                atomic_link(current, previous)
            elif current.is_symlink():
                current.unlink()
            shutil.rmtree(target, ignore_errors=True)
            raise
        emit(success=True, code='INSTALLED', version=revision, dirty=dirty, changed=True,
             desktopId=DESKTOP_NAME, path=str(target), logPath=log_path)


def entrypoint():
    global log, lock_handle

    def interrupted(signum, frame):
        raise KeyboardInterrupt()

    signal.signal(signal.SIGTERM, interrupted)
    try:
        main()
    except Failure as error:
        emit(success=False, code=error.code, stage=stage, message=error.message,
             details=error.details, logPath=log_path)
        sys.exit(CODES[error.code])
    except KeyboardInterrupt:
        emit(success=False, code='CANCELLED', stage=stage, message='Installation cancelled.', logPath=log_path)
        sys.exit(130)
    except Exception as error:
        code = 'PERMISSION_DENIED' if isinstance(error, PermissionError) else (
            'UNINSTALL_FAILED' if action == 'uninstall' else
            'INSTALL_FAILED' if stage == 'install' else 'INTERNAL_ERROR')
        if log:
            log.write(repr(error) + '\n')
            log.flush()
        emit(success=False, code=code, stage=stage, message='Operation failed; see the log.', logPath=log_path)
        sys.exit(CODES[code])
    finally:
        if log is not None:
            log.close()
            log = None
        if lock_handle is not None:
            lock_handle.close()
            lock_handle = None


if __name__ == '__main__':
    entrypoint()
