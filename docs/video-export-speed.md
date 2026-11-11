# Faster recording exports (103)

**Export a clip → Details → Export speed** offers 1×, 1.5× and 2×.
The selected source range stays unchanged; the exported file becomes shorter.
The summary shows the resulting duration. This is separate from playback speed:
preview and the original recording are not modified.

MP4 retains audio unless the audio checkbox is off. Acceleration preserves pitch
through FFmpeg's tempo filter; it is not speech enhancement. GIF remains silent.
Static annotations, opaque privacy cover, crop and output resolution compose with
speed. Privacy stays above annotations. No cloud service or source overwrite is
involved. This is post-processing, not live drawing or real-time accelerated capture.

The default and each new export session use 1×. Format switches retain the choice.
An open Save dialog cannot silently pick up a later speed change, and encoding
locks settings. Export needs at least 100 ms of output for MP4 or 200 ms for GIF.
GIF still limits the selected **source** range to 15 seconds; increasing speed does
not bypass that cost limit. Frame cadence and container granularity can cause small
rounding differences from the displayed duration.

## Verification boundary

Software-window tests and FFmpeg fixtures check exported duration, audio, source
preservation, settings reset and Save-dialog races. Native KDE/KWin capture,
real microphone/system-audio devices and GPU behavior remain unaccepted. The
preview100 binary predates this source feature.

## Recorded results

Build and source checks pass (27/27 static). Focused speed tests pass 6/6,
including MP4/WebM at both speeds, nine actual short exports, exact video frame
counts, sparse-frame retention, 440 Hz pitch dominance, compressed audio cue timing,
source hashes, silent output and cancellation cleanup. Actual Qt light/dark/420×520
workflows pass, including new-file Save races and defaults reset. Dark combines
static annotations, opaque privacy cover, reframed crop and removed audio; compact
WebM also produces and plays a real accelerated GIF. Screenshots are actual
software-rendered windows, not mockups or native desktop capture.

The first two aggregate runs each passed 26/27, with different pre-existing
short-deadline tests failing: portal responses returned cancellation in the first
run; a crop format-switch wait exceeded 10 seconds in the second. The portal
suite and crop case passed isolated rechecks (9.02 s and 2.636 s). New speed cases
passed in both aggregate runs. No timeout was widened and no production behavior
was changed to conceal these intermittent test failures.

A final quiet serial aggregate run passed **27/27 in 175.95 seconds**. The earlier
failures above remain recorded rather than being described as an uninterrupted
clean run. This does not establish native desktop or device acceptance.
