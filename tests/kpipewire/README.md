# Downstream A/V timeline regression fixture

This standalone target compiles the **same patched production producer, software
video encoder, audio encoder and MP4 muxer sources** used by the application. It
injects synthetic timestamped media at the private producer boundary, not a real
PipeWire source. It does not constitute native KDE, audio-device, hardware-encoder
or compositor acceptance.

Apply `kpipewire-initialize-failure.patch`, then
`kpipewire-timeline-pause.patch` to the pinned upstream tree. Build/install that
library into the isolated development prefix first. Configure with:

```sh
cmake -S tests/kpipewire -B /build/timeline-pause084 -G Ninja \
  -DCMAKE_PREFIX_PATH=/opt/nkshot \
  -DKPIPEWIRE_SOURCE=/sources/kpipewire-f30b39c53092673d136660c8deaf24d3935723eb \
  -DKPIPEWIRE_BINARY=/build/kpipewire
cmake --build /build/timeline-pause084 -j2
QT_QPA_PLATFORM=offscreen NKSHOT_TIMELINE_ARTIFACT_DIR=/logs/084-timeline-media \
  /build/timeline-pause084/timeline_pause_test
```

The fixture-only artifact variable selects an output directory; it does not alter
production behavior. FFmpeg's CLI is required for independent complete decoding,
not for creation or trimming of the recordings. The producer writes the files.
Failures or missing encoders are failures, not simulated successful runs.

## Contract under test

- A producer-thread monotonic interval history removes the same pauses from video
  timestamps, audio sample coordinates, gaps and final padding.
- Audio buffers are copied while valid. Future sample tails are bounded and held
  until their interval can be classified; the source callback never retains a
  borrowed buffer pointer. Intervals use half-open boundaries, with sample-boundary
  precision limited to one sample. Historical duplicate samples are not appended.
- A/V source deactivation plus processing gates precede the pause acknowledgement.
  Previously accepted encoder queues drain. Stop invalidates pending UI requests.
- Missing video timestamps are rejected. Initial pre-epoch static pictures may be
  accepted at zero. After resume, timestamps older than that resume are rejected,
  **not** relabelled as new frames. The previously encoded picture can remain
  visible until fresh timestamped content arrives. Native static refresh is not
  claimed.
- The exported non-inline `timelinePauseVersion()` symbol is an intentional strong
  compatibility requirement. Unpatched headers fail the app's configure check;
  an unpatched library cannot satisfy the executable's new symbol. A matching
  SONAME alone is not acceptance.

The test generates distinct pre-pause/post-resume colours and tones, injects
paused-only markers including delayed delivery, decodes the actual MP4s and checks
content and compressed audio duration. Two inputs and one input ending while
paused are separate cases. Decoder priming/padding has a 50 ms duration tolerance;
that tolerance is not a claim of lossless or sample-perfect AAC encoding.

Build/test status is recorded by the iteration's verification report, not inferred
from this fixture's existence.

## Verified isolated run

`084-timeline-final.log`: **9 passed, 0 failed, 0 skipped**. The patched library
and standalone target compiled successfully. Three decoded MP4s cover one/two
inputs, resume, and stopping while paused with one input ended. Video checks use
passthrough decoding and all ffprobe frame PTS, not default frame duplication.

The initial fixture crashed because it supplied a null cleanup callback to
`PipeWireFrameData`; its constructor requires a ref-counted cleanup object. The
fixture now holds the QImage through that required ownership mechanism. Production
was not changed to tolerate an invalid fixture object.

The three interval-mapping media cases pump the real encoders and finalizer
explicitly. A separate case uses real encoding/output workers, paced 100 ms input
and a bounded final-packet gate: Pause and Stop occur with output still pending,
then production deactivate/destroy/finished drains the queue and closes the muxer.
It verifies ten frames, all 100 ms timestamps, one second of audio and one finished
signal/thread exit. No manual encoder pumping or finalization is used in that case.
The real Base acknowledgement test separately exercises queued cross-thread control.
These combined checks still do not constitute successful native capture.

An initial threaded fixture used burst input and blocked the first output packet;
it failed before establishing its gate and was not accepted. The final fixture
uses normal frame cadence and only gates the final packet, without changing
production queue handling or weakening frame-count assertions.
