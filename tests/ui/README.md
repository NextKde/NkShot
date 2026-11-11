# Actual viewer capture

Configure with `-DNKSHOT_ENABLE_UI_CAPTURE=ON` (OFF for release builds).
Create a non-personal fixture using `python3 tests/ui/make_fixture.py /tmp/fixture.ppm`.
Run the built executable:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
NKSHOT_TEST_EDITING=1 NKSHOT_TEST_UI_CAPTURE=/tmp/editor.png \
./bin/nkshot --new-instance --edit-existing /tmp/fixture.ppm
```

The hook waits for the actual ViewerWindow, enables annotation mode, and uses
QQuickWindow::grabWindow. It does not composite a mock UI or replace the canvas.
Optional `NKSHOT_TEST_UI_SIZE=640x660` selects a test window size. It restores
standard icon paths and uses Breeze only when an offscreen platform provides no
theme. It exits nonzero if the viewer or capture is unavailable.

This is a render smoke test, not automated interaction or desktop capture testing.
Software mode cannot validate LiquidBackdrop GPU effects or Wayland recording.

The portable annotation viewport keeps upstream input and document behavior;
only its software rendering node uses the same document's composed image. GPU
platforms retain the upstream paint node. Shader-only viewport color/gamma preview
adjustments are not implemented in this fallback; committed document adjustments
are rendered normally. NkShot currently does not set those preview properties.

Set `NKSHOT_TEST_MORE=1` to open the actual More menu before capture.

Additional capture states: `NKSHOT_TEST_MORE_TOOLS=1`, `NKSHOT_TEST_ZOOM=1`,
`NKSHOT_TEST_TOOL=blur` or `text`, and `NKSHOT_TEST_DARK=1`. Dark mode supplies a
dark QPalette to the actual offscreen application; it does not post-process pixels.

`NKSHOT_TEST_PROPERTIES=1` opens the persistent properties panel. With
`NKSHOT_TEST_TOOL=rectangle`, add `NKSHOT_TEST_CONTEXT_MENU=1` to send an actual
QTest right-button click to an editable stroke input and require the native menu
to become visible within one second. The menu is closed before the window capture.
UI-capture builds link QtTest; default release builds do not.

## Edit/property/Copy/Save transaction test

Use a fresh output filename beginning with `nkshot-ui-test-` and append
`--output /tmp/nkshot-ui-test-edited.png` to the viewer command. Set
`NKSHOT_TEST_PROPERTY_TRANSACTION=1 NKSHOT_TEST_PROPERTIES=1 NKSHOT_TEST_COPY_SAVE=1`.
This creates a rectangle on the synthetic image, changes stroke width through the
actual QML control, closes immediately, checks undo/redo, and checks the actual
Copy/Save action pixels. Existing export paths are rejected.

## Vulkan component material probe

```sh
cmake --build build --target kos_material_probe
QT_QPA_PLATFORM=vkkhrdisplay QT_QPA_DISABLE_INPUT=1 \
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
./build/bin/kos_material_probe /tmp/kos-material
```

This explicitly requested target uses QQuickRenderControl and a Vulkan texture
render target. Outputs are **component harness images**, not screenshots of the
application window or desktop. It uses existing CPU Mesa Vulkan support; no
system security settings, compositor permissions or input-device access change.
The baseline/liquid/reduced-transparency sampling states must be false/true/false.

## Actual selection window with synthetic input

Set `NKSHOT_TEST_CAPTURE_OVERLAY=1` to construct CaptureWindow using the fixture
fitted to the synthetic primary screen. Add `NKSHOT_TEST_REGION_DRAG=1` to send
actual mouse press/move/release events and assert the inclusive selection bounds.
This does not invoke a desktop capture backend or establish Wayland permissions.

For the capture overlay probe, set `NKSHOT_TEST_CAPTURE_MORE=1` together with
`NKSHOT_TEST_CAPTURE_OVERLAY=1` to open the selected-region secondary actions menu.
The region drag probe still uses synthetic pixels, not a desktop capture backend.

`NKSHOT_TEST_NOTIFICATIONS=1` exercises a bounded twelve-warning stack, scrolls
the actual list, clicks a real dismissal button, checks copied-message replacement
and verifies stable workbench geometry. It does not invoke external file handlers.

Add `NKSHOT_TEST_NOTIFICATION_TIMEOUT=1` to the notification test to wait for the
real ten-second transient-message expiry while its row is scrolled out of view.
Allow at least 30 seconds for the complete process. Persistent warnings must remain.

`NKSHOT_TEST_SECOND_DOCUMENT=1` injects an independent document into actual
editor controls, clicks tool/undo/redo, edits a property and confirms the original
screenshot document remains unchanged. Use with `NKSHOT_TEST_EDITING=1`.

Generate a patterned redaction fixture with
`python3 tests/ui/make_fixture.py /path/to/redaction.ppm --texture`.
Use it with `NKSHOT_TEST_EDITING=1 NKSHOT_TEST_REDACTION=blur` (or `pixelate`)
to exercise actual window mouse gestures and pixel/undo/redo assertions.

`NKSHOT_TEST_TEXT_INPUT=1` with editing enabled clicks the real canvas, sends
Latin key events and a Chinese preedit/commit event, then validates annotation
text and undo/redo pixels. Requires a CJK font for the visual fixture; it does not
test a real Fcitx/IBus candidate-selection session.

`NKSHOT_TEST_CROP_GESTURE=1` with editing enabled tests actual drag/right-click
cancel, timed double-click acceptance, crop pixels and keyboard undo/redo.

`NKSHOT_TEST_PIN=1` with editing enabled clicks More → Pin Image, verifies two
independent immutable snapshots, actual copy/topmost-toggle/close controls and
editor-close survival, then captures the real pin window. Topmost flag checks
do not establish compositor stacking behavior.

The pin test accepts `NKSHOT_TEST_PIN_SIZE=260x180` for minimum-size review and
`NKSHOT_TEST_PIN_FEEDBACK=1` to verify Copied feedback returns to Copy after its
actual timer. Clipboard pixels are checked independently of the feedback label.

## Pinned snapshot saving

Set `NKSHOT_TEST_PIN=1 NKSHOT_TEST_PIN_SAVE=1` with the normal synthetic viewer
input and `NKSHOT_TEST_UI_CAPTURE`. The harness selects widget dialogs, tests
cancel/reopen and repeated actions, types a temporary PNG path, verifies saved
pixels equal the immutable pin and confirms the editable source is unchanged.
Production still permits native dialogs. Repeat with
`NKSHOT_TEST_PIN_SIZE=260x180` and `NKSHOT_TEST_DARK=1`. This does not validate
native file portals or the overwrite-confirmation interaction.

`NKSHOT_TEST_EDITOR_MATERIAL=1` asserts the editor's sampling source excludes its
consumers and reports active/sampling state. With QT_QUICK_BACKEND=software both
must be false. Merely setting QSG_RHI_BACKEND=vulkan is not evidence that the
application used Vulkan; the offscreen platform can choose software. Explicit
QT_QUICK_BACKEND=rhi currently fails to create a Vulkan instance in the cloud.

`NKSHOT_TEST_COMBINE=1` drives More → Combine images, the contextual direction/
method chooser and widget file picker. The chooser is canceled/reopened, selected
options are clicked repeatedly and must remain selected. A `.setup.png` captures
the actual configuration window. It tests reuse, cancel/reopen, reverse-selected natural filename order,
exact output pixels and unchanged editor content, then rejects duplicate frames.
The capture is the actual resulting pin containing a synthetic document fixture.
Native file portals and automatic scrolling are not covered.

Add `NKSHOT_TEST_COMBINE_ERRORS=1` to the combine scenario to exercise wrong width,
a damaged PNG file, one-image selection and absent overlap through the actual
picker. It requires the corresponding specific warning and no preview/editor
mutation. The final capture shows the missing-overlap guidance. Duplicate error
text is also checked in the ordinary combine scenario.

`NKSHOT_TEST_OCR_RESULT=1` supplies a synthetic text image to the real core OCR
entry point and waits for actual Tesseract output. It checks the result window,
plain read-only text, unchanged source pixels, Ctrl+A/C selection, Copy all and
Close destruction. Add `NKSHOT_TEST_OCR_CHINESE=1` for the installed chi_sim model,
`NKSHOT_TEST_OCR_COMPACT=1` for 360×260, or `NKSHOT_TEST_DARK=1`. This invokes the
core action, not a menu click, and does not establish real-world OCR accuracy or
native desktop notification delivery.

Add `NKSHOT_TEST_OCR_SAVE=1` to OCR_RESULT to use widget dialogs and test actual
Save clicks, cancellation/reopen, repeated-dialog reuse, rejected .png extension,
exact UTF-8 file bytes and source isolation. Run light/dark/compact/Chinese
variants. Native portal behavior, overwrite prompts and disk failures are not
covered by this harness.

`NKSHOT_TEST_OCR_LANGUAGE=1` with OCR_RESULT opens the actual More/language menus
and clicks the installed eng or chi_sim entry before waiting for real recognition.
The menus are programmatically opened, but the language choice is a mouse click.
Combine with OCR_CHINESE, OCR_SAVE, OCR_COMPACT or DARK for the existing downstream
checks. A pre-click menu image is saved alongside the result with `.menu.png`.

`NKSHOT_TEST_TOOL_KEYS=1 NKSHOT_TEST_EDITING=1` seeds one annotation and sends
actual Alt+1…7 events in dock order, checking tool identity, unchanged rendered
pixels and undo depth. Run separately from TEXT_INPUT/REDACTION tests because
those tests intentionally assert history starting from an unedited document.
Redaction regressions require `make_fixture.py --texture`.

Add `NKSHOT_TEST_HORIZONTAL=1` for left-to-right composition, including a readable
wide synthetic fixture, exact pixels and direction-correct failure messages.
Add `NKSHOT_TEST_STACK=1` to COMBINE for explicit no-overlap composition. The input
fixture is split into two non-overlapping halves. The actual menu, picker reuse,
cancel/reopen, reversed numeric filename selection, exact reconstructed pixels,
preview close and unchanged editor are checked. Stack mode intentionally does
not run overlap duplicate rejection; duplicate retention is covered by core tests.

## Local recording playback

Create a non-personal silent video using an installed FFmpeg:

```
ffmpeg -f lavfi -i 'testsrc2=size=640x360:rate=20' -t 4 -c:v libx264 -pix_fmt yuv420p playback.mp4
```

Set `NKSHOT_TEST_PLAYBACK=/absolute/path/playback.mp4` with the ordinary viewer
fixture/capture setup. The test switches to the real recording view, supplies the
local file, clicks Play/Pause and seeks, checks a real software frame and verifies
file bytes did not change. Run DARK and UI_SIZE=640x420 variants. For a corrupt
MP4, add `NKSHOT_TEST_PLAYBACK_ERROR=1` to check its visible retry flow. This does
not exercise desktop recording, audio, GPU rendering or performance. The screenshot
shows actual decoded test-pattern frames, not a composited stand-in.

For GIF/WebP, generate local fixtures using
`python3 tests/ui/make_animation_fixture.py /path/to/animation` (Pillow with WebP
support). Pass either output to TEST_PLAYBACK plus `NKSHOT_TEST_ANIMATION=1`.
Tests verify actual frame progression, mouse pause/resume and stable paused frame,
hidden seek, empty MediaPlayer source and unchanged input bytes. Run both themes.
Qt's WebP image plugin must be installed; libwebp alone is not enough. Corrupt
animation files use PLAYBACK_ERROR as above. These are not desktop recordings.

`NKSHOT_TEST_MEDIA_SWITCH=1` with a valid TEST_PLAYBACK MP4 runs same-window
transitions. Provide `NKSHOT_TEST_SWITCH_GIF`, `NKSHOT_TEST_SWITCH_WEBP` and
`NKSHOT_TEST_SWITCH_BROKEN` file paths. The broken fixture is copied into a
QTemporaryDir; only that private copy is replaced with valid bytes before an
actual Retry click. Assertions cover stale frame/error clearing, format isolation,
animated frames and recovered video frames, and untouched original inputs.
Transitions assign the view source property; they do not simulate desktop capture.

`NKSHOT_TEST_RESIZE=1` seeds an annotation and drives More/Share a copy/Resize, Escape cancel,
reopen, typed width, Create copy, clipboard and PNG Save As. It checks exact scaled
pixels plus untouched source document/undo depth. The dialog is captured alongside
the output with `.dialog.png`; run DARK and UI_SIZE=640x420 variants. Widget file
dialogs are used, not native portals. Core image_transforms_test covers alpha,
DPR/profile, ownership, invalid widths and pixel/height caps.

`NKSHOT_TEST_SOLID_COVER=1 NKSHOT_TEST_EDITING=1` selects a seeded rectangle, clicks
the Solid cover preset and verifies no selected-mark mutation. A real drag then
checks opaque black interior, unchanged pixels beyond a boundary margin and exact
undo/redo. Use a textured fixture and optionally COPY_SAVE with a fresh explicit
nkshot-ui-test-* output path to verify clipboard and exported PNG pixels.

`NKSHOT_TEST_BEAUTIFY=1` reuses the sharing-copy transaction test, with actual
style-button and watermark-field interaction. `NKSHOT_TEST_PRESENTATION_STYLE`
is 0=Light, 1=Dark, 2=Aurora. It verifies preview/clipboard/saved PNG against the
core composition and unchanged source/history, including cancel/reopen. Run dark
and 460×400 variants. `make_presentation_fixture.py` creates a clearly labeled
synthetic input (Pillow and a configurable font path); it is not a screenshot of
another application. Core tests cover dimensions, corner clipping, transparency,
DPR, tagged P3→sRGB conversion, watermark/footer and invalid/bounded inputs.

`NKSHOT_TEST_TASK_MENU=1` validates grouped image-task rows; add
`NKSHOT_TEST_TASK_MENU_VIDEO=1` for recording-context filtering. Sharing, OCR
language and composition tests open their task submenu through an actual row
click. `NKSHOT_TEST_BEAUTIFY_LIMIT=1` uses a real 8192×200 image: default padding
disables Create, then typed zero padding recovers, preserving all other options.

`NKSHOT_TEST_SCROLL_BACKEND_BOUNDARY=1` is only for the cloud/offscreen test
environment with no session DBus. It verifies null/missing-backend request refusal,
repeat cancellation, and unchanged editor pixels/history. It exits with an error
if session DBus is connected rather than claiming a live-capture test. Its screenshot
is the unchanged existing editor, not a rolling-capture demo.

`NKSHOT_TEST_SCROLL_PREVIEW=1` verifies the actual More entry is discoverable with
`NKSHOT_EXPERIMENTAL_SCROLL` unset, clicks the real entry and checks unavailable
API5 guidance, disabled selection, refused Capture/Finish, no new result pin,
window reuse, Escape/reopen, source/history/clipboard isolation. Use DARK and
SCROLL_COMPACT variants. `NKSHOT_TEST_SCROLL_SELECTOR_VISUAL=1` exercises actual
mouse/key/Tab/Enter/Cancel handlers only after overriding the QML component phase;
the C++ phase stays Setup and accepts no region/frame. Its captures are selector
layout/input diagnostics, not native capture screenshots. SCROLL_VIDEO confirms
the entry and open method remain image-only. No successful native Finish is claimed.


The selector diagnostic now exercises real keyboard events: centered seeding,
move/Shift move, Alt resize, mixed mouse/keyboard adjustment, clamping at all tested
edges/minimums, Tab focus to Cancel/Use region, Return activation and keyboard
cancellation. It verifies too-small regions disable confirmation. `.keyboard-layout.png`
and `.invalid-region.png` remain layout/input diagnostics, with C++ Setup state and
zero captured frames throughout. No compositor success is simulated.


### Pinned-image local text (060)

`NKSHOT_TEST_PIN_TEXT=1` creates an explicitly synthetic text pin and clicks its
real Extract text action. It checks busy/repeat protection, actual OCR, result
copy and UTF-8 save, unchanged source/history/configuration, no automatic
clipboard writes, hidden-pin result deferral and close-during-work suppression.
`NKSHOT_TEST_PIN_TEXT_CHINESE=1` uses Noto Sans CJK SC and real chi_sim data;
`NKSHOT_TEST_PIN_TEXT_COMPACT=1` tests the 260-pixel toolbar.
`NKSHOT_TEST_PIN_TEXT_BLANK=1` and `NKSHOT_TEST_PIN_TEXT_MISSING=1` check no-result
and missing-model UI without a result window or state mutation. The fixture
language setting is restored after each run. These tests need installed eng and
chi_sim models and do not establish native compositor focus or general OCR accuracy.

### Long pinned-image reading (061)

`NKSHOT_TEST_PIN_ZOOM=1` opens an 800×2600 generated text fixture and uses actual
Fit/Width/100%/zoom button clicks, mouse dragging, Ctrl+wheel and keyboard shortcuts.
It checks scale bounds, Fit after resizing, full-resolution clipboard pixels and
unchanged source/history. `NKSHOT_TEST_PIN_ZOOM_COMPACT=1` uses a 260×300 window.
The main screenshot shows the panned 100% view; `.fit.png` shows the initial Fit
view; `.width.png` shows the top-aligned width-fit reading mode. No screenshot is a live captured desktop or a touch/trackpad acceptance test.

### Playback sound/speed controls (062)

Combine `NKSHOT_TEST_PLAYBACK=/path/to/with-audio.mp4` and
`NKSHOT_TEST_PLAYBACK_SETTINGS=1` to use actual popup, mute/unmute, volume slider
and speed-combo interactions, followed by Escape/reopen/outside dismissal.
`NKSHOT_TEST_PLAYBACK_SILENT=1` expects no audio track and disabled sound controls.
The additional `.settings.png` shows the opened popup. Audio-track metadata and
QAudioOutput properties are checked, not physical sound output. Source-file bytes
must remain unchanged. The fixture used for 062 is generated H.264 plus AAC tone.

### 066 quick-note workflow

Set `NKSHOT_TEST_QUICK_NOTE=1` with an image fixture and UI capture path. The test
opens the actual Pins and notes submenu, enters Chinese and English text, checks
empty/oversized disabled actions, copies exact preview pixels, creates an immutable
pin, changes card style, closes/reopens the retained draft and checks original
screenshot/history isolation. `NKSHOT_TEST_NOTE_COMPACT=1` tests 400×560, and
`NKSHOT_TEST_DARK=1` tests the dark shell. These are software-window tests, not
KWin topmost, native compositor capture or GPU material acceptance.

### 067 explicit local code reading

`NKSHOT_TEST_CODE=1` opens the real Read text and codes submenu and decodes a
Prison-generated QR fixture with literal URL/markup and Chinese content. It checks
no automatic clipboard mutation, repeated-request protection, plain-text display,
explicit Copy all, result destruction and source/history isolation. Use
`NKSHOT_TEST_CODE_COMPACT=1` or `NKSHOT_TEST_DARK=1` for visual variants.
`NKSHOT_TEST_CODE_EMPTY=1` with a blank fixture checks one actionable warning and
no result window. Renderer tests additionally exercise real Code128, binary,
transparency/DPI and pixel bounds; this is not an accuracy benchmark.

`NKSHOT_TEST_CODE_STALE=1` hides/reopens the viewer before delivery and requires
no result or warning. `NKSHOT_TEST_CODE_MAXIMIZE=1` maximizes while decoding and
requires a normal result. Successful-result runs also verify UTF-8 export, repeat
scan after close, and result/dialog destruction while Save As is open.


## Image-local color sampling (068)

Generate the synthetic fixture with `python3 tests/ui/make_color_fixture.py /tmp/color.png`;
add `--alpha` for the translucent variant. `NKSHOT_TEST_COLOR=1` drives the actual
Read from image menu and sampler window. It asserts no implicit clipboard change,
initial disabled copy, source-pixel selection, arrow nudging, pointer drag, HEX/RGB
and Ctrl+C copy, resize preservation, repeat-open coalescing, hide/close cleanup,
fresh reopen, video rejection and unchanged source pixels/history. The `.empty.png`
and `.task.png` sibling captures show the initial state and actual menu.

Use `NKSHOT_TEST_COLOR_COMPACT=1` for 400×460 and `NKSHOT_TEST_DARK=1` for dark UI.
The helper's `color_sample_test` covers profile conversion, external pixel memory,
alpha formatting, premultiplied inputs, DPR, bounds and edge neighborhoods. These
are software-window tests with synthetic content, not desktop capture, global
color picking, GPU rendering or native monitor-profile validation.

### Selected-area pinned OCR (069)

Add `NKSHOT_TEST_PIN_AREA=1` to `NKSHOT_TEST_PIN_TEXT=1`. A DPR=2 synthetic pin
contains a desired line and a separate excluded line. The harness clicks the OCR
menu, verifies an empty selection cannot read, exercises Escape and Cancel/reopen,
changes zoom, verifies nonzero pan at 100%, reverse-drags physical-pixel bounds,
checks outside-image clamping and hide-during-selection cleanup, verifies no
implicit OCR/clipboard change,
uses Ctrl+C for the full image, then explicitly reads only the selected line.
The `.selected.png` sibling capture shows the real selection state. Existing
result copy/UTF-8 export, history/pixel isolation, hidden delivery and close-during-
work checks also run. Combine with PIN_TEXT_CHINESE, PIN_TEXT_COMPACT (true 260×180) and DARK.
Whole-image PIN_TEXT now clicks its menu item rather than assuming the icon starts
recognition. Software window tests are not native desktop acceptance.


### Inline OCR word selection (070)

`NKSHOT_TEST_PIN_WORDS=1` generates a DPR=2 two-line fixture and clicks the actual
Select text on image menu action. Add `NKSHOT_TEST_WORDS_CHINESE=1`,
`NKSHOT_TEST_WORDS_COMPACT=1` (260×180), or `NKSHOT_TEST_DARK=1`. BLANK/MISSING
variants use `NKSHOT_TEST_WORDS_BLANK` / `NKSHOT_TEST_WORDS_MISSING` and check
truthful failures without mode activation or clipboard changes.

The harness verifies physical word hit-testing, Shift ranges/reverse drags,
logical multi-line order, arrow/Home/End/Ctrl+A and Tab, explicit text copy and
Ctrl+C, separate whole-image Copy and pixel-equal PNG Save As while in word mode,
ordinary wheel scrolling, nonzero pan/100% mapping, active-word visibility inside
the actual viewport, zoom/word-bound preservation and immutable source/history.
It also checks Escape mode exit and restored image-copy shortcut, cancel while
recognizing, hide/show before completion, reopen, minimize and close during work.
The main screenshot shows a selected range; `.all.png` shows whole-text selection.

`local_text_extraction_test` uses real English/Chinese fixtures to verify word
bounds over source ink, physical DPR/alpha behavior, exact line/word spacing,
reading order, literal markup serialization and complete-metadata limits. Metadata
must reconstruct the full OCR text; incomplete/oversized lists are cleared instead
of presented as successful partial selection. These are synthetic software-window
checks, not general OCR accuracy, native desktop or GPU acceptance.

### Held-edge word-selection scrolling (071)

`NKSHOT_TEST_PIN_WORD_SCROLL=1` creates a 1600×1600 DPR=2 English word grid and
opens the real pin window. Add `NKSHOT_TEST_WORDS_COMPACT=1` for 260×180 or
`NKSHOT_TEST_DARK=1` for the dark palette. At 100% zoom the harness presses a
recognized word, moves once to an edge, then waits across multiple frames. It
requires further pan and selection changes without additional pointer moves.

Checks include all four directions and clamped content boundaries, neutral-center
stop while still pressed, release inside and outside the viewport, diagonal pan,
reverse logical selection order, same-gesture reversal across an anchor that has
scrolled out of view, no blank-start drag, and active-drag cancellation
through Cancel/Escape/hide/minimize/close. Source pixels, editor undo depth and
clipboard sentinel must remain unchanged. A far-outside pointer cannot jump the
endpoint across multiple screens; navigation keys cannot reset a held-drag anchor.
The main capture shows an actual
cross-viewport selected range during the held diagonal drag. These tests use
software-rendered synthetic windows, not native compositor/GPU acceptance.

The 071 timer test also repeats stationary-edge extension and release-stop after
Ctrl+- changes the image scale below 100%, then restores 100% for remaining tests.

Reproduce the focused probe with the existing UI-capture binary and a local input:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
NKSHOT_TEST_UI_CAPTURE=/tmp/071-scroll-light.png \
NKSHOT_TEST_PIN_WORD_SCROLL=1 \
./build/bin/nkshot --new-instance --edit-existing /path/to/local-fixture.png
```

Repeat with `NKSHOT_TEST_DARK=1` or `NKSHOT_TEST_WORDS_COMPACT=1`. The local input
opens the editor; the probe creates its own immutable 1600×1600 DPR=2 OCR fixture.
Use an isolated `XDG_CONFIG_HOME` per invocation. Existing 070 six-variant probes,
`ctest --output-on-failure`, and `python3 tests/nkshot/test_identity.py -v` remain
the regression checks. These probes preserve OCR logical order, not a guarantee
of human reading order for arbitrary multi-column layouts.

### Local video clip export (072)

`NKSHOT_TEST_VIDEO_CLIP=1` with `NKSHOT_TEST_PLAYBACK=/path/to/synthetic-six-second.mp4`
drives the real playback-settings entry and clip popup. The source must be a local
six-second MP4 fixture; do not use private media for public test screenshots.
Repeat with `NKSHOT_TEST_DARK=1` and `NKSHOT_TEST_UI_SIZE=420x520` for dark/narrow
layouts. `NKSHOT_TEST_CLIP_MISSING=1` temporarily removes command discovery in the
probe process and checks the truthful missing-tool error with export disabled.
`NKSHOT_TEST_CLIP_UNSUPPORTED=1` uses an otherwise Qt-playable WebM fixture and
checks the unsupported-input explanation with export disabled without changing
command discovery. Both failure probes verify source bytes and application state
remain unchanged. The normal probe also logs the actual title label font resolved
by `QFontInfo` for visual review, without overriding the application font.

The probe types 1.35 and 3.15 seconds into the actual editable time controls,
previews the bounded interval, exercises Escape/reopen and Save dialog cancellation,
and explicitly saves a newly encoded MP4 with spaces/quotes in its filename.
It selects an existing destination and checks new-file-only rejection preserves its bytes,
cancels an active export, reopens, and switches source during another export to
reject stale completion. The actual Qt media backend then opens and plays the new
file, checking its approximately 1.8-second duration and expected audio presence.
Playback is set to 2× and muted beforehand; these preview preferences must not
alter the exported duration or remove its audio track. Source bytes, current video
identity and image-editing pixels/history must remain unchanged.

The main capture shows the real range popup; `.saved.png` shows successful export.
They are software-window captures of generated media, not evidence of native KWin
recording, audible output, live-device A/V sync or native portal acceptance. The
backend `video_clip_test` separately verifies encoded frame/audio content and
failure/commit behavior; the UI probe does not replace those checks.

`NKSHOT_TEST_CLIP_INDEPENDENT=1` types 1.25–4.75 seconds into the same controls,
exports through the real file dialog, then Qt-decodes/plays the result. Supply the
independently generated 12-second long-GOP source via `NKSHOT_TEST_PLAYBACK`.
The output remains at `/logs/072-independent-export.mp4` for external frame/audio
verification, or the new path in `NKSHOT_TEST_CLIP_OUTPUT`. The probe refuses an
already existing output. All clip variants require the software video preview to
remain visible, taller than 60 logical pixels, and above the open clip panel,
including the 420×520 narrow layout.


### GIF clip export (073)

Add `NKSHOT_TEST_CLIP_GIF=1` to the 072 clip probe with a generated local MP4
source. The test clicks the actual format ComboBox, types 1.35–3.15 seconds,
checks the 0.20-second GIF minimum without silent input clamping, and previews
the interval above the unobscured panel. Save cancellation retains the range;
the dialog must use the GIF suffix/filter. A real GIF with spaces/quotes in its
new filename is exported and inspected for a GIF89a header, infinite-loop
extension and the 20 MiB size limit.

Format switching clears old status/output while retaining the selected range.
The probe sets playback to 2× (and mutes an audio-bearing source), but the export
still has the selected duration’s 10 fps frame count. It rejects an existing
target without changing its bytes, switches format
during an active GIF job to cancel it without creating a target, then checks
explicit Cancel/reopen. The existing Qt AnimatedImage route opens the resulting
file and must show multiple frames, dimensions no greater than 640 on either
axis, actual frame advancement, a runtime wrap at the end, stable pause and
resumption. MP4 playback is restored and source bytes, current video identity,
clipboard and editor history remain unchanged. This is real local media export,
not desktop capture or audible/live-device acceptance.

Repeat with `NKSHOT_TEST_DARK=1` and `NKSHOT_TEST_UI_SIZE=420x520`. For the maximum
range guard, supply the generated 18-second 960×540 source and also set
`NKSHOT_TEST_CLIP_GIF_LIMIT=1`: the initial full-source range and a typed
0–16-second GIF must remain unchanged
in the fields while Export is disabled, and shortening restores export. The
result must decode at 640×360, preserving aspect ratio while downsizing. The
main image records range selection; `.limit.png`, `.saved.png`, `.playback.png`
and optional `.initial-limit.png`/`.long-limit.png` record actual short-limit, success, decoded output
and long-limit states. Backend tests separately check the 10 fps/100 ms delay,
15-second ceiling, downscale/no-upscale and encoded image content contracts.


Combine `NKSHOT_TEST_CLIP_GIF=1` and `NKSHOT_TEST_CLIP_INDEPENDENT=1` for the
independently generated 1280×720, 20 fps long-GOP MP4 source. Actual time entry
selects 1.25–4.75 seconds; the new output in `NKSHOT_TEST_CLIP_OUTPUT` (default
`/logs/073-independent-export.gif`) is retained for external decoding. The UI
probe checks GIF metadata and actual Qt decoding at 640×360 with 35 frames,
then observes looping/pause/resume without changing the source or clipboard.
External analysis remains responsible for precise frame content and duration.

### Desktop portal color picking (074)

The production picker has no test injection hook. The test-only UI build drives
the real More → Pick a color → Screen controls and uses a separately running
portal process on an explicitly private bus.

With a normal development system's dbus-run-session, run:
```sh
dbus-run-session -- env NKSHOT_TEST_PRIVATE_BUS=1 \
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  NKSHOT_TEST_SCREEN_COLOR=1 NKSHOT_TEST_UI_CAPTURE=/tmp/screen-color.png \
  ./build/bin/nkshot --new-instance --edit-existing /tmp/generated-pattern.png
```
Build `screen_color_picker_test` beside the application first. The harness starts
its `--fake ui` subprocess, which returns a real D-Bus sRGB tuple after a short
delay. It checks initial inactivity, pending/success/cancel/error/retry, explicit
HEX/RGB/keyboard copy, source switching and unchanged image/history/video.
Screenshots show simulated portal responses, not native cursor interaction.

Use `NKSHOT_TEST_SCREEN_COMPACT=1` for 420×520 and the existing
`NKSHOT_TEST_DARK=1` for the dark palette. `NKSHOT_TEST_SCREEN_VIDEO=1` verifies
the entry without an image, including the disabled Screenshot source keyboard
path. The existing `NKSHOT_TEST_COLOR=1` regression continues to test the 068
image sampler; its entry moved from Read from image to the main More menu.

For a separately implemented portal on the supplied private bus, set
`NKSHOT_TEST_SCREEN_EXTERNAL=1`. The harness does not start or replace a service.
`NKSHOT_TEST_SCREEN_EXPECT=success|cancelled|error` selects the expected outcome,
and `NKSHOT_TEST_SCREEN_HEX` selects the expected successful 8-bit HEX result.
`NKSHOT_TEST_SCREEN_EXTERNAL_CANCEL=1` clicks Stop waiting and then waits 1.8
seconds to verify that a deliberately late response cannot restore a sample,
change status or modify the clipboard.

The real-protocol unit test covers typed tuple boundaries, early responses,
different returned handles, invalid responses, owner loss, request cancellation,
timeout and delayed-handle cleanup. Run it only on a dedicated test bus:
```sh
dbus-run-session -- env NKSHOT_TEST_PRIVATE_BUS=1 ./build/bin/screen_color_picker_test
```
When dbus-run-session is installed, CTest wraps this test automatically. Otherwise
the caller must provide a private DBUS_SESSION_BUS_ADDRESS and the explicit flag.
No test should register the portal service name on a user's desktop session.
Wayland export, compositor cursor behavior and real KDE/GPU selection remain
separate native acceptance work.

For the genuine empty startup workflow, use `--launchonly` instead of
`--edit-existing` and set `NKSHOT_TEST_SCREEN_STARTUP=1` with the Screen flags.
The harness requires a null image document, clicks the start page's actual More
button, records the menu in `.entry.png`, then drives the full Screen flow.
No synthetic image is loaded merely to make the entry available.
External cancellation can additionally use `NKSHOT_TEST_SCREEN_PICK_MARKER`:
the external test service creates the marker only after receiving PickColor, so
the test cancels a real request rather than guessing a network delay.

## Startup workflow (075)

Build with the existing opt-in `NKSHOT_ENABLE_UI_CAPTURE` configuration. The
production executable contains no environment-based capability override. For a
real empty startup window, use `--launchonly`, not `--edit-existing`:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  XDG_CONFIG_HOME="$(mktemp -d)" \
  NKSHOT_TEST_STARTUP=1 NKSHOT_TEST_UI_CAPTURE=/tmp/startup.png \
  build/bin/nkshot --new-instance --launchonly
```

The hook operates the real QML tabs, target popup, disclosure, options and decimal
delay. It checks equal-count replacements on the actual QML-bound C++ mode models
and restores them, without changing backend capabilities. If Immediate shutter
is supported, the actual primary action starts a **99-second** Core countdown,
then the hook restores the window, changes setup tab, and clicks Cancel long
before `doGrab` can occur. Document/video/clipboard remain unchanged. This is an
integration test of dispatch/cancellation, not native screenshot acceptance.

Use a fresh configuration directory for every run. Additional flags:
- `NKSHOT_TEST_DARK=1`: synthetic dark desktop palette.
- `NKSHOT_TEST_UI_SIZE=420x520` (also checked at 360x480 and a requested 360x360).
- `NKSHOT_TEST_STARTUP_LARGE_FONT=1`: 1.5× application font.
- `NKSHOT_TEST_STARTUP_RTL=1`: right-to-left layout.
- `NKSHOT_TEST_STARTUP_ERROR=1`: visibly labeled synthetic error to stress layout.

The requested size is subject to the content-derived minimum height, particularly
with large text and an error banner. Options scroll; Cancel and the footer remain
inside the window. The test also edits Delay and microphone preferences through
actual controls to demonstrate that short viewports remain operable.

`startup_setup_test` loads the same production presentation component with
synthetic settings and models. Its ten QtTest results cover all target values,
repeated dispatch, missing/changing capabilities, same-count replacements,
external selection changes, cross-tab cancellation, all retained option
dependencies, decimal delay, keyboard disclosure, RTL and minimum-size text.
It cannot trigger a native screenshot or recording. Run the full suite with
`ctest --test-dir build --output-on-failure`; the portal test still requires the
isolated private-bus arrangement documented above.

Normal shutdown is checked both with and without `--new-instance`. The 075
shutdown fix destroys QML views before their engine and destroys Core before
QApplication's GUI services. A plain `--launchonly` capture/exit reproduced the
pre-fix crash independently of the interactive harness.

### Recording preparation (076)

`NKSHOT_TEST_RECORDING_ADAPTER=1` drives the real Wayland adapter with explicit
constructor-injected providers. Ten scenario groups include the post-selection
clock, no Countdown output/stream calls, successful state/timing/save dispatch,
Starting cancellation/drain, stale callbacks, dynamic capabilities, path limits,
and the native KPipeWire missing-configuration failure bridge. This is not a
successful native recording test.

`NKSHOT_TEST_RECORDING_PREPARATION=1` requires the real Null video platform. It
injects only a synthetic confirmed region into the actual Core/selection route,
then operates the actual countdown QML. Run separately with `--launchonly`,
`--edit-existing` on a synthetic image, `NKSHOT_TEST_RECORDING_PREPARATION_VIDEO=1`
(with synthetic `/logs/playback062.mp4`), and `NKSHOT_TEST_DARK=1`. Checks include
button/Esc/close, original media/history/clipboard, original Viewer size and
visible content, no unintended screenshot signals, queued old selection results,
zero-delay no-flash, readiness rejection and cross-workflow cancellation.
Screenshots identify synthetic selection results; they are not screenshots of a
native selection or proof of compositor capture. Use an isolated test session
bus and fresh configuration, never a user's desktop session or private media.

### Private recording output cleanup (077)

The adapter harness now runs 17 groups, including real synthetic file writes with
a held QFile producer, cancellation/drain/Idle cleanup, protected replacements,
explicit NewOnly reservation and success/error retention. Its JSON includes the
first actual preservation warning. Set `NKSHOT_TEST_RECORDING_CLEANUP_WARNING=1`
alongside `NKSHOT_TEST_RECORDING_ADAPTER=1`, `NKSHOT_TEST_STARTUP=1` and
`--launchonly` to forward that warning through the production Core/InlineMessage
route and capture the real warning layout. This bridge is test-only, with no
capability override and no native-recording claim.

## MP4 sharing dimensions (102)

Add `NKSHOT_TEST_RESOLUTION_CLIP=1` to `NKSHOT_TEST_LOCAL_MEDIA_OPEN=1`.
The actual Details combo selects Max 1280 px, checks no enlargement and exact
final width/height, rejects a Save dialog after its setting changes, exports a
real MP4 and probes its dimensions, then checks fresh-session Original reset.
Use a landscape 1600×900 MP4 for a real 1280×720 reduction; a 640×360 WebM checks
no enlargement and the unchanged GIF path. DARK/COMPACT and existing crop/privacy/
annotation/audio flags apply. Test pointer clicks wait for Flickable movement to
finish so they activate controls instead of merely stopping kinetic scrolling.

### Export speed (103)

`NKSHOT_TEST_SPEED_CLIP=1` selects actual 2× export in Details, tests a stale
Save dialog after changing speed, checks the resulting MP4 duration against the
preview, source preservation and fresh-session reset. Combine with crop/privacy,
annotation/resolution/silent flags; WebM also exercises actual GIF export. This
is software-window UI, not native KDE capture acceptance.

### Input/edited comparison (104)

`NKSHOT_TEST_COPY_EDITOR=1 NKSHOT_TEST_COMPARE_EDITOR=1` extends the local-media
workflow with real hold/release, keyboard Space, drag-out, focus-loss, hide/reopen,
copy output, immutable full-input pixels, crop/aspect ratio, history and viewport
preservation. `.compare.before.png`/`.compare.after.png` and cropped variants are
actual software-window evidence, not native compositor acceptance.

### Shared editor navigation (105)

`NKSHOT_TEST_COPY_EDITOR=1 NKSHOT_TEST_COMPARE_EDITOR=1
NKSHOT_TEST_NAVIGATION_EDITOR=1` checks real wheel bursts and pointer anchoring,
middle-button pan with unchanged drawing tool/history, Fit/100%, focused SpinBox
shortcut guard, cropped DPR2 image, focus cancellation and resized bounds. Evidence
suffixes include `.navigation-detail.png` and `.navigation-fit.png`.

### Explicit QML garbage collection (105)

Add `NKSHOT_TEST_GC_WINDOWS=1` to local-media/copy-editor checks. Before video
import, the helper calls the real Quick Note, Color Picker and Scroll Preview
invokables through JS, drops their wrappers, then performs two GC/deferred-delete
cycles. It verifies C++ ownership, Quick Note visible/hidden/reopened draft and
normal Color Picker/Scroll Preview close destruction. Actual frame-pin Edit and
Done flows also check GC survival and edited-pin close destruction. This caught
a real pre-existing visible-window collection bug before the ownership fix.
