# Recording pause and resume

This is recording control, separate from pausing playback of an existing file.
Pause/resume is available only after the recorder has actually entered Recording.
The app waits for the producer's acknowledgement before showing Paused. Stop remains
available during a transition and while paused. The recording timer counts active
segments rather than time spent paused; resume continues the same output and target.

## Required private dependency

Pinned dependency: KDE KPipeWire `f30b39c53092673d136660c8deaf24d3935723eb`.
Apply these downstream patches **in order**:

1. `third_party/patches/kpipewire-initialize-failure.patch`
2. `third_party/patches/kpipewire-timeline-pause.patch`

Rebuild/install that dependency into the private prefix, then configure NkShot.
The CMake compile/link check and the native recorder require the exported
`PipeWireBaseEncodedStream::timelinePauseVersion()` symbol. Stock KPipeWire's
same-SONAME library does not satisfy this contract. Do not substitute it: its
original pause implementation stops video only, not audio or elapsed timestamps.
Patch metadata records prerequisite and final source hashes. Reverting this feature
requires reverting its application integration and the second dependency patch;
the initialize-failure fix remains independent.

## Media contract

The producer owns pause boundaries on its monotonic capture clock. Video and audio
sources and processing are gated together. Captured timestamps are mapped through
recorded pause intervals rather than subtracting the current pause duration from
old frames. Audio buffers that cross boundaries are sliced into active sample
intervals; late buffers cannot reintroduce paused samples. Audio padding and final
Stop use the same compressed timeline. Source deactivation is not device permission
revocation, nor a promise that operating-system buffers instantly disappear.

Previously accepted encoder work may finish draining after Pause. It belongs to
pre-pause active time, not newly accepted paused content. Stop invalidates pending
pause/resume acknowledgements and follows the existing encoder drain/output ownership
rules. Resumed static video with an old render timestamp is treated conservatively:
the earlier accepted picture remains until a new eligible frame arrives; no queued
old picture is relabeled as newly captured content.

## Verification boundary

The standalone fixture in `tests/kpipewire` exercises patched production producer,
software encoders and the MP4 muxer with generated video and audio. It is not native
KWin capture, a PipeWire device test, or GPU acceptance. Actual KDE source activation,
resume-first-frame behavior and hardware encoders still require native verification.

## 084 reproducible results

- Standalone production-source fixture: **9 QtTest results passed**. Three interval
  cases decode 10/20 frames at 100 ms PTS, compressed 1/2-second AAC audio, active
  tones and no paused-only green frame or tone. AAC duration tolerance is 50 ms.
- A separate real-worker case feeds ten frames at normal cadence, holds only the
  final output packet briefly, then pauses/stops with a pending queue. Production
  deactivate/destroy/finished closes the real MP4: ten frames, one second of audio,
  one finished signal and worker exit pass. This does not use manual finalization.
- Actual Base cross-thread queued acknowledgements and Stop invalidation pass.
  An advancing-clock test covers the shared pause-boundary snapshot.
- Five focused application CTests, 19 adapter groups, three actual software
  Core/QML/SNI pause scenarios, existing status controls and 18 static checks pass.
- Patched capability probe succeeds; old headers fail compilation and the old
  same-SONAME library fails runtime symbol resolution (exit 127).

A first burst-input threaded fixture failed to reach its first-packet gate. It was
replaced with paced input/final-packet gating, without relaxing output assertions
or changing production drain semantics. These are generated-media and controlled
platform checks, not a successful desktop/device recording claim.

## 087 focused status UI

See [recording status](recording-status.md) for the pause/finish hierarchy, explicit
encoder-finalization feedback, reviewed screenshots and verification limits.
