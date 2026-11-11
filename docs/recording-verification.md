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


## Capture pause API audit

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
