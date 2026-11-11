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

`NKSHOT_TEST_COMBINE=1` drives More → Combine screenshots and the widget file
picker. It tests reuse, cancel/reopen, reverse-selected natural filename order,
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

Add `NKSHOT_TEST_STACK=1` to COMBINE for the explicit no-overlap action. The input
fixture is split into two non-overlapping halves. The actual menu, picker reuse,
cancel/reopen, reversed numeric filename selection, exact reconstructed pixels,
preview close and unchanged editor are checked. Stack mode intentionally does
not run overlap duplicate rejection; duplicate retention is covered by core tests.
