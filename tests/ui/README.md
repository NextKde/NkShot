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
