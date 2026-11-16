# Building NkShot

## Dependency floor

Everything NkShot needs comes from distribution packages. The repository carries
no downstream patch of KPipeWire and none of KQuickImageEditor.

| Requirement | Minimum | Why |
|---|---|---|
| Qt | 6.8 | Vendored KQuickImageEditor 0.7 under `third_party/` |
| KF6 / Extra CMake Modules | 6.13 | Same |
| KPipeWire | 6.4 | `start()`/`stop()`; 6.3 does not have them |
| Google Highway | 1.3 | Vendored KQuickImageEditor |
| Tesseract | any | OCR |
| C++ compiler | C++23 | Vendored KQuickImageEditor (GCC 13+, Clang 17+) |

KQuickImageEditor is not a system dependency: `third_party/kquickimageeditor/`
holds a pinned v0.7.0 snapshot, so distributions that only ship 0.5.x or 0.6.x
still build NkShot.

Two capabilities are compiled in only when the installed KPipeWire offers them.
When it does not, the matching control is absent from the interface instead of
being present and broken:

| Capability | Needs | Without it |
|---|---|---|
| Recording pause | KPipeWire 6.8, `pause()`/`resume()` | No pause control |
| Audio capture | KPipeWire 6.8, `recordSystemAudio()`/`recordMicrophone()` | Recording is silent and the audio rows are hidden |

The X11 backend additionally needs Qt 6.10 private X11 headers. Below Qt 6.10 the
build is Wayland only, and configure says so.

CMake decides both capabilities with `check_cxx_source_compiles` rather than with
a version number, so a distribution backport is detected correctly.

## Build sequence

```sh
./install.sh check                 # lists anything missing, with package names
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF -DNKSHOT_ENABLE_UI_CAPTURE=OFF
cmake --build build --parallel 4
DESTDIR="$PWD/stage" cmake --install build --prefix /usr --strip
```

No private prefix, no `CMAKE_PREFIX_PATH` and no `LD_LIBRARY_PATH` are needed: a
system that satisfies the table above builds against its own libraries.
`install.sh` runs the same build and installs into a versioned slot under
`~/.local/share/nkshot-app/`.

Keep `--parallel` bounded. An unbounded `--parallel` on a 16+ thread machine can
exceed available memory and get the build, or the surrounding desktop session,
killed by the OOM killer.

Release artefacts (.deb, .rpm, AppImage) are produced by
`.github/workflows/release.yml`, which builds inside Debian sid and Fedora rawhide
containers. End users are expected to install those instead of compiling.

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

## 084 recording pause and audio capability

No downstream patch is required or carried. KPipeWire 6.8 introduced
`pause()`/`resume()` together with the system-audio and microphone API; earlier
releases simply do not have them.

CMake probes both at configure time and NkShot degrades accordingly:

- Without `pause()`/`resume()`, `RecordingBackend` reports `supportsPause() ==
  false` and no pause control is created.
- Without the audio API, `supportsAudioCapture() == false`, the two audio rows are
  hidden in the recording settings and the recording carries no audio track.
- Upstream `pause()` only gates video, so pause is additionally disabled while any
  audio source is enabled, and the reason is shown in the tooltip.

See the [pause contract](recording-pause.md).

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
CLIs). The existing 084 preview requires users to install it separately. Verified 085, 097 and 100 production preview packages are documented in
[packaging](../packaging/debian/README.md); native desktop acceptance remains separate.
