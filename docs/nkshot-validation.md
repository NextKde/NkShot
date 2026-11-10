# NkShot UI v1 validation

## Completed locally

- Inspected upstream QML components; platform capture implementation remains unchanged.
- Namespaced the runtime, settings and installed metadata for independent NkShot use.
- Fourteen identity/action/editing-route/material static regression tests pass.
- Generated desktop entry passes desktop-file-validate.
- Preserved each modified source file's copyright/SPDX header.
- Reviewed source diff for whitespace errors and action/shortcut preservation.

## Actual build checks

- Complete NkShot C++/QML build and staged install pass using Qt 6.11.2 / KF 6.30.
- Official filename CTest: 1/1 pass.
- Official KQuickImageEditor image-processing CTests: 3/3 pass.
- The compiled application responds with `nkshot 6.8.3`.
- Install manifest uses independent NkShot names, with no Spectacle-named artifacts.
- Five changed QML files also pass the Qt 6.11.3 parser.
- Standalone Kos.Ui software fallback renders; that is not the application UI.

## Not run yet

Actual KWin capture, recording, editing interactions, GPU liquid optics and
side-by-side application coexistence have not passed runtime acceptance.
The cloud environment currently rejects the local display sockets required by
Xvfb/KWin. See the iteration log; no screenshot-success claim is made.

## Required before release

- Run configure/build, upstream CTest suite and Qt qmllint with generated imports.
- Launch on a matching KWin Wayland desktop, including normal permission checks.
- Capture fullscreen, window and region; accept, cancel/Escape, save, save-as, copy.
- Repeat capture after cancel and after saving; ensure no stale overlay/focus.
- Test recording start/stop, OCR availability and every export action.
- Test annotation tools, undo/redo and options panels joining their parent panel.
- Test light/dark palettes, keyboard focus, tooltips, disabled and checked states.
- Test 100%, 125%, 150%, 200% scaling, mixed-DPI screens, negative screen origins.
- Test tiny/edge/cross-screen selections and draggable toolbars without clipping.
- Verify controls and shadows are absent from final screenshots/recordings.
- Verify narrow screens and long translations; increased panel padding may wrap.
- Test the distinct NkShot identities with a staged install; verify no files overlap
  Spectacle, both can launch independently, and KWin authorizes normal capture.
- Verify translations compile under the nkshot domain and notifications resolve.
- Assign optional global shortcuts manually and check no Spectacle bindings change.
