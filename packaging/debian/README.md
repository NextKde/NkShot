# Debian sid amd64 preview packaging

This is a narrowly targeted preview, **not** a distribution-independent package.
The 080 artifact uses published commit
`3b28ce4d13634764d7b25e6d857e4cd3506a4d4b`, not the unpublished translation work.

## Supported build baseline

- Debian sid amd64, Qt >= 6.11, KF >= 6.30, Plasma development libraries >= 6.8.
- KDE Plasma Wayland is the intended runtime. KWin screenshot permission and
  PipeWire screencast availability must be tested in a logged-in real session.
- Not suitable for Debian stable, Ubuntu LTS, X11-only environments, or GNOME.
- GCC 16.2, Qt 6.11.2, KF 6.30 were used in the initial isolated build.

The package installs `nkshot` and `org.nextkde.nkshot` desktop/DBus entries.
It neither replaces `spectacle` nor assigns PrintScreen or changes global shortcuts.
Patched libraries and QML modules stay in `/opt/nkshot`; there are no global
library replacement or ldconfig hooks. The launcher supplies private search paths.

## Build procedure

Install official distro development dependencies listed in `docs/building.md`.
The build host also needs `dpkg-dev`, Python 3, CMake, Ninja and a normal dpkg
package database. Build from the exact published source, not an uncommitted tree.

1. Obtain upstream KQuickImageEditor commit
   `ac7568044defc89c494b0d6edb60073cb0ad8b25` and KPipeWire commit
   `f30b39c53092673d136660c8deaf24d3935723eb`.
2. Apply the matching patches in `third_party/patches/` to pristine copies.
   For the recording pause/resume source, KPipeWire requires
   `kpipewire-initialize-failure.patch` **first**, then
   `kpipewire-timeline-pause.patch`. Verify prerequisite and final source hashes
   against the metadata accompanying the exact source revision being packaged;
   do not use hashes from an earlier, still-changing development patch.
   KQuickImageEditor requires `kquickimageeditor-shadow-repaint.patch`.
   Do not apply these patches to a different upstream revision without review.
   When reproducing the old 080 package, use only patches present at 080:
   it did not contain the timeline-pause patch or the new recording capability.
3. Configure/build/install both into a **clean** `/opt/nkshot` development prefix:
   `cmake -S SOURCE -B BUILD -G Ninja -DCMAKE_INSTALL_PREFIX=/opt/nkshot
   -DCMAKE_PREFIX_PATH=/opt/nkshot -DCMAKE_BUILD_TYPE=Release`, then
   `cmake --build BUILD --parallel 4` and `cmake --install BUILD`.
4. For pause/resume-capable source, first confirm the private KPipeWire exports
   `PipeWireBaseEncodedStream::timelinePauseVersion() const` and that the app's
   compile/link capability check passes. A stock library with the same SONAME
   is not an acceptable replacement. See `docs/recording-pause.md`.
   Configure NkShot with the same settings plus
   `-DBUILD_TESTING=OFF -DNKSHOT_ENABLE_UI_CAPTURE=OFF
   -DCMAKE_INSTALL_RPATH=/opt/nkshot/lib/x86_64-linux-gnu` and build it.
5. Run `packaging/debian/package.sh APP_BUILD KPIPEWIRE_BUILD
   KQUICKIMAGEEDITOR_BUILD OUTPUT_DIR DEBIAN_VERSION`.
   The version argument is mandatory and must match the source being packaged.
   For the exact 080 source above, use `0.1.0~preview080`. For a later verified
   source revision, choose its own release version and record the actual commit;
   do not reuse `preview080`. Epochs and path/control characters are rejected.
   No post-080 package is claimed built or validated by this document.

Packaging stages CMake install manifests afresh. It does **not** copy the entire
live prefix, a Python virtual environment, translator models, or UI test harness.
`runtime-deps.py` traverses every packaged ELF dependency and fails on unresolved
libraries; QML runtime modules are declared separately. Library package versions
are conservative lower bounds from the build system. This makes a large but safe
runtime dependency set; it is not a claim of ABI compatibility with older systems.
The script includes licenses for the app and both bundled libraries.

For the isolated extracted-deb toolchain only, `NKSHOT_PACKAGE_MAP` can point to a
JSON map of installed file paths to `[package,version]`, derived from the actual
official .deb archives. This is not a substitute for validating package origins.

## Install / remove

For the already generated **080** preview artifact, on the matching test system:

```
sudo apt install ./nkshot_0.1.0~preview080_amd64.deb
nkshot
# Removal leaves user captures/settings untouched:
sudo apt remove nkshot
```

Do not force dependencies or install this package on an older Plasma system.
`apt` resolving dependencies is a prerequisite, not proof of capture correctness.

## Required acceptance checks before a stable release

- `dpkg-deb --contents` contains no Spectacle-owned registration paths.
- All ELFs resolve; `nkshot --version` starts using only package private libraries.
- Real Plasma Wayland: full-screen, region, window, fractional/multiple monitors.
- Real video recording, actual stop/finalize, playable exported video and audio.
- For pause/resume releases: check the packaged private KPipeWire's exported
  capability symbol and the app's strong reference, not merely a matching SONAME.
  Run the `tests/kpipewire` production encoder/muxer fixture against that exact
  dependency build and retain its results. Generated-media fixture success alone
  does not validate native KWin capture, PipeWire devices or hardware encoders.
- In real Plasma recording, verify pause gates audio and video together, exported
  duration excludes paused intervals, resume preserves audio/video sync, and Stop
  works while paused or transitioning. Test the first frame after resume and
  both software and supported hardware encoders; do not infer these from a build.
- Annotation/undo/save/reopen; cancellation and permission-denied recovery.
- DBus launch and menu launch work with the renamed application permission ID.
- Install/uninstall alongside the system Spectacle without changing its behavior.

An offscreen UI launch can check imports and startup only. It cannot validate
KWin privileges, real screen capture, recording, scrolling or global shortcuts.

## Initial 080 verification

Release compilation and `.deb` generation succeeded. The extracted package's
launcher returned `nkshot 6.8.3` (the inherited upstream application version).
All packaged ELF dependencies resolved in the matching sid toolchain. The two
bundled dependency patches passed reverse-apply checks. The archive contains no
system Spectacle registration files or translation virtual environment.

Full UI startup is **not verified**: the isolated toolchain has no session bus,
and the host sandbox refused creation of a new D-Bus socket. No install transaction
or real Plasma capture was performed. Treat this as a packaging preview pending
the acceptance checks above, not a stable end-user release.

## Next package preparation (not yet built)

After the pause/resume source and dependency metadata are frozen and published,
record that exact commit and use a new clean source snapshot with its own production
build directory (planned: `/build/package084`). Use the lead-verified private
KPipeWire build only after its patch hashes and capability export are finalized;
do not race or rebuild a shared dependency while core validation is underway.
Keep the existing 080 package and its evidence separate, select an explicit new
Debian version, then repeat archive, dependency, launcher and native acceptance
checks. This is a plan, not a claim that an 084 package or fixture has passed.
