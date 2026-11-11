# Building NkShot

## Verified dependency versions

The first complete development build used:

- GCC 16.2, CMake 4.4.5 and Ninja.
- Qt 6.11.2, including Base/Declarative private headers, Wayland, Multimedia,
  QuickDialogs2 and ShaderTools, from official Debian sid packages.
- KDE Frameworks / Extra CMake Modules 6.30.0, plus the development packages listed
  by the top-level CMake file and their normal dependencies.
- LayerShellQt 6.7.4, Wayland / Plasma Wayland protocols, XCB, Tesseract 5.5.4.
- KQuickImageEditor v0.7.0.1 from KDE, commit
  `ac7568044defc89c494b0d6edb60073cb0ad8b25`, built with Highway 1.3.0.
- KPipeWire v6.8.3 from KDE, commit
  `f30b39c53092673d136660c8deaf24d3935723eb`, built with official FFmpeg/Mesa
  development dependencies.

Sources: https://deb.debian.org/debian,
https://github.com/KDE/kquickimageeditor and https://github.com/KDE/kpipewire.
The source libraries were installed in a separate development prefix. No host
system upgrade or screenshot-permission bypass was used.

**KPipeWire 6.7 is not sufficient.** Spectacle 6.8 uses its new microphone and
system-audio API. CMake now rejects an older version instead of letting configure
succeed and failing much later in compilation. Do not remove audio functionality
to work around this requirement.

## Build sequence

In an environment with the matching development packages installed, first build
KQuickImageEditor and KPipeWire into your chosen `PREFIX`. Before building the
pinned KQuickImageEditor v0.7.0.1, apply the reviewed one-line repaint fix:

```sh
git -C "$KQUICKIMAGEEDITOR_SOURCE" apply "$NKSHOT_SOURCE/third_party/patches/kquickimageeditor-shadow-repaint.patch"
```

This fixes a reproduced incremental-shadow boundary difference. The patch targets
commit `ac7568044defc89c494b0d6edb60073cb0ad8b25`; do not blindly reapply it to a
newer upstream version. `shadow_repaint_test` detects whether the linked library
still exhibits the issue.

Before building the pinned KPipeWire 6.8.3 source, apply its initialization-failure
fix as well:

```sh
git -C "$KPIPEWIRE_SOURCE" apply --check "$NKSHOT_SOURCE/third_party/patches/kpipewire-initialize-failure.patch"
git -C "$KPIPEWIRE_SOURCE" apply "$NKSHOT_SOURCE/third_party/patches/kpipewire-initialize-failure.patch"
```

The patch targets commit `f30b39c53092673d136660c8deaf24d3935723eb`. Its companion
JSON records original source hashes. It preserves the normal cleanup path after
stream initialization fails and connects completion/error handlers before starting
asynchronous initialization. Build and install that patched dependency into the
same prefix before configuring NkShot. Do not blindly apply it to a newer version
or modify a distribution's installed binaries. This is a local downstream patch,
not an upstream-accepted change. `recording_initialize_failure_test` directly
checks the linked library and intentionally fails/times out against the affected
unpatched implementation. Then build NkShot:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DBUILD_TESTING=ON
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
cmake --install build
python3 tests/nkshot/test_identity.py -v
```

Set `QML2_IMPORT_PATH` to the prefix's installed QML directory when launching if
it is not a standard system prefix. For this build that was `lib/x86_64-linux-gnu/qml`.
Also include the prefix's `share` directory in `XDG_DATA_DIRS`, and ensure its
libraries are resolvable by the runtime linker. Do not replace the system
Spectacle installation while testing.

## Validation boundary

Building, installing and passing CTest do not prove screenshot permissions,
recording output, editing interactions or visual quality. Those require a real
KWin session and the checklist in `nkshot-validation.md`. The cloud build was
isolated from the host; local display sockets are currently unavailable there.

OCR regression testing requires Tesseract English data (`tesseract-ocr-eng`) and
DejaVu Sans fonts. The test creates a synthetic English image and uses a temporary
configuration directory; it does not submit images to a network service.

## Production configuration

Use `-DBUILD_TESTING=OFF -DNKSHOT_ENABLE_UI_CAPTURE=OFF` for a production build.
Qt Test is needed only for tests or the explicit developer capture hook. A fresh
production build has also been verified with
`-DCMAKE_DISABLE_FIND_PACKAGE_Qt6Test=TRUE`; do not enable the capture hook in
distributed builds.

Chinese OCR regression coverage additionally uses Tesseract `chi_sim` trained
data and a CJK-capable font, preferably Noto Sans CJK SC (`fonts-noto-cjk`).
The Chinese case explicitly skips if either dependency is missing; inspect
individual QtTest results rather than treating an aggregate pass as proof it ran.

## Runtime image plugins

Animated WebP review needs Qt's WebP image-format plugin, not merely libwebp.
On the verified Debian environment this is `qt6-image-formats-plugins` with its
runtime dependencies; GIF support is supplied by the Qt GIF plugin. Include the
corresponding official Qt image plugins in distribution packaging. A missing
plugin produces a visible playback failure, not a claim that a file was decoded.
The isolated 049 environment added image-formats/libmng/libwebpdemux from signed
Debian metadata, without upgrading host packages.

## Optional local video clip export (072–073)

Install your distribution's FFmpeg CLI package, including `ffmpeg`, `ffprobe`,
H.264 `libx264` and AAC encoding, to enable local MP4 clip export. This is a runtime
optional dependency, not bundled media software and not an automatic download.
Playback/capture and the rest of the application still build without those CLIs;
clip export explains missing tools or unsupported encoders. The real
`video_clip_test` requires them and fails clearly when unavailable rather than
pretending that real media content was validated. The verified isolated toolchain uses
Debian FFmpeg 9.0.2. See [clip scope and safety](video-clips.md).

GIF export additionally requires FFmpeg’s GIF/PNG encoders and palettegen/paletteuse
filters. Its capability checks are independent of MP4 H.264/AAC encoders. No new
application-linked library or automatic installer is introduced.

## Desktop portal screen colors (074)

The screen picker reuses QtDBus and KF6 WindowSystem. It requires a desktop portal
implementing Screenshot.PickColor at runtime, not an extra capture library.
KWindowSystem 6.28 or newer supplies the non-deprecated asynchronous
exportToplevel API used for Wayland parent identifiers.

Protocol tests must never own the portal name on a normal desktop session.
CTest runs them through dbus-run-session when that tool is available. In isolated
build roots without it, supply a dedicated private test bus and explicitly set
NKSHOT_TEST_PRIVATE_BUS=1; absence is a test failure, not a silent skip.
The test executable's --fake mode is only a test service, not production code.

### Startup presentation tests

Iteration 075 adds `startup_setup_test` (Qt Test/Quick plus the existing
`KF6::I18nQml` and Kos.Ui dependencies). No new runtime package is required.
The full development CTest suite now contains 18 tests. The standalone setup
fixture injects models/settings only; production still uses SpectacleCore and
the platform models. See [startup UI reproduction](../tests/ui/README.md#startup-workflow-075).

## Recording preparation tests (076)

No new runtime library is introduced. `RecordingPreparation` and
`RecordingStartGuard` use QtCore; the existing opt-in UI harness links QtTest to
exercise the real Wayland adapter through explicit dependency injection. Native
wrappers still use the documented KPipeWire/Screencasting dependencies and patch.
There is no production environment-based capability override.

```sh
cmake --build build --target recording_preparation_test recording_start_guard_test startup_setup_test
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ctest --test-dir build \
  -R '^(recording_preparation_test|recording_start_guard_test|startup_setup_test)$' \
  --output-on-failure
```

With `NKSHOT_ENABLE_UI_CAPTURE` enabled, run the real-adapter checks separately
from countdown UI checks, each with a fresh test configuration:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  XDG_CONFIG_HOME="$(mktemp -d)" NKSHOT_TEST_RECORDING_ADAPTER=1 \
  NKSHOT_TEST_UI_CAPTURE=/tmp/recording-adapter.png \
  build/bin/nkshot --new-instance --launchonly

QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  XDG_CONFIG_HOME="$(mktemp -d)" NKSHOT_TEST_RECORDING_PREPARATION=1 \
  NKSHOT_TEST_UI_CAPTURE=/tmp/recording-preparation.png \
  build/bin/nkshot --new-instance --launchonly
```

The adapter emits a JSON pass/failure report; failed assertions are not skips.
Its missing-config native-recorder case scopes and restores the two PipeWire
configuration variables and must not use live user devices. Run only one build
process at a time. The prior 18-test total described under 075 is historical;
076 adds two CTest targets. The final build passed three focused targets, ten adapter groups and five Core
UI scenarios (launch-only/image/video/dark/1.5× text). The complete 20-target CTest suite passed with a dedicated private bus.
ScreenColorPickerTest deliberately refuses to run without NKSHOT_TEST_PRIVATE_BUS
and a real isolated bus; merely setting the flag is not a substitute for isolation.

## Owned recording outputs (077)

`owned_recording_output_test` links QtCore/Test and exercises Linux descriptor
operations against private temporary directories. It does not require a desktop.
Its FFmpeg compatibility case runs a local synthetic encode into a reservation;
if the CLI is absent, that case explicitly skips rather than reporting success.
The application does not install FFmpeg or change the user's permissions.

```sh
cmake --build build --target owned_recording_output_test
ctest --test-dir build -R '^owned_recording_output_test$' --output-on-failure
```

Re-run the opt-in `NKSHOT_TEST_RECORDING_ADAPTER=1` harness from the 076 section
against the final binary for cancellation/Idle/output retention and warning
isolation. Ownership logic is production code; injected recorder providers and
synthetic filesystem/media fixtures belong only to tests. Final results: 21/21 CTest targets, 16 detailed helper QtTest results with zero
skips, 17 adapter groups, five Core UI scenarios and three warning UI scenarios. The [ownership document](recording-output-ownership.md)
separates private cleanup, explicit preservation and same-UID TOCTOU limitations.

For the warning screenshot workflow, combine `NKSHOT_TEST_RECORDING_ADAPTER=1`
with `NKSHOT_TEST_RECORDING_CLEANUP_WARNING=1`, `NKSHOT_TEST_STARTUP=1`,
`--launchonly` and the normal screenshot destination. The adapter first produces
a real preservation warning using synthetic files; only the test harness forwards
it to the actual Core/InlineMessage UI. Add `NKSHOT_TEST_DARK=1` or
`NKSHOT_TEST_UI_SIZE=420x520` for appearance checks. Never run against private media
or claim this signal handoff is a native recording.

## Status-notifier availability checks (078)

`StatusNotifierAvailability` uses existing QtDBus. Its test watcher runs in a
separate process on a dedicated private session bus. Use the same isolated-bus
arrangement as `screen_color_picker_test`, with `NKSHOT_TEST_PRIVATE_BUS=1`; never
register the fake watcher on a normal desktop session. No package installation or
production capability override is introduced. The watcher test passed 11 QtTest results. `NKSHOT_TEST_RECORDING_CONTROLS=1` with
`--launchonly` runs the actual Core and KStatusNotifierItem against a private test
watcher, including real D-Bus GetAll/Activate calls. Use a fresh XDG configuration
and `NKSHOT_TEST_UI_CAPTURE` destination; `NKSHOT_TEST_DARK=1` selects dark appearance.
The full suite passed 22/22 CTest targets. These checks do not certify native tray display.

## Offline translation (080)

The application links no model engine: QtCore launches the explicitly installed
local one-shot worker. Follow [installation and model licensing](local-translation.md).
Model weights, venvs and downloaded archives must not enter the repository.
`translation_manager_test` needs only local Python 3 stdlib and tests fake-worker
protocol/lifetime behavior, not model quality. For actual-model software UI checks,
set `NKSHOT_TEST_TRANSLATION=1` and an ordinary synthetic input image.
`NKSHOT_TEST_TRANSLATION_DEFAULT=1` removes all explicit overrides and exercises
the standard XDG layout; `NKSHOT_TEST_TRANSLATION_NARROW=1` checks 360-pixel width.
The setup-link test inspects its fixed documentation URL without opening a browser.

## Scrolling alignment and preview (082)

The scrolling session uses `ScrollFrameAligner`; file-based exact composition
continues to use `VerticalStitcher`. Focused tests are `scroll_frame_aligner_test`,
`scroll_capture_session_test`, `scroll_capture_controller_test` and
`vertical_stitcher_test`. Real QPainter text/white-margin fixtures require the GUI
platform to be set to offscreen when no desktop exists.

The opted-in UI harness flag `NKSHOT_TEST_SCROLL_SESSION=1` supplies explicitly
synthetic frames through the real controller/session into the actual preview,
retry and Finish/pin-export route. It cannot validate the native KWin backend.
`NKSHOT_TEST_SCROLL_SESSION_COMPACT=1` checks 420-pixel width; the usual dark flag
checks theme. Production has no environment switch to inject frames. Existing
`NKSHOT_TEST_SCROLL_PREVIEW=1` still verifies unavailable API5 behavior separately.

For 083 continuous collection, add `NKSHOT_TEST_SCROLL_CONTINUOUS=1` to the
`NKSHOT_TEST_SCROLL_SESSION=1` controlled-frame UI check. The existing dark and
compact flags apply. This drives the actual checkbox, periodic requests, pause,
unchanged-frame skip, failed-frame retry and Finish during an outstanding request;
it does not replace native KDE acquisition acceptance.

## 084 recording pause dependency

Apply `kpipewire-initialize-failure.patch` then
`kpipewire-timeline-pause.patch` to pinned KPipeWire
`f30b39c53092673d136660c8deaf24d3935723eb`; retain both patches. Rebuild
the private dependency before configuring NkShot. Stock headers fail the CMake
compile/link capability check; a stock same-SONAME runtime fails symbol resolution
rather than silently using video-only pause. See [contract](recording-pause.md) and
[`tests/kpipewire/README.md`](../tests/kpipewire/README.md) for the standalone real
encoder/muxer regression fixture and exact isolated commands. No global library
replacement is required. Native KDE validation remains separate.

## 085 local media import checks

`ctest -R '^(local_media_loader_test|video_clip_test)$'` exercises real local
image/MP4 decoding and the existing export job. Importing MP4 needs existing
`ffmpeg` and `ffprobe`; the image loader uses Qt image plugins. The loader does
not require encoding support merely to open an MP4. It respects Qt's configured
image allocation ceiling, so unusually low `QT_IMAGEIO_MAXALLOC` settings may
reject large screenshots without changing that setting.

With `NKSHOT_ENABLE_UI_CAPTURE`, `NKSHOT_TEST_LOCAL_MEDIA_OPEN=1` drives real Open,
confirmation, Save As and clip dialogs with generated media. Existing DARK/COMPACT
flags apply. The normal variant also opens a real 19.2MP generated long image.
Controlled delayed capture/Finished platforms cover gating, not native recording.

For generic source builds these external CLI tools remain optional at build time;
MP4 Open/clip operations report their absence at runtime. Debian preview085
packaging explicitly declares the `ffmpeg` package dependency (which supplies both
CLIs). The existing 084 preview requires users to install it separately. Verified 085 and 097 production preview packages are documented in
[packaging](../packaging/debian/README.md); native desktop acceptance remains separate.
