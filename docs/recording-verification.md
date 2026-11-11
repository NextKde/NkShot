# Recording backend verification boundaries

NkShot retains Spectacle's KPipeWire recording integration. The verified library
is KDE KPipeWire v6.8.3, commit `f30b39c53092673d136660c8deaf24d3935723eb`.

## Cloud checks completed

Build the pinned dependency's `TestEncoder` and `TestAudioEncoder` targets, then run:

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir "$KPIPEWIRE_BUILD" \
  -R '^(TestEncoder|TestAudioEncoder)$' --output-on-failure
QT_QPA_PLATFORM=offscreen "$KPIPEWIRE_BUILD/bin/TestEncoder"
QT_QPA_PLATFORM=offscreen "$KPIPEWIRE_BUILD/bin/TestAudioEncoder"
```

Both CTest entries pass, but the detailed results matter:

- Video test: **11 passed, 6 skipped**. Software x264 main/baseline/high, VP8, VP9,
  GIF and WebP initialize; reinitialization at a different size and discarding
  queued frame state pass. Three VAAPI profiles skip because no hardware encoder
  is available. Three OpenH264 profiles skip because that codec is unavailable.
- Audio test: **5 passed, 0 skipped**. AAC/Opus initialize. The Opus pipeline
  processes synthetic samples with varying frame sizes and verifies packet
  timing/duration and tail draining. It does not capture a microphone.

These are upstream backend tests, not a complete recording from NkShot. Video
initialization is not proof that a finalized recording file plays correctly.

## Still blocked / not accepted

The cloud environment has no DISPLAY, WAYLAND_DISPLAY, session DBus connection,
KWin process or compositor socket. Do not bypass socket/security restrictions to
pretend it is a desktop. Full acceptance needs an authorized Plasma/KWin session:

1. Region, window and screen capture through real permission paths.
2. Start/pause/resume/stop; verify the resulting file's codec, dimensions, duration
   and decoded frames rather than only checking that a path exists.
3. System audio and microphone separately, combined A/V synchronization, silence,
   device removal and denied permissions.
4. Resize/scale/multiple displays, minimized/closed targets and cancellation.
5. Actual GPU liquid-material UI screenshots and reduced-transparency fallback.

Do not label these items passed based on synthetic editor screenshots, codec
initialization, or the separate Vulkan material probe.

## Local file playback checks (048–049)

The real QtMultimedia backend decodes a silent synthetic four-second H.264 MP4.
Mouse Play/Pause/seek and software presentation of actual decoded frames pass.
Actual GIF and animated WebP frame progression, pause stability and resume pass
through Qt AnimatedImage, with seek hidden and no duplicate MediaPlayer source.
The WebP image-format plugin is required. Corrupt GIF/WebP/MP4 files display an
error and tolerate Retry without altering input bytes. Light/dark and compact
software UI screenshots are recorded in the iteration log.

These are generated local media fixtures, not files captured from a real desktop.
They do not change any full recording acceptance item above. Audio, GPU output,
rotation/HDR, high-resolution throughput and native desktop capture remain open.

## Playback sound and speed (062)

A single Playback settings button groups 0.5×–2× playback speed, mute and volume.
These controls affect the preview only, not the saved media or future capture
settings. Silent files show an explicit no-audio-track message and disable sound
controls. GIF/WebP continue on their separate animated-image route and do not
expose these audio/speed settings. Popup Escape, reopening and outside dismissal
are exercised along with the normal play/pause/seek path.

A generated six-second H.264/AAC file is probed for both tracks, then loaded by the
actual Qt multimedia backend. UI interactions verify the playback-rate and audio
output settings; no physical audio device exists here, so audible output, pitch
quality and A/V synchronization are **not** certified. The displayed test pattern
and tone are synthetic fixtures, not a desktop recording. MP4 without audio,
GIF/WebP and corrupt-file retry are regression-tested. Native recording, live
microphone/system capture and capture pause/resume remain separate open items.


## Historical upstream capture pause API audit (before 084)

`PipeWireRecord` inherits `PipeWireBaseEncodedStream::pause()` and `resume()`
in the locally built dependency. Inspecting only the derived header misses these
methods. The local implementation queues `PipeWireProduce::setStreamActive()`,
which toggles the video source and updates the encoded-stream state to Paused or
Recording. This code path does not stop the separate audio stream, nor does it
remove paused time from video timestamps. The API's presence therefore does not
establish seamless, synchronized capture pause/resume. NkShot has not integrated
these methods, and its current recording timer would also require pause-aware
accounting. Native A/V acceptance is required before exposing this as a feature.

Source inspection additionally identified recording-start failure isolation as
a next audit target: output-path setup currently follows source-stream creation,
and recorder state connections are installed in each start request. These are
risks to investigate with injected failures or a real KDE session, not bugs claimed
reproduced here. No unvalidated native-backend lifecycle rewrite was shipped.

## Output preflight hardening (063)

The output-path check previously accepted a directory named `clip.mp4`, because
it checked only whether the parent directory existed. The new Qt filesystem test
reproduces that invalid destination and verifies rejection. Existing FIFOs and
other non-regular files are also rejected. Preflight does not open/truncate a
recording file: existing bytes remain intact, and a new destination creates only
its parent directories. Existing writable regular files remain valid even when
their parent is not writable. New files require a writable parent.

The Wayland adapter now resolves target selection first, clears old node/output
properties, checks encoder availability and prepares the output path **before**
creating a new compositor stream or minimizing windows. A null stream is handled
before using its frame-size estimate. Structural tests enforce this ordering;
they are not compositor execution tests. The actual recorder must still handle
storage/permission changes after preflight.

Independent static review identified remaining lifecycle work: old streams are
not retired or generation-isolated, recorder state callbacks accumulate per start,
and encoder write failures need explicit handling. These are not claimed fixed by
063. Real KWin failure injection and A/V recording acceptance remain necessary.

## Callback lifetime and error recovery hardening (064)

The recorder state/error connections are now installed once per recorder, not once
per start. A capture-stream lease gives each source a generation. Disconnecting a
Qt signal does not remove already queued callbacks, so created/failed/closed
callbacks also verify that generation before touching the active recorder.

Stopping invalidates callbacks but **retains the compositor source while encoding
drains**. The source is released at recorder Idle. Failure is latched before stop;
subsequent state changes cannot report that attempt as successfully saved. A late
producer started notification cannot switch the UI back to Recording after Stop.
Queued starts and window-selection replies have cancellation epochs; screen
pointers are guarded across queued dispatch. Pending capture can be canceled.
The next completed attempt gets a fresh recorder outside signal emission, avoiding
reusing its prior producer-thread cleanup. Destructor connections are removed
before member teardown.

Tests use real Qt queued signals to reproduce a late created/failed/closed callback
after replacing a stream, repeated attempts, and stop-time quiescence without early
source destruction. Structural assertions check single recorder subscriptions and
adapter guards. These tests do not drive KWin, PipeWire or real encoder failures.
Native start/stop/restart, disk-full, device removal and A/V acceptance remain open.

An independent dependency-source audit also found a pre-existing KPipeWire edge:
`PipeWireProduce::initialize()` can clear a failed source and return without an
error/completion notification, while `destroy()` returns early for a null source.
That path may leave shutdown pending and is **not fixed by 064**. It needs a
separately verified dependency patch, not an arbitrary timer pretending encoding
has finished. Capture pause/resume was not implemented at that stage; see the 084 downstream contract below.

## Patched dependency initialization failure (065)

The specific null-source termination gap above is now fixed in the isolated
**patched** KPipeWire build. The unpatched public `PipeWireRecord` probe produced
`Failed to create PipeWire context`, emitted no error/completion, then hung during
destruction after Stop; the external eight-second watchdog exited 124. Running
the same probe against the patched library emitted error and Idle and exited 0.

Both original, unpatched files byte-match the pinned upstream commit recorded in
`third_party/patches/kpipewire-initialize-failure.json`. The inspected master branch
also retained the failing initialization branch at review time. The downstream
patch keeps the failed source alive for normal producer-thread cleanup, emits the
error, and queues initialization only after handlers are connected. Normal cleanup
emits completion once and exits the thread; repeated cleanup is guarded. No fake
completion timer or successful recording signal is added.

`recording_initialize_failure_test` links the actual installed recording library.
It deliberately uses an absent PipeWire context configuration and verifies six
attempts across normal failure, Stop on error and immediate Stop. Each must report
one nonempty error, one completion, inactive recording, no new output or change to existing file bytes, a closed
owned descriptor and a returning destructor. Environment changes are scoped to
the test process and restored exactly. CTest's timeout detects the old hang; it
does not convert it to success. This is real failure-path execution, not simulated
successful capture. Hardware codecs and native KDE recording remain unverified.

The library patch must be applied when rebuilding the pinned dependency, as shown
in [building.md](building.md). Installing unpatched distribution binaries does not
include this fix. The patch has not been submitted or accepted upstream.

## Local recording-file clip export (072)

The playback view can export a selected interval as a new H.264/AAC MP4, with
explicit local range preview, asynchronous progress/cancellation and new-file-only
atomic publication. This is genuine file re-encoding, separate from playback speed
or mute, and never modifies the recording source. See [supported media, validation
and safety boundaries](video-clips.md). It does not complete any native capture,
recording A/V or capture pause/resume acceptance item above.

## Local recording-file GIF Maker (073)

The same clip panel also offers a silent, looping animated GIF preset. It samples
a user-selected 0.20–15-second interval from the existing narrow local MP4 input
profile at 10 fps, scales down to at most 640 pixels per side without upscaling, and
limits output to 20 MiB. Separate palette and encode passes bound buffering; the
finished GIF is fully decoded for one finite cycle before new-file-only atomic
publication. Playback settings do not change the export.

This is MP4-file-to-GIF creation, **not** native GIF capture, KWin/PipeWire acceptance,
audio-device synchronization or GPU certification. See [scope](video-clips.md).

## Post-selection preparation gate (076)

[Recording preparation](recording-preparation.md) adds an actual pre-output,
pre-stream countdown after target selection, not a timer painted over an already
running recording. Selecting/Countdown/Starting do not count as recorded time;
only an actual recorder Recording notification does. Cancellation invalidates
attempt callbacks and retains an already-started producer's source until explicit
Idle completion. Retry readiness includes that outstanding producer.

The final integration build passed all three focused unit/fixture targets and
all ten injected real-adapter scenario groups, including the positive
Recording/Rendering/Idle and saved-once path. Core countdown UI passed five
software-window scenarios: launch-only, existing image, existing video, dark
appearance and 1.5× text. The full 20-target CTest suite passed on a dedicated private test bus.
Restoration additionally checks original Viewer size and visible loaded content;
old producer error/state signals after Idle are isolated from a new countdown.

The injected adapter test uses the production Wayland orchestration and explicit
fake providers, including a real three-second post-selection delay and a real
40 ms injected Starting watchdog. A native-recorder bridge exercises patched
KPipeWire failed-context/Cancel/Idle/retry/destruction with missing configuration,
not a real server or device. Native compositor selection, hide/present/frame
ordering, real recorded media, GPU/audio and A/V synchronization remain open.
Starting cancellation may retain a private partial temporary file; cleanup is
neither immediate nor guaranteed, and explicit user output is not deleted.

## Output ownership (077)

077 adds descriptor-bound reservations for private recording outputs and checks
identity before start and cleanup. The 076 conservative cleanup statement above
is superseded **only for safely identified private reservations**; explicit
outputs remain non-deleting and may leave an empty/partial file. Existing explicit
outputs are now rejected. See [ownership contract and limits](recording-output-ownership.md).

The final build passed 21/21 CTest targets, 18/18 static checks, 16 detailed helper
QtTest results with zero skips, 17 real-adapter scenario groups, five Core/QML
restoration scenarios and three reviewed warning UI scenarios. Independent checks
passed 13 QtTest results (including direct libavformat AVIO writes), 17 existing
workbench flows and 18 Gio portal/QML cases. QFile-controlled producer tests keep
the real write handle open after cancellation and flush a tail before Idle;
this is distinct from native AVIO compatibility and from native screen capture.
See the ownership document for the pinned KPipeWire close/completion source basis.
No new native desktop acceptance is claimed.

## Preparation status/control continuity (078)

078 reuses one GUI status item across preparation/Recording/Rendering and makes
Start/Stop cancel-only while pending. Starting has no floating window. The GUI
checks registered-host protocol availability before output/stream commit; unknown
or missing host cancels with recovery guidance. Host loss cancels pending work or
normally finishes/drains actual Recording. Non-GUI CLI/D-Bus are not host-gated.

Private-bus watcher tests passed 11 QtTest results, covering the real property type,
owner changes, signals, stale replies and destruction. Actual Core/SNI integration
passed in light and dark appearance: all three recording shortcuts cancel in
Selecting/Countdown/Starting, D-Bus GetAll/Activate reaches the real notifier,
menu cancellation restores the workbench, missing/lost host cancels, and rendering
or producer drain leaves the prior GUI intent unchanged. Explicit screenshot
intent still cancels preparation then starts its own countdown.

One full run passed 22/22 CTest targets (71.77 s); static checks passed 18/18.
The 17 adapter groups and actual local MP4/GIF export-and-playback smoke tests
passed. A final short warning-text/test-isolation adjustment was rebuilt and the
affected light/dark Core/SNI scenarios rerun, not an unnecessary full suite repeat.
No native screen or panel was used. Actual Recording/Rendering state behavior is
covered by the controlled adapter, not a new native successful-capture test. Registered-host state does not establish shell visibility,
native focus or clean first-frame capture. Rendering must drain without forced
frame loss; only actual Recording starts recording timing/notifications/animation.

## 084 downstream recording pause

[Recording pause and resume](recording-pause.md) documents the new pinned dependency
patch, strong capability gate, acknowledged controls and compressed A/V timeline.
The upstream audit above remains valid for an unpatched library, which is rejected.
Synthetic production encoding is distinct from native capture acceptance: actual
KWin, devices, GPU encoders and static-target resume still require validation.
