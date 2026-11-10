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
