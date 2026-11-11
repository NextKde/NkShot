# Export a local video clip (072–073)

In the completed video's **Playback and clips** settings, choose **Export a clip**.
Set Start and End in seconds, or drag the two range handles. Preview plays only
that interval of the original source in the visible video above the temporary
clip panel. It is not a preview of encoded GIF colors, size or silence; those
apply to the exported file. Export opens
a Save dialog for a **new file** in the selected MP4 or GIF format. Existing destinations, the original, and
source-equivalent hard links are rejected rather than overwritten. Choose another
name if a destination already exists. After success, Done closes the panel; Export
can save the selected interval again under a different new filename. Nothing is
uploaded or copied automatically.

The original file, the current video identity and image editing history stay
unchanged. Playback speed, mute and volume are preview preferences, not export
parameters. MP4 retains a supported source audio track even when playback is
muted. GIF is always silent and loops continuously. Cancellation, source changes, hiding/minimizing and closing cancel pending
work and remove incomplete output. A failed export leaves the destination alone.

## Deliberately narrow first version

- Local, readable regular MP4 files up to 2 GiB and two hours.
- One SDR H.264 8-bit 4:2:0 video track, even dimensions up to 4096 per axis,
  a valid frame rate up to 120 fps, and zero-based stream start times.
- Zero or one mono/stereo audio track. Extra video, audio, subtitle/data tracks,
  attached artwork, rotated/HDR media and unsupported layouts are rejected rather
  than silently dropped.
- A selected interval of at least 0.10 seconds inside the inspected duration.
- New H.264/AAC (48 kHz audio) MP4 output, not a lossless keyframe-only stream copy. Video begins
  on an available frame; audio/video rounding can differ by up to one video frame.
  Metadata and chapters are intentionally not copied. General variable-frame-rate,
  color-managed/HDR, rotated and arbitrary multi-track media are not certified.

## GIF Maker preset (073)

Choose **GIF** beside the panel title to create an actual animated GIF from the
same MP4 source. The preset is silent, infinitely looping, 10 fps, at most 640
pixels on the longest side without upscaling, and at most 20 MiB. Select 0.20–15
seconds. Switching format keeps your range; an out-of-budget selection is explained
rather than silently shortened. GIF timing is quantized to 100 ms frames, so the
selected boundary may differ by one output frame. Limited GIF colors and
downsampling are not a lossless conversion.

Changing format stops preview, cancels stale work and clears the prior result.
The Save dialog uses the matching suffix/filter; a mismatched explicit extension
is rejected, not silently renamed. Format selection is disabled while that dialog
is open. MP4 remains the initial format each time the panel opens.

Palette creation and GIF encoding are two separate bounded passes using a small
private full-interval palette file, not a split graph that caches the whole movie.
Both passes accurately seek to the selected start before decoding the interval,
rather than decoding a long recording’s entire prefix twice. The palette
and unfinished output are removed on cancellation/failure. Verification reads
one finite cycle even though the published GIF loops indefinitely.

## Dependencies and output safety

FFmpeg and ffprobe must already be installed by the system package manager. MP4
requires `libx264` and (for audio) AAC encoding. GIF instead requires the
GIF/PNG encoders and palettegen/paletteuse filters; it does not require AAC or libx264. NkShot checks them and gives a
recoverable error if they are missing. The application never downloads or installs
media software. The isolated development environment used official Debian FFmpeg
9.0.2 packages, extracted inside its own workspace without upgrading the host.

Media commands use an executable path and separate arguments, never a shell
command. Source inspection and encoding force the MOV/MP4 demuxer, allow local file protocol
only, and disable external data references/absolute MOV aliases. GIF verification
explicitly uses the GIF demuxer and ignores loop repetitions. Work is
asynchronous with bounded metadata/log buffers, source/output limits, operation
timeouts and limited decoder/filter/encoder threads. Source and target identity
checks detect replacements or changes during work.

The job encodes into its own private temporary file in the destination directory.
Successful encoder exit alone is insufficient: metadata, expected tracks/size/
duration and a complete error-fatal decode are checked before publication. New
output is published atomically without replacing a name that appeared meanwhile.
Cancellation invalidates the operation before stopping the subprocess, and only
then removes its temporary output. Synchronous change-signal cancellation is also
covered; an old job cannot report a new source's clip as saved.

## Validation boundary

Tests use generated local media, not private recordings. Backend tests inspect
non-keyframe cuts, decoded first/last frame content and an audio pulse's relative
position; additional cases cover silent/short clips, invalid ranges/files,
corrupted encoded data, unavailable tools, encoder failure, cancellation, changed
sources, existing targets and names containing spaces/quotes. Actual Qt UI tests
exercise range input/preview, save/cancel, errors/reopening and playback of the
newly exported file in light/dark/narrow software-rendered windows.

This is recording-file post-processing. It does **not** establish real KWin
capture, microphone/system audio capture, device A/V synchronization, pause/resume,
native save portals or GPU acceptance. See [recording verification](recording-verification.md)
and [UI test reproduction](../tests/ui/README.md#local-video-clip-export-072).

The shared real probe-timeout path is tested. The five-minute GIF palette/encode
timeout itself has not been waited out in acceptance testing; it uses the same
termination and epoch cleanup mechanism.
