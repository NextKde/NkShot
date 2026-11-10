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

## 008 — Zoom controls and truthful selected-tool state

Replace the permanent numeric zoom field with compact minus / percentage / plus
controls. Percentage opens Fit to view, Actual size and an editable custom value,
so numeric entry is retained. More tools now shows the selected secondary tool
and menu check state. More surfaces use the unmodified KosSurface component.

Visual review caught a zoom popup overflowing the right edge. After correcting
its anchor and rebuilding, the actual [640px popup capture](screenshots/008-editor-zoom-final.png)
fits inside the window. Blur is selected through the test hook so its existing
strength control remains visible. Build steps 018–020 succeeded; CTest 2/2 and
static checks 14/14 passed. The prior invalid zoom TextInput Connections warning
is removed with the replaced permanent field. No claims about all popup
interactions, clipboard, Wayland capture or GPU material acceptance.

Next: move property groups into a persistent, non-clipping panel and review dark
appearance. Preserve the existing 250ms property-transaction commit semantics.

## 009 — Actual input, zoom/DPR and document switching matrix

Extend EditorDocumentTest only. Add real QTest mouse rectangle gestures at zoom
0.5/1/2 and image DPR 1/1.25/1.5/2; compare against document-coordinate rendering.
Add freehand/highlighter/line/arrow/ellipse undo/redo, document replacement and
old-document isolation. Use QTRY assertions rather than fixed sleeps.

Verified: incremental editor_document_test compilation passed. Eight separate
processes (QT_SCALE_FACTOR 1/1.25/1.5/2 × QSG_RENDER_LOOP basic/threaded) each
reported 22 passed, 0 failed, 0 skipped, with a 25-second timeout. Software/offscreen
only. No UI source changed in this iteration; the 008 runtime screenshot remains
the current visual baseline.

Known discrepancy: geometry tests disable shadows to isolate coordinates. With
shadows enabled and image DPR > 1, earlier mouse-path versus direct-document
comparison showed an approximately one-pixel right-edge difference. It may be an
upstream incremental shadow repaint boundary; not diagnosed or fixed yet. Original
tests retain their default shadow setting. Do not interpret this matrix as all
annotation tools, text/IME, full QML interaction or GPU/desktop acceptance.

## 010 — Injected editor model and dark palette review

AnnotationEditor now requires an explicitly supplied editorDocument. Screenshot
and capture-overlay callers provide their document; the reusable canvas no longer
fetches the screenshot singleton. This is one boundary improvement, not a claim
that the complete UI has become an independent photo-editor library.

The first actual dark-palette run exposed black labels/icons on dark surfaces.
Bind viewer palette roles to Kos.AppTheme semantic colors and explicitly tint
monochrome action icons with buttonText. After each correction, rebuild and render.
The final actual [light](screenshots/010-editor-light-final.png) and
[dark](screenshots/010-editor-dark-final.png) windows were visually reviewed: labels
and tools are legible and the layout is unchanged. The test-only hook supplies a
dark desktop palette because the offscreen platform has no theme service; these
are real controls, not recolored screenshots.

Verified: incremental builds 021–024 passed; CTest 2/2 and updated static identity/
injection checks 14/14 passed. Required-property injection was exercised by the
actual viewer; capture overlay compiles but awaits a live compositor test.
Remaining: full high-contrast/disabled-state audit, tool properties panel, live
system theme changes, text/IME and native screenshot/recording workflows.

## 011 — Persistent contextual property panel

Move the contextual property groups off the permanent bottom strip into an
anchored, scrollable panel. The same component supports the original horizontal
capture toolbar and a vertical property layout; stroke/fill/strength/font/number/
shadow controls are not removed. The instance remains alive while the popup is
closed, and closing flushes the existing 250ms selected-item commit timer.

Verified: builds 025–027 passed; CTest 2/2 and static checks 14/14 passed. Actual
[text properties](screenshots/011-editor-properties.png) and
[640px dark rectangle properties](screenshots/011-editor-properties-dark-final.png)
were reviewed. The first dark popup had black labels, which was corrected by
explicit semantic popup palettes and re-rendered before this checkpoint.

Not yet accepted: rapid property-change/close undo transactions, all font-dialog
flows and keyboard/IME. A pre-existing context-menu signal warning remains in
the property spinbox and needs a separate Qt 6.11 compatibility fix. The model
flush path is implemented but not claimed as a full QML interaction regression.

## 012 — Fix reproduced upstream shadow repaint clipping

Isolated reproduction confirmed the prior edge discrepancy exists in upstream
AnnotationDocument without any NkShot viewport or mouse input. Incremental repaint
expanded only left/top bounds, clipping faint right/bottom shadow pixels. Apply
a one-line symmetric repaint expansion to pinned KQuickImageEditor and keep the
patch in third_party/patches with reproducible build instructions.

Verified: 48 DPR/origin/size cases changed from 13 mismatches to 0; all full-render
SHA256 values remained unchanged. The linked library was rebuilt/installed in the
isolated prefix. New shadow_repaint_test checks the same 48 cases. NkShot CTest
3/3, official KQuickImageEditor CTest 3/3 and static checks 14/14 pass. Actual
[editor after dependency replacement](screenshots/012-editor-shadow-regression.png)
was rendered and reviewed.

Scope: this fixes reproduced rectangle shadow incremental-vs-full rendering,
including a fractional-origin DPR1 case. It does not prove every transform,
object-deletion or multi-tool shadow scenario. It is a local patch, not claimed
to be accepted upstream.

## 013 — Correct opt-in global shortcut registration

Correction to earlier coexistence wording: independent component naming did not
by itself remove Spectacle's default Print/Meta combinations. Startup still called
setGlobalShortcut with those defaults, potentially claiming currently unused keys
(existing conflicts are rejected by KGlobalAccel, not forcibly stolen).

Register every original action with an empty default and Autoloading so existing
user-customized NkShot shortcuts are preserved. Do not use NoAutoloading or alter
Spectacle configuration. No automatic removal of previously saved NkShot keys is
attempted because they cannot be reliably distinguished from user choices.

Verified: incremental compile/link passed, CTest 3/3 and static checks 15/15 pass,
including a new check of the actual startup registration path. No UI layout change;
012 remains its visual baseline. Real KGlobalAccel first-launch/custom-key/restart
coexistence validation requires a live desktop session and has not been run.

## 014 — OCR readiness and real menu/input regressions

Fix OCR availability notification independently of processing status: initial
Ready→Ready previously emitted no signal, so the menu could stay hidden after
the engine initialized. Add an asynchronous OCR regression with a generated
English fixture; it recognizes HELLO NKSHOT 12345, verifies Processing→Ready,
returned eng language, clipboard text and invalid-image rejection.

Fix Qt6.11 text-input context handling with ContextMenu.requested. A first attempt
with TapHandler was canceled by Qt's built-in context menu and failed the actual
mouse test; use the supported attached request and disable the duplicate built-in
menu. The development-only QTest click now opens the actual native text menu,
then closes it before capturing the editor.

Actual menu review also found KosSurface's small implicit width had clipped menu
labels. Set explicit usable widths, semantic icon tints and disabled opacity.
Reviewed [light OCR menu](screenshots/014-editor-ocr-menu-final.png),
[640px dark OCR menu](screenshots/014-editor-ocr-menu-dark-final.png), and the
[editor following the right-click test](screenshots/014-editor-context-tested.png).

Verified: main incremental builds through 036, CTest 4/4, source checks 15/15,
actual native context-menu mouse regression and visible OCR entry. Dev UI capture
builds link QtTest; normal release builds remain capture-hook-free.

Not claimed: Chinese OCR accuracy, arbitrary document recognition accuracy,
translation, real desktop clipboard ownership after exit, text IME or GPU/Wayland
acceptance. OCR data/fonts used in the test are official installed dependencies.

## 015 — Actual edit/export transactions and RHI material validation

The full-window test changes an actual QML stroke control, immediately closes its
property popup, and verifies undo/redo pixel equality. It then calls the real
ViewerWindow Copy and Save actions and compares clipboard/exported pixels with
the annotated document. This found --edit-existing returned before parsing
--output, so Save could overwrite the input despite an explicit destination.
Resolve output before that branch. The fresh-output test now passes and the input
SHA256 is unchanged. The test refuses pre-existing export files to avoid stale
image false positives.

Verified: incremental builds through 040; CTest 4/4 and static checks 15/15.
[Actual application after property/Copy/Save tests](screenshots/015-editor-action-tests.png)
was reviewed. Its visible save/copy notifications still occupy too much space,
which is recorded as the next product-layout refinement, not hidden as complete.
A test-process shutdown incubation warning remains after the export notifications;
it is not a demonstrated normal-session crash.

### Separate component experiment (not the application screenshot)

Existing Mesa/llvmpipe and official Qt vkkhrdisplay support Vulkan RHI texture
rendering through QQuickRenderControl without a desktop socket. Add the explicit
EXCLUDE_FROM_ALL kos_material_probe target, loading the real pinned Kos.Ui
LiquidBackdrop/LiquidPanelSurface and QSB. Source and consuming panel are siblings,
so the panel does not sample itself or foreground text.

Actual component outputs: [baseline](screenshots/material-probe-baseline.png),
[liquid](screenshots/material-probe-liquid.png),
[reduced transparency](screenshots/material-probe-reduced-transparency.png).
Sampling states were false/true/false; shader execution and visible refraction/
softening were verified, with no QML/shader errors. The expected missing display
extension notice does not prevent offscreen texture rendering. This is a CPU
Vulkan driver, not hardware GPU, a ViewerWindow, or a Wayland capture acceptance.

## 016 — Production build and actual selection-window test

A separate production configuration (BUILD_TESTING=OFF, UI capture hook OFF)
completed all 331 compilation/link steps. ldd showed no Qt6Test or missing library,
the binary contained no test-capture environment hook, and --version exited 0.
Default startup exited 1 because this cloud environment has no DBus session.
Using the existing --new-instance --edit-existing path stayed alive for 8 seconds
until timeout124, with a kglobalaccel/DBus warning but no QML errors. Setting test
capture variables on that production executable did not create a screenshot.
This is a startup smoke test, not desktop functionality or visual acceptance.

The opt-in development hook now constructs the **actual CaptureWindow** with a
synthetic document fitted to the synthetic screen, never reads the real desktop,
and sends actual QTest selection mouse events. This exposed stale assumptions
that FloatingToolBar.background was a Rectangle with border/color properties.
Add stable semantic toolbar properties and use them for joined option borders;
also align the floating control palette with Kos colors. The warnings were fixed
and both development and production configurations incrementally rebuilt.

Verified: actual drag from (80,100) to (480,400) produced the inclusive
401×301 selection. Reviewed real [light](screenshots/016-capture-overlay-final.png)
and [dark](screenshots/016-capture-overlay-dark-final.png) CaptureWindow grabs.
No QML errors remained; CTest 4/4 and static checks 15/15 pass. The platform still
has no window-system plugin or real screenshot/recording session. These are
software-rendered selection UI tests using synthetic pixels, not desktop captures.

Product follow-up: the selection controls are still too dense and retain three
separate tool strips. Consolidate secondary actions without removing functionality.

## 017 — Focused selection controls

Consolidate the selected-image capture interface into two rounded strips. Keep
accept, cancel, copy and save visible; group Save As, OCR, export, new capture,
recording and options in a semantic-color More menu. The initial capture-mode
strip remains when no region is selected and in video mode. Annotation controls
use the same essential-tools palette as the editor, with secondary tools in More.
No capture or recording backend was replaced.

Incremental development build passed. Actual CaptureWindow/QTest drag tests passed
in light, dark and open-menu states; CTest 4/4 and static tests 15/15 passed.
Reviewed [light](screenshots/017-capture-controls.png),
[dark](screenshots/017-capture-dark.png), and
[secondary actions](screenshots/017-capture-more.png). Visual review caught a
misplaced visibility condition leaving the third strip visible; corrected and
rebuilt before these final screenshots. Labels and icons remain legible, selected
region unobscured, and the menu fits the 800×800 synthetic screen.

These are real application-window grabs with synthetic input and software
rendering, not real desktop captures or production GPU-material acceptance.
Backend-gated recording is unavailable here; menu action routes are retained but
not all native dialogs or video actions have been interactively exercised.

## 018 — Stable workbench during export notifications

Move notifications into a right-aligned, bounded, scrollable overlay instead of
reflowing the image viewport. Keep existing messages, error/warning states, file
links, open-folder/copy/dismiss actions and copied/shared timeout behavior.
Add a real-window assertion that Copy/Save notifications leave the workbench
position and size unchanged.

The isolated build environment was lost and reconstructed from approved official
packages and pinned source dependencies. KQuickImageEditor (91 steps), KPipeWire
(177 steps), and NkShot (361 steps) rebuilt successfully; dependency tests 3/3,
NkShot CTest 4/4 and static tests 15/15 passed. Actual annotated-image Copy/Save
pixel equality, properties close/undo/redo and stable geometry passed in light
and dark software-rendered application windows. Reviewed
[light](screenshots/018-notification-overlay.png) and
[dark](screenshots/018-notification-dark.png), using synthetic pixels.

Open findings, not acceptance: an extra unannotated-image Copy pixel comparison
failed and needs diagnosis; the annotated route passes. Dark notification icon
and link contrast needs refinement. Repeated-message overflow/dismiss semantics
need interaction coverage. Forced test exit can warn about two pending QML
incubations. No DBus/KWin, real capture/recording or GPU-window acceptance is claimed.

## 019 — Diagnose unannotated export comparison

The extra Copy test failure in 018 was a test-comparison issue: diagnostic PNGs
of clipboard and document output had identical RGBA pixels (zero difference in
every channel). QImage object equality was stricter than the intended pixel
contract. Compare normalized RGBA scanlines explicitly instead; do not change
the working export backend to accommodate a faulty test.

Incremental compilation passed, followed by unannotated Copy and Save pixel
checks plus stable notification/workbench geometry in light and dark actual
development windows. CTest 4/4 and static checks 15/15 passed. Reviewed
[light](screenshots/019-unannotated-export.png) and
[dark](screenshots/019-unannotated-dark.png). The known dark notification contrast
issue remains for the next UI iteration. All screenshots use synthetic input
and software rendering, not a KWin desktop capture.

## 020 — NextKde notification material and dark contrast

Use the existing unmodified Kos.Ui KosSurface for rounded notification panels.
Preserve severity icons, message content and action routes; tint actual action
buttons explicitly and style existing rich-text anchors for readable dark links.
Remove the copy action's IconOnly display hint because it bypassed the custom
colored button; the custom button itself remains icon-only.

Incremental builds and real-window tests caught an incorrect KosSurface property
name before acceptance; corrected to fillColor/strokeColor and rebuilt. Reviewed
final [light](screenshots/020-notification-light.png) and
[dark](screenshots/020-notification-dark.png): file links, folder, copy and dismiss
icons are now readable and the panels match the editor's rounded surface family.
Copy/Save pixel equality and unchanged workbench geometry pass in both states;
CTest 4/4 and static tests 15/15 pass. These remain actual development-window grabs
with synthetic pixels/software rendering, not GPU/KWin acceptance. Repeated
notification overflow and dismissal need additional interaction coverage.

## 021 — Notification overflow and real dismissal regression

Add a dev-only actual-window scenario with twelve distinct warnings. Verify the
list is height-bounded and scrollable, scroll to its end, click the last warning's
real close button with QTest, then insert repeated Copied messages and check
replacement instead of accumulation. Assert image workbench geometry never
changes. Name actual notification controls for reliable testing. The harness
traverses the visual item tree because ListView-created controls are not all
QObject descendants of the window's root object.

Incremental build passed. All overflow/bounds, actual close click, deduplication
and geometry assertions passed; CTest 4/4 and static tests 15/15 passed. Reviewed
[actual window](screenshots/021-notification-overflow.png): warnings retain their
severity icons and remain available in a bounded stack rather than displacing
the image. Synthetic input/software rendering only. Scrolling here is driven by
the list's content position; physical wheel/touch gestures and timeout behavior
for virtualized messages are not yet covered.

## 022 — Notification expiry independent of virtualized delegates

Move the existing ten-second Copied/Shared lifetime from QML ListView delegates
into InlineMessageModel. A scrolled-out delegate must not suspend or reset the
message lifetime. Assign each inserted item a monotonic identity so a pending
old timer cannot delete a newer replacement or another row after removal.
Persistent saved/error/warning messages remain persistent.

Incremental compilation passed. Extend the actual-window overflow scenario to
scroll the transient row out of view and wait for expiry: Copied disappears while
all eleven remaining warnings persist, and the workbench stays unchanged. Actual
close click and duplicate replacement checks still pass. CTest 4/4 and static
checks 15/15 pass. Reviewed the [post-expiry window](screenshots/022-notification-timeout.png).
Synthetic input/software rendering only; no real desktop or recording claim.

## 023 — Production build without Qt Test or developer capture

Honor BUILD_TESTING when adding test targets and finding Qt Test. The opt-in
developer capture hook still requests Qt Test explicitly when enabled; ordinary
production builds no longer require it solely because test sources exist.

Development reconfiguration/build and CTest 4/4 passed; static tests 15/15 passed.
A fresh production tree configured with BUILD_TESTING=OFF,
NKSHOT_ENABLE_UI_CAPTURE=OFF and CMAKE_DISABLE_FIND_PACKAGE_Qt6Test=TRUE, then
completed all 333 build steps. CTest reports zero production tests; the final
executable links neither Qt6Test nor missing libraries and contains no
NKSHOT_TEST_UI_CAPTURE string. Its version command reports nkshot 6.8.3.

Production --new-instance --edit-existing stayed alive for the full eight-second
smoke interval (timeout exit124), emitted only the expected unavailable DBus
warning, and ignored test capture environment variables without creating an
image. Binary SHA256: 43388374a0ee84d54be855cab223197d63937e938802e3ce553ffcdccb18516b.
No UI was changed in this build-only iteration; no new visual screenshot is
claimed. This remains a cloud startup smoke check, not real KWin/recording or
production GPU visual acceptance.

## 024 — Explicit document boundary for editor controls

Extend the existing injected annotation canvas boundary to the tool palette,
property controls and undo/redo group. Each now requires an AnnotationDocument
parameter instead of reaching into SpectacleCore. The screenshot and capture
page owners provide the document explicitly; the palette forwards it to its
undo/redo controls. This prepares reuse without changing capture or export
ownership. It is not yet a standalone photo-editor library: application theme,
localization, window helpers and surrounding command integration remain.

Incremental compilation passed; actual property commit/close/undo/redo, Copy/Save
pixel checks, notification geometry, and CaptureWindow region drag/menu tests
passed. CTest 4/4 passed and static checks now 16/16 include explicit control
document dependency checks. Reviewed [editor](screenshots/024-editor-document.png)
and [capture](screenshots/024-capture-document.png) for unintended layout changes.
Both are real development windows using synthetic input/software rendering.
Standalone alternate-document control hosting has not yet been runtime-tested.

## 025 — Actual second-document control isolation

The dev harness injects a separate 100×80 document into the actual palette,
undo/redo group and property controls. Real mouse clicks select freehand and
undo/redo only the alternate document. A real property spin-box edit and popup
close commits a second history entry there. The original screenshot's tool,
history depth and pixels remain unchanged. Restore original ownership before
destroying the alternate document.

Incremental builds, the actual-window isolation scenario, CTest 4/4 and static
checks 16/16 pass. The expanded harness initially retained a spin-box pointer
across a Loader recreation and crashed; reacquire the actual control after
settling the document/tool transition. Final process exits 0. Forced teardown
may report pending QML incubations, as in prior dev captures. Reviewed
[restored window](screenshots/025-document-isolation.png), synthetic pixels and
software rendering only. This verifies control isolation in the app host, not
a separately packaged editor SDK.

## 026 — Blur/pixelate gestures across zoom and DPR

Expand the actual viewport mouse-drag coordinate regression from rectangle alone
to rectangle, blur and pixelate, at three zoom levels and four image DPRs. Use
patterned source pixels so redaction effects are observable. Compare against an
independent document-coordinate gesture and verify exact undo/redo output.
The editor test now passes 46 cases in each of eight process configurations
(window scale 1/1.25/1.5/2 × basic/threaded software render loop).

Also drive actual ViewerWindow blur and pixelate mouse gestures, verify changed
pixels and undo/redo, and review [blur](screenshots/026-blur-gesture.png) and
[pixelate](screenshots/026-pixelate-gesture.png). A flat, grid-aligned initial
fixture made pixelation correctly leave pixels unchanged; add the explicit
--texture fixture mode rather than weaken the changed-pixel assertion.
Incremental application/test builds, CTest 4/4 and static checks 16/16 pass.
These tests verify operation/coordinates, not that blur/pixelation makes private
information unrecoverable. Synthetic input/software rendering only.

## 027 — Recording dependency verification, with explicit skips

No UI or recording backend implementation changed. Rebuild checks report up to
date. Execute the pinned KPipeWire video/audio tests: CTest 2/2 passes; detailed
video results are 11 passed and 6 skipped (three VAAPI profiles unavailable, three
OpenH264 profiles missing), audio results 5 passed and no skips. Software video
initialization/resize state and the actual synthetic Opus sample pipeline pass.

Document reproducible commands and missing full-recording acceptance in
[recording verification](recording-verification.md). This is meaningful backend
evidence, not a live KWin recording or a claim that all codecs work. No new UI
image is produced for this verification-only iteration.

## 028 — Real local Chinese OCR and one-shot language restoration

Add a generated Chinese+digits fixture rendered using Noto Sans CJK. Run it
through the actual asynchronous Tesseract chi_sim engine and assert the recognized
text equals 中文截图编辑12345 after whitespace normalization, clipboard output
matches, and a one-shot Chinese request restores the configured English language
without changing saved settings. This is offline OCR, not translation.

Test build passed. OCR QtTest reports 4 passed, 0 failed, 0 skipped including
existing English coverage; aggregate CTest 4/4 and static checks 16/16 pass.
The existing system Noto Sans CJK font was copied into the isolated dependency
root for the fixture, without modifying the host or distributing the font.
On machines without chi_sim data or CJK font this optional Chinese case reports
a named skip. One clean synthetic phrase does not establish general OCR accuracy.
No application UI changed, so no new UI screenshot is claimed.

## 029 — Actual text annotation keyboard and IME event path

Create a text annotation by clicking the actual application canvas, type Latin
characters via QTest key events, then send a Chinese preedit/commit event to the
focused text control. Assert the selected annotation reads exactly NkShot 中文,
that rendered pixels change, and all text history can be undone/redone without
altering the original image. Set explicit test font after changing tool mode,
so restored per-tool preferences cannot override the fixture's intended size.

Incremental builds and the actual window scenario exit0. CTest 4/4 and static
checks 16/16 pass. Reviewed [text annotation](screenshots/029-text-ime.png), an
unmodified software-rendered application-window grab with synthetic image input.
This checks Qt's input-method commit path, not integration with a real installed
Fcitx/IBus candidate panel or every writing system.

## 030 — Actual crop cancellation, double-tap acceptance and keyboard history

Exercise the actual crop overlay: drag, right-click clear with unchanged image
and empty selection, drag again, double-tap accept, compare cropped pixels with
the corresponding source rectangle, and invoke real Ctrl+Z/Ctrl+Shift+Z.
The harness must send intermediate pointer movement for DragHandler activation
and explicit click timestamps: QTest's default click delay deliberately separates
double-clicks. Compare the crop's rounded pixel rectangle, not a larger outward
aligned rectangle, when logical dimensions are fractional.

Incremental compilation and final actual-window scenario exit0. The verified
crop is 518×324 at source origin (173,108); keyboard undo/redo restores exact
pixels. CTest 4/4 and static tests 16/16 pass. Reviewed
[cropped window](screenshots/030-crop-gesture.png), with synthetic input/software
rendering. Update the preservation map to distinguish now-tested editing, OCR
and dependency paths from remaining real desktop/recording gaps.

## 031 — Offline pinned-image MVP

Add More → Pin Image for still images. Freeze the current annotated pixels into
a separate resizable, aspect-preserving window with shared Kos.Ui surfaces,
Copy, Close and a Stay on Top toggle. Each pin has an independent image provider
and immutable snapshot; no document mutation, disk write or network transfer is
needed. Multiple pins are allowed. Native editor-window closure leaves pins
alive; quitting the application closes them. Pins are not persistent saved files.

Application build passed. Actual menu click, two different pin snapshots,
original-document edit/undo isolation, real Copy/Stay on Top/Close clicks,
resizing and survival after editor close all pass in light and dark. CTest 4/4
and static checks 16/16 pass. Reviewed actual pin windows:
[light](screenshots/031-pin-light.png), [dark](screenshots/031-pin-dark.png).

The toggle's Qt window flag is tested, not real cross-application stacking.
Wayland/compositor policy can affect topmost behavior; KWin desktop validation
is still unavailable. These are actual software-rendered application windows
with synthetic image input, not a fake desktop demonstration. The opt-in dev
harness can report pending QML incubations at forced shutdown.

## 032 — Compact pins and copy feedback

The pin's Copy action now shows a checkmark/Copied confirmation for 1.8 seconds,
then returns to Copy. At widths below 340 pixels it uses an accessible icon-only
control with a tooltip, preserving all controls at the 260×180 minimum.

Incremental build passed. Extend actual pin tests to assert immediate feedback
and timer reset; rerun full snapshot/copy/topmost-flag/close/editor-close checks
in minimum size, normal dark and timeout scenarios. All exit0, CTest 4/4 and
static checks 16/16 pass. Reviewed [minimum size](screenshots/032-pin-compact.png)
and [dark feedback](screenshots/032-pin-dark.png). Real pin windows, synthetic
image input/software rendering only; compositor stacking remains unverified.

## 033 — Conservative offline vertical composition core

Add a Qt Gui-only local stitcher for future scrolling capture/photo reuse. Find
unique exact vertical overlaps, verify pixels after hash matching, preserve
alpha/DPR/color space, and reject ambiguity, duplicates, incompatible images or
oversized output. It does not yet add a menu or control another application.

Incremental build initially caught a missing QColorSpace include; fixed and
rebuilt. New QtTest has 10 passed, 0 skipped. Three frames reconstruct exact long
image pixels; rejection/ownership/alpha/DPR cases pass. Aggregate CTest now 5/5;
static tests 16/16 pass. See [contract](local-image-tools.md). No UI changed, so
no new UI capture is claimed; automatic scrolling remains unimplemented.

## 034 — Save an independent pinned image

Add Save As and its keyboard shortcut to each pin. Save the immutable pin pixels
as PNG through an asynchronous native file dialog and QSaveFile atomic commit;
report success/failure without changing the editor document or export settings.
Repeated Save As reuses the existing dialog. At the 260 px minimum, hide the
secondary dimensions label to retain all four actions.

Rebuilt after restoring the isolated toolchain. Actual widget-dialog tests cover
cancel/reopen, duplicate action, filename input, PNG pixel equality and document
isolation, in normal/light, minimum-size and dark windows. CTest 5/5 and identity
checks 16/16 pass. Test files are temporary and cleaned up. Native desktop file
portals, overwrite prompts and compositor stacking remain unverified. Captures
use synthetic input and software rendering, not real desktop screenshots.

## 035 — Clearer editor hierarchy and shared controls

Give Save a primary accent surface, keep Copy secondary, label capture as New
Screenshot, and add a compact screenshot/recording heading with canvas dimensions
on wide windows. Narrow windows retain icon actions and omit secondary metadata.
Reduce the bottom palette from 60 to 50 px and use consistent 18 px icons.
Replace legacy property checkboxes with the existing Kos.Ui control and give
numeric fields a shared, restrained rounded field surface. Editing actions,
shortcuts, undo history and secondary tools remain present.

Incremental build, CTest 5/5, static checks 16/16 pass. Actual property transaction
(change, close, undo/redo) passes. Reviewed actual [light](screenshots/035-editor-light.png),
[dark](screenshots/035-editor-dark.png) and [640 px](screenshots/035-editor-narrow.png)
windows. These are software-rendered synthetic-input captures. This stage improves
hierarchy and controls, not GPU liquid-material verification or a finished Apple
reproduction; the editor palette is still an opaque shared surface.
