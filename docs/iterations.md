# NkShot verified iteration log

The requested long-running quality target is up to 200 useful iterations, not a
claim of completed work. Each code increment must compile before the next broad
change. Runtime/visual checks are recorded separately from source checks.

## 001 — Obtain a real build and fix dependency validation

Change: require KPipeWire >= 6.8 explicitly. Its 6.7.4 package configured but failed
compilation because four microphone/system-audio APIs were absent. Build the
matching official 6.8.3 library; retain the recording features unchanged.

Verified:
- Complete NkShot C++/QML build and staged installation succeeded.
- Application version command returned `nkshot 6.8.3`.
- NkShot filename CTest passed (1/1).
- Official KQuickImageEditor processing tests passed (3/3).
- Source identity/material checks passed (14/14).
- Installed application artifacts are independently named NkShot.

Not accepted yet:
- Real screenshot/recording capture, editing interaction and GPU material visuals.
- Side-by-side coexistence with a running system Spectacle.
- Xvfb could not open local sockets in the cloud runtime, including a supported
  elevated attempt. No security settings were disabled to force it.

Screenshot: none for this compile-only iteration. A standalone material fallback
image is not presented as a screenshot of the application.

Next: render the actual editor with a deterministic test image in a test-only
configuration, inspect its pixels, then make small UI/action-hierarchy changes.

## 002 — Correct runtime application identity

The first real offscreen editor launch revealed a warning that KAboutData still
advertised `org.kde.nkshot` even though the QGuiApplication ID was
`org.nextkde.nkshot`. The early QCoreApplication setting alone was overwritten by
KAboutData's defaults. Set both the organization domain (`nextkde.org`) and desktop
ID on KAboutData explicitly, and strengthen the source regression test.

Verified: incremental compile/link passed; filename CTest passed (1/1), source
checks passed (14/14), and the actual editor process stayed running in an offscreen
smoke launch without the identity-mismatch warning. The separate test-root MIME
cache was generated, removing the missing PNG MIME warning.

Not accepted yet: a live D-Bus service/coexistence test and screenshot backend.
The smoke launch was stopped by its test timeout, not by a full interactive test.
A pre-existing zoom-field context-menu signal warning remains to investigate.

## 003 — Real editor render and software canvas fallback

The first actual offscreen capture exposed a white image canvas and missing icons.
The upstream annotation viewport uses a custom GPU material unsupported by Qt's
software scene graph. Add PortableAnnotationViewport, keeping the original
AnnotationDocument/input API and GPU renderer while displaying the document's
composited image on software platforms. An opt-in developer-only capture hook
restores the otherwise absent offscreen icon theme and saves the real window.

Verified: incremental C++/QML compile/link passed; filename CTest 1/1 and static
checks 14/14 passed. Actual editor launch exited successfully and produced
[the unmodified 960×660 window capture](screenshots/003-editor-software.png),
with the synthetic test image and annotation icons visible. This is the existing
editor layout, **not Apple-style UI acceptance**.

Not yet validated: individual annotation gestures, save/copy interaction, GPU
material appearance, Wayland capture/recording, viewport-only color/gamma preview
on software. No desktop/session security limits were bypassed. Next: editor
interaction regression tests and high-frequency/secondary action layout.

## 004 — Primary action hierarchy and shared surfaces

Keep Capture / Edit / Copy / Save visible; aggregate Save As / OCR / Export /
Recording / Options in More. Preserve the original actions and platform gates.
Use unmodified NextKdeAppDemo KosSurface for calm, rounded solid toolbar buttons.
At narrow widths, icon-only controls retain their text/accessibility/tooltips.

Verified: each QML/C++ step incrementally compiled; filename CTest 1/1 passed.
The recording source check was updated to verify the actual menu route and
supportedRecordingModes gate rather than requiring the removed button class.
Static checks 14/14 pass. Actual window renders reviewed at
[960px](screenshots/004-editor-toolbar.png),
[640px](screenshots/004-editor-toolbar-narrow.png), and with the
[More menu open](screenshots/004-editor-more.png). No primary-action clipping was
observed. No full native-menu invocation/save/clipboard interaction test yet.

The image is intentionally a synthetic fixture. These are software-rendered
window captures, not glass-effect or complete Apple-style acceptance. The
annotation sidebar and native menu styling still need a coordinated review.

## 005 — Editor regression tests and render-thread safety

Review found a potential lock inversion: calling the upstream document renderer
inside a scene-graph paint callback can wait for the GUI thread's pending timer.
No hang was reproduced, but the implementation now pre-composes an immutable
image in GUI-thread updatePolish; the render callback only consumes that snapshot.
Document repaints invalidate the snapshot, reducing repeated full-image work
during scrolling. Match upstream fractional-DPR crop rounding.

New QtTest verifies rectangle creation, pixel changes, undo, redo, nonzero-origin
crop, PNG save/read pixel equivalence, and actual software viewport repaint after
undo/redo. The initial PNG comparison exposed premultiplied/nonpremultiplied
QImage format differences; comparison now normalizes storage format, not pixels.

Verified: incremental build passed; CTest 2/2 passed. The editor test also passed
with QSG_RENDER_LOOP=threaded and a 20-second timeout. This exercises the requested
software loop configuration, not a real Wayland desktop. Static checks 14/14 pass.
[Actual editor screenshot](screenshots/005-editor-snapshot.png) was reviewed after
the threading change: image and controls remain visible.

Still not covered: physical mouse gestures, large-image performance, all annotation
tools, fractional-DPR matrix, GPU visuals, desktop screenshot/recording. User
rejected the original editor appearance; the next visual iteration must redesign
the workspace, not merely recolor toolbar buttons.

## 006 — Workspace composition, not just buttons

Replace the full-height tool rail with a rounded two-column floating tool palette,
add canvas breathing room, and use the same KosSurface vocabulary as NextKdeAppDemo.
Entering edit mode now fits large images instead of cropping their view at 100%;
small images are not enlarged. All original annotation tools remain present.
Reduced-motion preference now also covers workspace and zoom transitions.

Verified: both incremental builds passed; CTest 2/2, static checks 14/14 passed.
Actual [960px](screenshots/006-editor-workspace.png) and
[640px](screenshots/006-editor-workspace-narrow.png) captures were reviewed. The
image is fully visible and controls are not clipped. This is an intermediate
workspace redesign, not user acceptance: the palette still needs clear grouping,
and the bottom properties/zoom area needs a coordinated compact treatment.

Reference: NextKde NkNotes 014 light/013 dark layouts favor calm content planes,
wide margins and a few capsules rather than glass on every control. GPU liquid
materials remain unverified in this software-only runtime.

## 007 — Centered tool dock and secondary annotations

Remove the sidebar allocation entirely. Center the image workspace and place a
compact floating annotation dock underneath, outside the document coordinate
space. Highlighter, line, ellipse, blur and number remain available in More tools;
core pan/crop/select/freehand/arrow/rectangle/pixelate/text tools stay visible.
The original full tool collection remains used by the capture overlay.

Verified: incremental builds passed; CTest 2/2 and static checks 14/14 passed.
Reviewed actual [960px workspace](screenshots/007-editor-dock.png) and
[640px with More tools](screenshots/007-editor-more-tools.png). Image and dock are
not clipped. The menu opens and retains complete secondary tool labels. Selecting
every tool and native clipboard/save flows are still pending interaction tests.

Remaining visual work: tool grouping/current-tool indication, coherent property
popover, modern zoom controls, dark/high-contrast review. No finished Apple-style
claim or GPU-glass acceptance is made.
