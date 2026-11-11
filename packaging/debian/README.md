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
   The verified 084 and 085 previews are documented below; no 086 package is claimed.

Packaging stages CMake install manifests afresh. It does **not** copy the entire
live prefix, a Python virtual environment, translator models, or UI test harness.
`runtime-deps.py` traverses every packaged ELF dependency and fails on unresolved
libraries; QML runtime modules are declared separately. Library package versions
are conservative lower bounds from the build system. This makes a large but safe
runtime dependency set; it is not a claim of ABI compatibility with older systems.
The script includes licenses for the app and both bundled libraries.
The 085 packaging source explicitly declares `ffmpeg (>= 7:9.0.2-4)` in Depends:
Debian's tested official package supplies both `/usr/bin/ffmpeg` and
`/usr/bin/ffprobe`. Clip/GIF export and MP4 opening launch these via QProcess,
so ELF dependency scanning alone cannot discover this requirement. This is a
dependency fix included in the verified 085 preview documented below.

For the isolated extracted-deb toolchain only, `NKSHOT_PACKAGE_MAP` can point to a
JSON map of installed file paths to `[package,version]`, derived from the actual
official .deb archives. This is not a substitute for validating package origins.

## Install / remove

For the current **085** preview artifact, on the matching test system:

```
sha256sum -c SHA256SUMS
sudo apt install ./nkshot_0.1.0~preview085_amd64.deb
nkshot
# Removal leaves user captures/settings untouched:
sudo apt remove nkshot
```

The already delivered 084 package omitted the explicit FFmpeg CLI dependency.
If using that older artifact, install the official `ffmpeg` package alongside it
for clip/GIF features;
its shared-library dependency closure alone does not provide those commands.
The 084 artifact and checksum are unchanged. The 085 package fixes this omission
with an explicit dependency; apt should resolve the CLI package automatically.

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

## Verified 084 packaging preview

Source commit: `5570265df73ceaf009a637610425e9f3abdb692b`.
Package: `nkshot_0.1.0~preview084_amd64.deb` (2,670,104 bytes).
SHA256: `7060ca48688fdca667a14875122c407d63d6011b1bde20f73e5eb538d4a4f903`.

All 844 published source blobs matched before and after an isolated production
Release build, with `BUILD_TESTING=OFF` and `NKSHOT_ENABLE_UI_CAPTURE=OFF`.
The snapshot excludes unpublished translation work and later active app changes.
Applying the two KPipeWire patches in order to the pinned pristine source produced
all metadata-specified final source hashes. Packaging used the frozen verified
private dependency build, without rebuilding the shared development libraries.

The complete archive extracted successfully. All packaged ELF dependencies resolved
and produced the same dependency list before and after extraction. The package's
recorder library retains the strong exported `timelinePauseVersion()` symbol and
the application retains its strong reference. A probe using the actual packaged
library returned `timeline-pause=1`. The extracted launcher returned
`nkshot 6.8.3`, the inherited application version; the Debian package version is
`0.1.0~preview084`. Registration remains independent of system Spectacle.

This is **not** native-desktop acceptance. No apt install/uninstall transaction,
actual KDE UI startup, real compositor capture, PipeWire-device recording, GPU
encoding or native pause/resume was verified for this package. The existing
synthetic production-source encoder/muxer fixture is separate evidence, not a
packaged end-to-end desktop test. Static resumed video may retain the previous
picture until a fresh eligible timestamped frame arrives. The old 080 package
remains historical evidence and does not contain the timeline-pause capability.
The later 085 package verification is recorded separately below.

## Verified 085 packaging preview

Source commit: `9857bd8cded84a80fc79ba57612f198b3a6c6eb0`.
Package: `nkshot_0.1.0~preview085_amd64.deb` (2,692,700 bytes).
SHA256: `96cfdd94099aad269ef25ef244c0e745d617e19984de2f0d8ffbfb6adeb2a342`.

All 853 published source blobs matched before and after the isolated Release build.
Tests and the UI capture harness were disabled. The ordered KPipeWire patches and
frozen private dependencies were verified without modifying the shared prefix.
Complete extraction, post-extraction ELF dependency closure, strong pause capability
export/reference, and the probe using the actual packaged library all passed.
The launcher returned `nkshot 6.8.3` and `--help` succeeded; the Debian package
version is `0.1.0~preview085`, distinct from that inherited application version.

The archive declares `ffmpeg (>= 7:9.0.2-4)` in Depends. The official Debian archive
and package mapping confirm that it supplies both `ffmpeg` and `ffprobe`; both
version commands ran successfully in the matching toolchain. This closes the 084
CLI dependency omission. Registration remains independent of system Spectacle.

No apt installation/removal transaction, real KDE startup/capture, PipeWire-device
recording, GPU acceptance, or interactive local-file Open flow through this
production package was verified. Source-level UI tests do not establish package
end-to-end acceptance. The 084 native pause/resume limitations above still apply.
This remains a Debian sid amd64 preview; no 086 package is claimed.
