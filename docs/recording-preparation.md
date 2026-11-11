# Recording preparation (076)

## Use

The GUI recording setup offers **No delay, 3, 5 or 10 seconds**, defaulting to no
delay. Choose the target first; the delay starts only after that selection is
accepted. Cancel, Escape or closing the countdown stops preparation. A zero delay
still starts asynchronously, without showing a countdown. Screenshot delay is a
separate setting. CLI/D-Bus recording continues to use zero preparation delay.

The countdown is not recording. “Starting” is not recording either: elapsed
recording time begins only after the recorder acknowledges its Recording state.
A cancelled attempt does not emit a saved result. When a previous producer is
still stopping, wait for it to finish before retrying; returning to the setup
screen does not mean the producer has already released its resources.

## Ordering and lifetime contract

`RecordingPreparation` owns one attempt token through Selecting → Countdown →
Starting → Recording. It uses a monotonic absolute deadline. Selection time does
not consume the delay; delayed event delivery may postpone, but not shorten, it.
Even no-delay readiness is queued. Phase notification dismisses the countdown
before the adapter queues its output/stream commit, which rechecks the token.
Output preparation and compositor stream creation are not allowed before this
gate. Compositor presentation/frame ordering still needs native acceptance.

Target, format/output selection, audio, pointer, filename template and save
location are frozen for the attempt. Settings edited afterward affect the next
attempt. Display topology changes, an unavailable target/provider or a KWin
owner change invalidate preparation instead of silently selecting another target.
Provider changes also refresh supported modes/formats.

Calling recorder `start()` can launch a producer while its public state remains
Idle. `RecordingStartGuard` therefore records start issuance before that call.
Cancel invalidates the attempt and suppresses callbacks, but retains an
outstanding producer's source lease until an explicit Idle completion. A late
Recording signal cannot resurrect a canceled attempt. `readyForRecording` gates
retry on this lifecycle, not merely on an Idle preparation phase. The default
Starting watchdog is 30 seconds.

Temporary GUI outputs must remain within the private temporary directory; clean
and canonical containment, symlink and existing-file checks precede assigning the
output target. An empty recorder-output reset is not file creation. Countdown
cancellation must leave an explicit existing output unchanged.

**Conservative cleanup limitation:** cancellation does not unlink an output just
because its pathname was recorded; inode-bound ownership is not established.
Starting cancellation can leave a partial private temporary file. Temporary
directories have auto-removal disabled, so exit does not guarantee cleanup. A
future temporary-directory initialization may scan older unlocked directories;
this is a cleanup opportunity, not an immediate or guaranteed deletion. Explicit
output files are not deleted by cancellation.

## Evidence status for the final 076 integration build

- The three focused unit/fixture targets passed:
  `recording_preparation_test`, `recording_start_guard_test`, and
  `startup_setup_test`.
- All **ten real-adapter scenario groups passed**, including the positive
  Recording/Rendering/Idle, recorded-time, saved-once and restart path.
- Core/countdown UI passed in **five scenarios: launch-only, existing image,
  existing video, dark appearance and 1.5× text**. These are software-window integration
  checks, not a native desktop recording.
- The full **20-target CTest suite passed** on a dedicated private test bus.
- Native KDE/Wayland/PipeWire capture, GPU encoding, real audio/A-V sync and final
  native output acceptance remain **unverified**.

The adapter checks instantiate the real `VideoPlatformWayland` with explicit fake
recorder/screencast providers. They cover the post-selection deadline, frozen
settings, cancellation, late callbacks, source retirement/retry, capability
changes, output-path boundaries and a 40 ms injected Starting watchdog. The
positive path exercises Recording → Rendering → Idle, timing and a saved signal
with a fake media producer; it does not produce a genuine recording.

A separate bridge uses the native recorder wrapper and patched KPipeWire with a
deliberately missing configuration. This verifies error/Cancel/Idle/retry and
bounded destruction without a live capture device. It is failure-path evidence,
not desktop recording acceptance. The 40 ms watchdog test exercises the real
callback, not the default 30-second wall-clock wait.

See [verification boundaries](recording-verification.md) and
[build/reproduction](building.md#recording-preparation-tests-076).

## Review-driven restoration and late-callback checks

Restoring a canceled Region preparation rebuilds the original Viewer rather than
showing an input-transparent selection overlay. It also restores the saved window
geometry and maximized/full-screen state. Actual image/video restoration tests
require the original size and a visible loaded workbench, not just an existing
window. Review caught and fixed a toolbar-only minimum-size restoration.

A recorder generation is retired immediately at Idle. Its queued error/state
signals cannot cancel a new Selecting/Countdown attempt while the next recorder
has not yet been created. The injected adapter tests explicitly deliver both
old signals during a new countdown.

Countdown screenshots wait for the actual root item to fill the window and for a
rendered frame. The test checks both bottom-edge background pixels against the
QML background color. A premature first capture had shown the initial minimum
root size, not the resized content; this was corrected in the harness without a
production layout change.
