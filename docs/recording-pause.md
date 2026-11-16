# Recording pause and resume

This is recording control, separate from pausing playback of an existing file.
Pause/resume is available only after the recorder has actually entered Recording.
The app waits for the producer's acknowledgement before showing Paused. Stop remains
available during a transition and while paused. The recording timer counts active
segments rather than time spent paused; resume continues the same output and target.

## Capability requirement

Pause needs `PipeWireBaseEncodedStream::pause()` and `resume()`, which KPipeWire
gained in 6.8. CMake probes for them at configure time. Older releases still build;
they simply have no pause control.

No downstream patch is carried. Upstream `pause()` gates the video source only: it
stops the picture while audio capture keeps running, so the audio track would grow
past the video. NkShot therefore offers pause only while no audio source is enabled:

- `supportsPause()` returns false whenever `recordSystemAudio()` or
  `recordMicrophone()` is true.
- The pause action is disabled in that state and its tooltip says why.
- With audio off, stopping the video source is exactly the behaviour wanted.

## Media contract

When pause is offered and used, the paused interval is excluded from the recorded
timeline rather than hidden: the timer counts active segments and resume continues
the same output. Encoder work already accepted may finish draining after Pause; it
belongs to pre-pause active time. Stop invalidates pending pause/resume
acknowledgements.

Because audio is never recorded while pause is offered, there is no cross-boundary
audio slicing to reason about: the audio path is inactive for the whole recording.

## Verification boundary

Building NkShot and probing the capability do not prove that a real KWin session
records pause boundaries correctly. That needs a native session with KPipeWire 6.8 or
newer, and it is not verified here.

Audio capture is a separate capability with its own probe. When it is absent the
recording is silent and the audio rows are hidden from the settings; see
[building](building.md) for the full dependency table.

## 087 focused status UI

See [recording status](recording-status.md) for the pause/finish hierarchy, explicit
encoder-finalization feedback, reviewed screenshots and verification limits.
