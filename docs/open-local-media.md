# Open a local image or recording

Use **More → Open…** on the startup page or viewer, or **Ctrl+O**. Select a local
file; NkShot validates it before replacing anything. Cancel, a decoding error,
unsupported media or a changed source leaves the current image, edits and history
intact. If content is already open, a separate confirmation explains that opening
the new file replaces the current working content and edit history. Cancel is the
default. This is a single-document workflow, not a document/tab system.

## Supported scope

- PNG, JPEG and static WebP: up to 64 MiB, 40 million pixels, 32768 pixels on either
  side and 192 MiB decoded allocation. This includes the current 40MP scrolling
  result budget. Animated WebP is not imported as a still image.
- Short H.264 SDR MP4: at most one video and one optional AAC audio stream,
  256 MiB, ten minutes, 4K dimensions and 60 fps. FFmpeg and ffprobe must already
  be installed. Probe and complete decode have bounded deadlines (15/60 seconds).
  A valid but expensive file may exceed that budget and be rejected safely.
- Remote URLs, playlists and unsupported formats are not imported. MP4 inspection
  and decoding force the MOV demuxer and local-file protocol. Opening does not
  install software or enable networking.

The limits are deliberate first-release boundaries. This is not a promise to open
all recordings or all formats supported by the playback engine. Longer recordings
can still use their external player; they are not silently truncated by Open.

## Editing and saving

Opened images use the existing annotation, Copy and Save As workflow. **Ctrl+S**
opens Save As for imported media, suggesting a `-copy` filename rather than writing
back to the input. An explicitly chosen existing destination still follows the
normal overwrite confirmation. Existing MP4s use the current playback and clip/GIF
export panel. Full-file Save As copies an imported recording, even if its path
resembles an application temporary directory; it never moves the imported source.

Opening is blocked during recording selection/countdown/start/recording/pause/
rendering, an outstanding screenshot capture, busy clip export or a pending Save
As transaction. Complete or cancel that task before switching files. A recording
that has fully finished is an idle state and may be replaced.

## Validation boundary

Generated local image/video fixtures and software-rendered Qt windows exercise the
actual file dialogs, decoders, document replacement, playback and export routes.
They do not validate native KDE portals, GPU rendering, device recording or capture.
