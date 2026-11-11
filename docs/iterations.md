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

## 036 — Separate editor sampling plane and real material route

Group only the ordinary workbench background and image loader in a full-sized
sampling plane. The annotation dock, properties, notifications and footer remain
outside it. The dock now uses the existing Kos.Ui LiquidPanelSurface on a capable
renderer, with an explicit KosSurface fallback for software rendering, disabled
liquid effects or reduced transparency. No consumer samples itself; capture and
export still use the unchanged document model.

Incremental builds and CTest 5/5/static 16/16 pass. Actual property transaction,
Copy/Save pixel equality and unchanged notification geometry pass. Runtime probe
confirms the source excludes capture consumers and software active/sampling are
both false. Reviewed [export/light](screenshots/036-editor-export.png),
[dark](screenshots/036-editor-dark.png) and [narrow](screenshots/036-editor-narrow.png)
software fallback windows. Full-app Vulkan was attempted explicitly with RHI;
the offscreen platform cannot createPlatformVulkanInstance. Therefore real GPU
sampling/mapping/optics are NOT verified, and these PNGs are not glass proofs.

## 037 — Compact notifications without losing actions

Replace the large InlineMessage layout with a restrained shared surface: small
status icon, wrapping message/link text, and compact folder/copy/dismiss controls.
Actions retain accessible labels and tooltips. Preserve original URL/data types,
persistent warnings, transient expiration and deduplication. Notification
animations now honor reduced motion. No message is silently discarded to improve
appearance, and notifications still do not resize the image workbench.

Build, CTest 5/5 and static tests 16/16 pass. Real clicks verify file-URL clipboard
contents and dismissing an offscreen warning after scrolling; repeated messages,
12-row overflow, bounded height, stable canvas geometry and transient timeout
preserving all eleven warnings pass. Copy/Save export pixels pass in light/dark/
640 px states. Reviewed [light](screenshots/037-notifications-light.png),
[dark](screenshots/037-notifications-dark.png), [narrow](screenshots/037-notifications-narrow.png)
and [overflow](screenshots/037-notifications-overflow.png). Folder action is
visible and wired, but native file-manager launch is not exercised without a
real desktop. All images remain software-rendered synthetic-input captures.

## 038 — Structured properties and a visible close control

Give the properties popover an Appearance heading and explicit accessible close
button. Align common checkbox labels and retain the shared numeric fields,
colors, shadow options, keyboard behavior and independent document binding.
The scroll area remains bounded for smaller windows and larger property sets.

Build and CTest 5/5 pass; static identity tests 16/16 pass. Actual light, dark and
640 px tests change a selected annotation property, click the new close control,
verify the pending edit becomes one undo transaction, undo/redo exact pixels and
reopen the popover. Pin Save As regression also passes after tightening its file
wait assertion. Reviewed [light](screenshots/038-properties-light.png),
[dark](screenshots/038-properties-dark.png) and [narrow](screenshots/038-properties-narrow.png)
actual software windows. Native font/color dialogs, desktop capture and GPU
optics remain outside this verification.

## 039 — Seven primary tools and reachable secondary tools

Keep crop, selection, freehand, arrow, rectangle, privacy pixelation and text on
the editor's primary palette. Move the less-frequent pan mode into More tools,
separated from highlighter, line, ellipse, blur and number. The menu reflects the
selected secondary tool and now uses a simple checkmark rather than a grid of
unchecked checkbox outlines. All original annotation modes remain available;
the fullscreen capture palette does not offer the editor-only pan mode.

Build, CTest 5/5 and static checks 16/16 pass. Real menu clicks select each of six
secondary modes without changing document pixels or undo history, in light/dark/
640 px states. Reviewed [light](screenshots/039-tools-light.png),
[dark](screenshots/039-tools-dark.png), [narrow](screenshots/039-tools-narrow.png)
and the [capture overlay](screenshots/039-capture-overlay.png). These use synthetic
input/software rendering; real desktop capture and GPU optics remain unverified.

## 040 — Capture completion hierarchy and reliable fallback corners

Place the primary Done action at the end of the capture completion bar, retain
Copy/Save and More, and present cancellation as a labeled/tooltip icon. Dim the
secondary size readout. Actions retain their original implementation, including
AcceptAction behind Done. Use the same small icons and primary accent treatment
as the standalone editor.

Actual screenshots caught a Grid-anchor layout regression during implementation;
remove that invalid anchor and add assertions that Done/Cancel and their toolbar
have usable geometry inside the window. Software-rendered layers also exposed
black rectangular corners: use the explicit solid FloatingBackground fallback
and disable the software-only opacity layer while retaining the GPU material
route. Rebuilt and reran all affected scenes after both corrections.

Final build, CTest 5/5 and static checks 16/16 pass. Real selection mouse drags,
finalizer geometry and More popup render pass in light/dark/menu states. Reviewed
[light](screenshots/040-capture-light.png), [dark](screenshots/040-capture-dark.png)
and [More](screenshots/040-capture-more.png). These are real selection windows
fed synthetic input, not desktop-capture or recording validation. Actual accept/
cancel capture lifecycle and full-window GPU effects remain unverified.

## 041 — User-facing offline screenshot composition

More → Combine screenshots opens a multi-file picker. Choose 2–16 equal-width
images with a unique unchanged vertical overlap (at least 24 pixels), numbered
from top to bottom. Natural numeric filename order is deterministic, independent
of click order. Decode and stitch off the GUI thread; reject over 40 megapixels
of total input before decoding. The result opens as a separate immutable pin for
Copy/Save As. Source files, the current annotation document and its history are
not replaced. One picker at a time; busy state prevents concurrent combinations.

A missing executable source entry was caught at link time and corrected. Final
build, CTest 5/5 and static checks 16/16 pass. Actual menu/picker tests cover
repeated open, cancellation/reopen, reverse-selected frame10/frame2 numeric sort,
exact reconstructed pixels, unchanged editor pixels and duplicate-frame rejection
without producing a preview. Both light and dark pass. Reviewed the
[entry](screenshots/041-combine-entry.png), [light result](screenshots/041-combined-light.png)
and [dark result](screenshots/041-combined-dark.png). Fixtures are clearly synthetic
local documents, not captures of another application. Native desktop picker,
large real-world images, mixed color profiles and automatic scrolling are not
validated by these UI tests; conservative core rejection cases remain covered.
This feature is manual local composition, NOT automatic scrolling capture.

## 042 — Actionable composition errors

Replace the generic combination failure with specific guidance for unreadable/
short images, mismatched width/scale/profile, missing overlap, ambiguous repeated
content, duplicate frames and size limits. Validate width/height/input pixel limits
before decoding and distinguish invalid image headers from oversized inputs.
If the preview cannot initialize, report the failure rather than silently doing
nothing. Each failure confirms that source files and current edits are unchanged.

Build, CTest 5/5 and static checks 16/16 pass. The actual picker/UI test now checks
specific duplicate, wrong-width, damaged PNG, too-few-input and missing-overlap
messages; each rejects without a preview or document mutation. Existing successful
composition and cancellation tests rerun in both themes. Reviewed
[light](screenshots/042-combine-errors-light.png) and
[dark](screenshots/042-combine-errors-dark.png) real software-rendered notifications.
The core's ambiguity/size cases are covered by its unit tests; this does not claim
large real-file or native desktop portal testing.

## 043 — Review local OCR results in the application

Successful OCR now opens a separate selectable plain-text result window with
Copy all feedback and Close. The shared Kos palette and surfaces support light,
dark and the minimum 360×260 size. The text is read-only and treated as plain
text, not rich text or links. Existing automatic clipboard and notification
behavior remains intact. Recognition stays on the device; no translation service
or image upload is introduced. Failed/empty recognition does not open a result.

Build, CTest 5/5 and static checks 16/16 pass. Actual Tesseract English and chi_sim
recognition of synthetic image fixtures reaches the new window. Tests verify
recognized text, unchanged source pixels, mouse focus plus Ctrl+A/C selection,
Copy all clipboard/feedback and actual Close destruction. Light, dark, compact
and Chinese cases pass. Reviewed [light](screenshots/043-ocr-light.png),
[dark](screenshots/043-ocr-dark.png), [compact](screenshots/043-ocr-compact.png)
and [Chinese](screenshots/043-ocr-chinese.png) software-rendered windows.
The harness invokes the real core OCR entry point, not the menu action; this is
not a general OCR-accuracy benchmark, desktop-notification test or GPU test.

## 044 — Save recognized text locally

The OCR result now offers Save text as and Ctrl+S/Ctrl+Shift+S. A non-blocking
file picker writes a UTF-8 .txt using QSaveFile atomic replacement, without
changing the image, clipboard or recognition result. Repeated requests reuse one
picker; cancelling creates nothing. Invalid extensions and write/commit errors
receive inline feedback. Closing the result owns and cleans up its picker.

Final build, CTest 5/5 and static checks 16/16 pass. Real English/Chinese OCR UI
tests click Save, cancel/reopen, repeat the action, reject a .png extension,
then compare the saved bytes exactly with recognizedText.toUtf8(). Original
image pixels remain unchanged. Light/dark/compact/Chinese variants pass, including
prior selection/copy/close tests. Reviewed [light](screenshots/044-ocr-save-light.png),
[dark](screenshots/044-ocr-save-dark.png), [compact](screenshots/044-ocr-save-compact.png)
and [Chinese](screenshots/044-ocr-save-chinese.png).
Tests use widget file pickers and synthetic images; native portals, existing-file
overwrite prompts and filesystem failure injection remain unverified.

## 045 — Choose an installed OCR language from the editor

Keep the existing one-click Extract Text action and add a low-frequency More →
Extract text in submenu listing the engine's installed recognition models by
human-readable name. Orientation-only osd is excluded. Selecting a model invokes
the existing one-shot language override; no download, external service or new
persistent language setting is introduced. Both entries disable during recognition;
the default action shows Extracting text while busy.

Build, CTest 5/5 and static checks 16/16 pass. The UI harness opens More/submenu
and physically clicks the English or simplified-Chinese entry, then verifies
actual Tesseract recognition, selection/copy, close and unchanged image pixels.
The Chinese compact scenario also reruns cancel/reopen/reuse, invalid extension
and exact UTF-8 save. Reviewed [light menu](screenshots/045-ocr-menu-light.png),
[dark menu](screenshots/045-ocr-menu-dark.png) and
[compact Chinese saved result](screenshots/045-ocr-language-compact.png).
Only installed eng/chi_sim models and software-rendered menus were exercised;
large language lists, mixed-language accuracy and native desktop operation are
not established. This editor submenu does not replace the capture-overlay action.

## 046 — Fast editor tool switching and core editing regression

The standalone editor's seven primary tools gain window-scoped Alt+1…Alt+7
shortcuts, in dock order: Crop, Select, Freehand, Arrow, Rectangle, Pixelate, Text.
Their hover-help text includes the assigned key. Shortcuts require the visible,
enabled essentials palette; the full-screen capture palette is unchanged. Normal
unmodified typing is not intercepted. Disabled Select remains disabled when there
are no annotations. Tool changes use the same existing button handlers.

Final build, CTest 5/5 and static checks 16/16 pass. Real key events switch all
seven tools without changing rendered pixels or undo depth, in both themes.
Separate actual editing regressions pass: English keyboard plus Chinese IME text,
text undo/redo, drag crop cancel/accept and keyboard undo/redo, and pixelate/blur
drag gestures with pixel-exact undo/redo. Redaction needs the patterned fixture;
a flat-color input is not a useful visible-change assertion. Reviewed
[light](screenshots/046-tool-keys-light.png), [dark](screenshots/046-tool-keys-dark.png),
[text](screenshots/046-tool-keys-text.png) and
[pixelation](screenshots/046-tool-keys-pixelate.png).
These are actual software-rendered editor interactions on synthetic images, not
KDE desktop capture/recording verification, compositor shortcut-conflict testing
or a guarantee that blurred/pixelated private information cannot be recovered.

## 047 — Explicit vertical stacking without overlap

Add More → Stack images vertically for local images that have no shared overlap.
It retains every pixel rather than inferring cuts or silently falling back after
an overlap failure. The reusable imaging function allocates one output, preserves
alpha/DPR/color profile and accepts repeated content intentionally. The UI shares
numeric filename sorting, asynchronous decoding, one-picker/busy guards, size
limits and separate preview/Copy/Save As with overlap composition. Original files
and the current editor remain untouched. No automatic scrolling is introduced.

Build, CTest 5/5 and static checks 16/16 pass. Added unit cases verify exact frame
boundaries, alpha, repeated-frame retention, independent output, DPR/profile,
invalid input and pixel-limit boundaries. Actual UI tests click the new action,
cancel/reopen, repeat requests, select frame10 before frame2 and verify exact
non-overlapping reconstruction and editor isolation in both themes. Existing
successful overlap and all specific UI rejection regressions also pass.
Reviewed [entry](screenshots/047-stack-entry.png),
[light](screenshots/047-stack-light.png) and [dark](screenshots/047-stack-dark.png).
Inputs are synthetic local images; native portals, maximum-memory stress and
real desktop scrolling/capture remain unverified. Mixed width/profile/scale
images are rejected instead of being silently resized.

## 048 — Actual recording playback, software frames and failure recovery UI

Playback review exposed a real gap: QtMultimedia decoded the H.264 sample and
advanced position, but VideoOutput displayed no pixels with Qt's software scene
graph. Add a dedicated QVideoSink-backed SoftwareVideoPreview that paints actual
decoded frames only for the software renderer. The existing GPU VideoOutput path
and MediaPlayer decoding/audio ownership remain unchanged. The view accepts an
independent sourceUrl for reuse while defaulting to the recorded file.

Paused recordings now keep controls visible without hunting for a hover target.
The seek control uses shared Kos colors and a round handle. Playback failures
show a readable card, original-file assurance, diagnostic and styled Retry button;
retry restores source bindings. Reduced-motion settings apply to control fades,
and software rendering avoids the opacity layer that previously produced bad
corners. No synthetic success status or screenshot composition is used.

Final build, CTest 5/5 and static checks 16/16 pass. A locally generated silent
4-second H.264 MP4 is decoded by the actual QtMultimedia FFmpeg backend. UI tests
click Play/Pause and seek to about 2.4 seconds, require a real software video
frame, and verify the file bytes are unchanged. Corrupt-MP4 tests click Retry and
verify a stable failure state without file mutation. Light/dark and 640×420
playback pass. Reviewed actual [light](screenshots/048-playback-light.png),
[dark](screenshots/048-playback-dark.png), [compact](screenshots/048-playback-compact.png),
[error](screenshots/048-playback-error.png) and
[dark error](screenshots/048-playback-error-dark.png). The colorful image is a
codec test-pattern video, not an application design mockup.

This validates local playback, NOT desktop recording. Real KWin/PipeWire capture,
GPU video presentation, audio, HDR/rotation metadata, high-resolution performance
and animated GIF/WebP regression remain unverified. The software fallback is a
correctness path, not a claim of GPU performance or Liquid Glass validation.

## 049 — GIF/WebP playback isolation and regression coverage

Choose the animation decoder from the file format, not from a Ready status that
only becomes true after loading. This prevents MediaPlayer from also trying to
open GIF/WebP during animation loading/retry. Keep actual AnimatedImage playback,
hide the video seek control for animations and center the compact Play/Pause bar.

The first real WebP test correctly failed: the isolated environment had libwebp
but no Qt WebP image plugin. Added official qt6-image-formats-plugins and runtime
dependencies to the isolated toolchain, recorded hashes and documented packaging
requirements. No host upgrade or permission changes occurred. After that, actual
GIF/WebP frame progression, mouse pause/resume, stable paused frames, hidden seek,
empty duplicate MediaPlayer source and unchanged file bytes pass in light/dark.
Corrupt GIF/WebP Retry flows also pass; the real H.264 play/pause/seek/software-frame
regression remains green. Build, CTest 5/5 and static checks 16/16 pass.

Reviewed [GIF](screenshots/049-animation-gif.png),
[dark WebP](screenshots/049-animation-webp-dark.png) and
[invalid WebP](screenshots/049-animation-webp-error.png). Added a reproducible
non-personal animation fixture generator. Updated stale README claims that OCR
and all playback were untested, while retaining the true desktop/GPU/audio limits.
These are local synthetic-media tests, not real recording acceptance, broad codec
coverage, mixed-format source-switch testing or a performance benchmark.

## 050 — Same-window format transitions and successful Retry recovery

This is a validation stage, not a new product feature. No production change was
needed after testing the real playback view continuously through MP4 → GIF → WebP
→ broken MP4 → repaired MP4 via the actual Retry button → another valid MP4.
The tests require decoder isolation, cleared stale software frames and errors,
animation frame progression and real decoded video frames after recovery.
Only a test-owned temporary broken file is deliberately replaced with valid MP4
bytes; all original fixtures remain unchanged. Source transitions are driven by
the reusable view's sourceUrl, not by a real desktop recording workflow.

Build, CTest 5/5 and static checks 16/16 pass in the restored environment. Both
light and dark same-window scenarios pass. Reviewed recovered playback in
[light](screenshots/050-media-recovery-light.png) and
[dark](screenshots/050-media-recovery-dark.png). These are actual test-pattern
frames after recovery, not new interface designs. Desktop capture, audio, GPU,
format rotation/HDR and performance acceptance remain open.

## 051 — Resize an annotated sharing copy without destructive edits

Add More → Resize a copy: enter physical pixel width, see proportional height,
and create a separate preview for Copy/Save As. A small reusable ImageTransforms
helper performs bounded smooth scaling; the actual annotated snapshot is captured
on the GUI thread and scaled on a worker. The original document, source files
and undo history are untouched. Shared Kos styling covers the modal, fields and
primary/secondary actions. Errors report failure rather than producing a bogus
preview; the busy flag prevents overlapping resize jobs.

Build, CTest 6/6 and static checks 16/16 pass. New core tests verify dimensions,
alpha, fractional DPR, color profile, output independence and invalid/size limits.
Actual UI tests click More/Resize, cancel via Escape and reopen, type 320 in the
width field, create the preview, copy its exact pixels and save a PNG through a
real widget dialog. Output matches smooth scaling of the annotated snapshot;
original rendered pixels and undo depth remain unchanged. Light/dark and 640×420
scenarios pass. Reviewed [light dialog](screenshots/051-resize-dialog-light.png),
[dark dialog](screenshots/051-resize-dialog-dark.png),
[compact dialog](screenshots/051-resize-dialog-compact.png) and
[saved output](screenshots/051-resize-light.png).

This produces an immutable flattened sharing copy, not a new editable annotation
document. Native portals, overwrite confirmation, large-image memory pressure
and GPU presentation remain unverified. Enlargement cannot recover lost detail.

## 052 — Opaque cover preset for safer screenshot sharing

Add More tools → Solid cover preset using the existing rectangle implementation:
opaque black fill, zero stroke, no shadow. Deselect before configuration so a
selected existing annotation is not accidentally recolored. The inline guidance
asks for a margin and an exported sharing copy, explicitly noting that the
editable original retains undo history. Existing pixelate/blur tools remain.

Build, CTest 6/6 and static checks 16/16 pass. The actual menu test starts with a
selected red rectangle, verifies the preset changes neither its pixels nor undo
depth, then drags a cover. All tested interior pixels are fully opaque black;
pixels outside the boundary margin stay unchanged. Undo/redo exactly restores
before/after images. Real Copy/Save action regressions verify clipboard and saved
PNG pixels equal the rendered covered image, in both themes. Existing secondary
tool clicks still pass. Reviewed [menu](screenshots/052-solid-cover-menu.png),
[guidance](screenshots/052-solid-cover-guidance.png),
[light export](screenshots/052-solid-cover-light.png) and
[dark export](screenshots/052-solid-cover-dark.png).

This is a rectangle style preset, not permanent erasure of source files, undo
history, metadata or other copies. The test checks opaque interior pixels rather
than certifying arbitrary sensitive data at antialiased edges. Users must leave
a margin and inspect the flattened exported file. Native desktop/GPU acceptance
remains separate.

## 053 — Sharing copies and task-based discovery

Added a reusable, local background/text-watermark compositor and a sharing-copy
workflow, rather than modifying the editable screenshot. Three restrained presets,
physical-pixel padding, optional text attribution and rounded corners produce a
separate preview with Copy/Save As. The source and annotation history are retained.
Tagged inputs convert to sRGB; alpha is flattened onto an opaque background. A
bounded worker shares the resize-copy lifetime/error path. Size limits are shown
before creation, with an actionable disabled state and recovery by reducing padding.

The product review also removed the growing flat More list. It now groups
**Share a copy**, **Extract text** and **Combine images**, leaving frequent Copy
and Save visible. Recording mode hides irrelevant image tasks. The OCR task keeps
both configured languages and installed language choices; composition distinguishes
matching overlapping screenshots from stacking without overlap. No functionality
was intentionally removed. Reviewed light/dark menus, task submenu, configuration,
460×400 narrow layout, output previews and oversized-input recovery. Corrected a
selected-preset toggle issue found during visual review, and an ampersand mnemonic
that incorrectly displayed in the sharing menu.

Build, CTest 6/6 and static checks 16/16 pass. Actual offscreen interactions cover
all three presets, dark/narrow variants, cancellation/reopen, typed watermark,
preview/clipboard/PNG equality and untouched original/history. An actual 8192×200
image initially exceeds output limits; typing zero padding restores Create and
exports correctly. Resize, English/Chinese OCR task selection, matching/stacking,
composition errors and pin Save As regressions pass through the regrouped menus.
Core checks include dimensions, corner clipping, transparent input, DPR, P3→sRGB,
watermark footer, invalid options and memory bounds.

Reviewed [task menu](screenshots/053-menu.png),
[sharing options](screenshots/053-aurora.png.task.png),
[configuration](screenshots/053-aurora.png.configured.png),
[dark configuration](screenshots/053-dark.png.configured.png),
[narrow dialog](screenshots/053-compact.png.configured.png),
[oversize recovery state](screenshots/053-limit.png.limit.png), and
[Aurora](screenshots/053-aurora.png), [Light](screenshots/053-light.png),
[Dark](screenshots/053-dark.png) output previews. The application-like input is
explicitly labeled synthetic, generated by the checked-in fixture script.

This is basic background styling and text attribution, not custom backgrounds,
logo watermarks, AI editing or full 1Capture parity. Native desktop/portal/GPU
acceptance remains pending. The unified [capability matrix](capability-matrix.md)
now separates source, automated evidence and real KDE acceptance. Updated the
roadmap/preservation document accordingly. Fixed-region rolling capture was
investigated but not faked: shared capture callbacks replace the active document,
so a safe implementation needs an isolated session collector and a live KWin
capture/input/DPI acceptance environment (see [plan](scroll-capture-plan.md)).

## 054 — Isolated scroll-frame collector foundation (not a capture feature)

Added a backend-independent collector for a future manual-scroll workflow. It
fixes the physical region/output/DPR, permits one pending capture, and uses
monotonic tickets so late callbacks after cancellation/restart cannot consume a
new request. Candidate failures preserve accepted pixels and allow retry. Buffer
ownership is detached from the backend. Only the cumulative image is retained,
with frame/output bounds; finish requires two frames and releases session storage.
No editor, export, backend signal or UI route is connected to this class yet.
It is currently compiled through its test target, not a live capture entry point.

Build and CTest 7/7 pass; static checks 16/16 pass. New tests cover exact pixels,
negative desktop coordinates, DPR/profile consistency, stale/failure/repeated
requests, cancel/restart, no overlap/ambiguous/duplicate frames, size/frame caps,
immutable source buffers and finished-result lifetime. These are synthetic frame
contract tests, not desktop capture or user-scrolling acceptance.

Re-ran actual sharing menu/cancel/options/preview/clipboard/PNG interactions and
reviewed the [existing UI regression screenshot](screenshots/054-existing-sharing-regression.png).
This screenshot shows the existing sharing workflow, **not rolling capture**.
There is deliberately no new button, placeholder or claimed scrolling demo.
The adapter contract and remaining live KWin/repaint/permission/DPI milestones are
recorded in [the plan](scroll-capture-plan.md) and capability matrix.

## 055 — Dedicated KWin frame request boundary (still no scrolling UI)

Compiled the isolated collector and a dedicated KWin frame-request adapter into
the app. Each request owns its ScreenShotSourceScreen2 rather than reusing normal
editor capture signals. It validates output identity, geometry, DPR and exact
native dimensions, then crops output-local physical pixels without scaling or
padding. Only one request is active; cancel/timeout invalidates its generation.
No controller or UI instantiates the adapter yet.

Inspection found two safety issues in the reused source path: the read-completion
lambda captured a bare this without a QObject context, and incomplete raw pixel
reads were not checked. Completion is now context-bound; incomplete data returns
an error rather than accepting a partly initialized image. Actual KWin transport
and cancellation acceptance still require the desktop, not a synthetic claim.

Build, CTest 7/7, static 16/16 and diff whitespace checks pass. Extended tests
verify strict crop bounds/size/scale, metadata and ownership, plus Qt continuation
suppression after QObject deletion and dispatch to the owning thread. The actual
offscreen app verifies missing-DBus/null-screen refusal and repeated cancellation
without changing editor pixels or undo history. Existing sharing-copy options,
preview/clipboard/PNG and source isolation also pass. Reviewed the
[unchanged editor](screenshots/055-backend-unavailable-editor-unchanged.png) and
[sharing regression](screenshots/055-existing-sharing-regression.png).
Neither screenshot depicts rolling capture.

The [plan](scroll-capture-plan.md) now spells out user-manual-scroll interaction,
fixed session baseline, single primary next-frame action, Finish/Cancel, failure
recovery, and the required hide/repaint/restore barrier. Successful native frames,
real permissions, fractional-DPI conversion, screen hotplug, timeout and window
exclusion are unverified. The adapter drops late results but does not forcibly
abort an already-blocking kernel pipe read. No automatic retry loop, scroll input
injection, fake button or full scrolling feature is advertised.

## 056 — Cancellable manual-scroll controller protocol (no public workflow yet)

Added a GUI-thread controller around the isolated collector with monitored backend
ownership. Capture-next first enters WaitingForHidden; only an explicit matching
ticket and unchanged output-layout confirmation may start capture. No fixed sleep
is treated as a compositor barrier. Separate barrier/capture deadlines fail safely.
Cancel, late replies, synchronous backend failure, backend loss and controller
teardown restore the requested hidden-window state and do not touch the editor.
Single-output selection mapping uses logical edge positions relative to the output
and checks native dimensions, rather than scaling global coordinates blindly.

The first compile caught a constructor-parameter name shadowing the cancel method;
this was corrected. A later intermediate test run caught the old object/new test
mismatch while refining cancellation before a hide request. The final rebuild
uses the matching sources and tests. Final CTest 8/8, static checks 16/16, full
build and whitespace checks pass. Controller tests cover DPR 1/1.25/1.5/2 and
negative screen coordinates, cross-output/native-size rejection, exact combined
pixels, hide confirmation ordering, repeated clicks, cancel/restart, stale frames,
layout change, duplicate guidance, synchronous failure, backend/controller loss,
and real timer expiry for both preparation and capture. Expiry retains accepted
pixels, cancels active work and ignores its late result.

Reviewed the [existing editor isolation regression](screenshots/056-existing-editor-isolation-regression.png).
This is existing UI after an unavailable-backend check, not a rolling-capture
interface or demonstration. No user menu constructs the controller yet. Backend
callbacks in future integration must use QObject-context connections; UI teardown
from notification handlers should use deleteLater. Required real region selection,
window exclusion/repaint confirmation, localized accessible controls and successful
KWin capture/export acceptance remain in [the plan](scroll-capture-plan.md).

## 057 — Verified API5 compositor exclusion gate

Upstream investigation found a real compositor exclusion mechanism, replacing the
need to invent a timing barrier: ScreenShot2 API5 supports hide-caller-windows and
filters the DBus caller's PID while rendering a separate scene. Confirmed official
v6.7.0 XML, DBus and renderer sources; v6.8.0 also advertises API5, whereas checked
v6.3.0 through v6.6.0 tags advertise API4. References and implementation implications
are recorded in the rolling capture plan.

The dedicated frame adapter now probes Version asynchronously for every request,
with no stale global version cache. Only API>=5 issues CaptureScreen with explicit
caller-window exclusion and native pixels. Failed/unsupported probes fail closed.
Probe cancellation uses the same generation guard as frame callbacks, and returned
output metadata must match exactly. This does not introduce any public menu yet.

Final build, CTest 8/8, static 16/16 and whitespace checks pass. Existing offscreen
unavailable-backend/editor-isolation regression passed and its
[screenshot](screenshots/057-api-gate-unavailable-editor.png) was reviewed. This is
not an API5 successful-capture test. A local isolated DBus test could not be started
because this sandbox forbids Unix socket creation; no workaround or claim of live
KWin acceptance is made. Native API5 result/exclusion, denial, probe/frame timeout
and hotplug remain unverified. The next substantive milestone is a single integrated
selector/session-panel/native-backend workflow, not another synthetic capture demo.

## 058 — Integrated manual-scroll technical preview, explicitly default-off

Connected a standalone display picker, fullscreen one-output drag selector,
manual frame panel, isolated controller, API5-only KWin request source and separate
result pin. It is only accessible with `NKSHOT_EXPERIMENTAL_SCROLL=1` and only in
image mode; no production default enables it. Capability preflight precedes region
selection and every frame rechecks the real compositor API. There is no automatic
scroll injection or sleep-based legacy fallback.

The panel has one primary action (Select region, Capture first, Capture next),
secondary Finish and Cancel. During selection/session the original editor/pin/text
windows are hidden for target access and their previous visibility is retained for
restoration. Selector failure, cancellation, output removal and teardown restore
windows; result-preview failure retains the composed image for retry. Original
image/history/export state is separate. These native paths are implemented but
not accepted on a real desktop yet.

Build, CTest 8/8, static 16/16 and whitespace checks pass. Actual offscreen tests
click the real opt-in More entry, verify default-off and video-mode exclusion,
window reuse, unavailable-backend explanation, disabled selection, Escape close,
reopening and unchanged original pixels/history. Existing sharing-copy clipboard/
PNG and composition workflows pass. The visual review caught and corrected a white
native display picker in dark mode, replacing it with the same Kos control surface.

Reviewed actual [unavailable preview](screenshots/058-preview-light.png),
[dark](screenshots/058-preview-dark.png) and [narrow](screenshots/058-preview-compact.png)
windows. Separate [selector](screenshots/058-preview-light.png.selector-layout.png)
and [panel](screenshots/058-preview-light.png.session-layout.png) screenshots are
**layout diagnostics only**: tests override the QML display phase, keep the C++
phase in Setup, and cannot accept any region or frame. They are not native desktop
capture examples. Existing [sharing regression](screenshots/058-existing-sharing.png)
was also checked.

Real KWin success, permission denial, compositor PID exclusion, fullscreen input,
window restoration, fractional-DPI/hotplug behavior, native accessibility and
end-to-end scrolling export remain unverified in this socket-restricted cloud.
Selector keyboard resizing is not implemented. The new [preview guide](scroll-preview.md)
spells out the explicit opt-in, bounds, workflow and acceptance gaps. Do not promote
this prototype to a default feature until native testing passes.


## 059 — Keyboard-adjustable region selection and explicit confirmation

The default-off scrolling preview now supports mixed mouse/keyboard selection.
Drag release preserves the rectangle rather than committing immediately. Arrows
seed/move it; Alt+arrows resize; Shift uses ten-point steps. A compact confirmation
bar shows physical-pixel dimensions, Cancel and Use region. Enter works for the
selector and focused action buttons; Tab follows the selector/Cancel/Use region
path. The overlay dims only outside the region, leaving the chosen area clear.
Invalid sizes disable confirmation and show the applicable size limits. Accessible
names/descriptions include position, dimensions and keyboard guidance.

Actual offscreen key tests found an initial read-only QML rect mutation issue:
seeding worked but the first move did not. It was corrected by computing numeric
coordinates before writing the endpoints. A missing test-only QSignalSpy include
was also caught at compile time and fixed before the final build.

Final build, CTest 8/8, static checks 16/16 and whitespace checks pass. Actual UI
events cover keyboard-only seeding, move/Shift move, Alt resize, mixed drag and
keyboard adjustment, edge/minimum clamping, Tab focus, Return activation, keyboard
Cancel, too-small-region disablement, reopening and unchanged source/history.
Default-off and video-mode gates remain verified; the compact non-selector path
still checks Escape cancellation.

Reviewed [light keyboard layout](screenshots/059-selector-light.png.keyboard-layout.png),
[dark keyboard layout](screenshots/059-selector-dark.png.keyboard-layout.png),
[invalid-size guidance](screenshots/059-selector-light.png.invalid-region.png) and
[narrow unavailable state](screenshots/059-selector-compact.png). Selection pictures
are explicitly **layout/input diagnostics**, not desktop captures: the test overrides
only QML display phase, retains C++ Setup state and accepts zero frames. The clear
area is the test window's blank background, not a captured desktop image.

No native KWin success, compositor focus/transparency or screen-reader acceptance
is claimed. The preview remains default-off. The current preview guide replaces
its former “keyboard resizing not implemented” limitation with the implemented
controls and remaining native acceptance boundary.

## 060 — Independent local text extraction from pinned images

Pinned snapshots now have a local Extract text action using the user's configured
OCR languages. A separate selectable text window supports Copy all and UTF-8 Save
As. It does not automatically replace the clipboard, alter the source image,
annotation history or language settings. Empty/missing-model results explain the
problem in place instead of creating an empty result window. This is practical
whole-image OCR, not inline word-highlight Live Text or translation.

The reusable `LocalTextExtraction` helper owns a Tesseract instance per job, with
bounded image size, language validation, a two-job budget and cooperative
recognition cancellation/deadline. Closing a pin suppresses late results; hidden
or minimized pins keep successful text pending until the next explicit action,
so a late result cannot interrupt the scrolling region selector. No network
processing or forced worker termination is added.

The real offscreen UI test clicks the toolbar, recognizes English and Chinese,
checks repeat protection, copies and saves exact UTF-8 text, hides/reopens the pin
and closes it during a fresh job. Source/history/configuration and automatic
clipboard non-mutation are asserted. Blank and unavailable-model UI paths are
checked separately. The existing pin Copy/top-toggle/Save As/cancel/reopen/source
isolation tests still pass. CTest 9/9 and static checks 16/16 pass.

Visual review caught the fifth toolbar action clipping at 260 pixels. Icon-only
buttons now use explicit 36-pixel widths, and the test asserts every action stays
within the window. Reviewed the corrected [compact pin](screenshots/060-pin-text-compact.png.pin.png),
[dark pin](screenshots/060-pin-text-dark.png.pin.png),
[English result](screenshots/060-pin-text-english.png),
[Chinese result](screenshots/060-pin-text-chinese.png),
[empty state](screenshots/060-pin-text-empty.png) and
[missing-language state](screenshots/060-pin-text-unavailable.png).
All pictures use synthetic non-personal fixtures and software Qt rendering.

The available environment was rechecked: only the cloud computer is available,
without a live KDE session. Native focus/accessibility, general OCR accuracy,
compositor capture/recording and GPU liquid-material acceptance remain unverified.
The 15-second recognition deadline does not bound language-model initialization.
No scrolling-preview gate was relaxed.

## 061 — Read long pinned images without changing their pixels

Previously, pins and image-composition results were always shrunk to the window,
which made long screenshots difficult to read. Pins now support view-only zoom
and panning. A compact footer outside the image groups Fit, Width, zoom out, percentage
(reset to 100%) and zoom in. Mouse dragging and scrollbars reach offscreen content;
Ctrl+wheel anchors zoom to the pointer. Ctrl+0/Ctrl+1 and standard zoom shortcuts
are available. Fit follows window resizing, with a 400% maximum manual scale.
Width starts at the top and fits horizontally, avoiding repeated sideways panning
when reading a long image. Original image data, full-image copy/save, OCR and
source edit history are preserved.

Real Qt event tests cover Fit/100%, drag, scale limits, resize, full-resolution
clipboard pixels, keyboard shortcuts and Ctrl+wheel. The wheel test caught a QML
WheelEvent coordinate-property mistake before release; it was corrected to use
`x`/`y`. Standard shortcut bindings were also changed to `sequences` so platform
alternate bindings are not silently omitted. Existing pinned OCR lifecycle and
PNG save/source-isolation regressions pass; build, CTest 9/9 and static 16/16 pass.

Reviewed [Fit overview](screenshots/061-pin-read-light.png.fit.png),
[fit-width reading](screenshots/061-pin-read-light.png.width.png),
[readable 100% view](screenshots/061-pin-read-light.png),
[dark view](screenshots/061-pin-read-dark.png) and
[260-pixel compact view](screenshots/061-pin-read-compact.png).
These are actual software-rendered Qt windows with an explicitly generated text
fixture. At 100%, one image pixel occupies one logical Qt unit; physical high-DPI
1:1 output, touch/trackpad behavior and native stacking are not certified.
No pinch gesture or compositor/recording success is claimed. Environment lookup
again returned only the cloud computer, not a real KDE desktop.

## 062 — Grouped playback speed and sound controls

Recording previews gain one Playback settings button rather than a row of extra
controls. Its compact popup groups speed (0.5×–2×), mute and volume. Silent media
shows why sound controls are unavailable. These are preview settings; source
files and capture preferences are unaffected. GIF/WebP retain their independent
animated-image decoder route without misleading audio/speed controls.

The actual Qt multimedia backend loads a generated six-second H.264/AAC fixture.
UI events exercise mute/unmute, volume, 2× selection, Escape, reopening and outside
dismissal, with byte-for-byte source preservation. A silent MP4 verifies disabled
sound controls; GIF, WebP and corrupt MP4 retry regressions pass. Build, CTest 9/9,
static 16/16 and whitespace checks pass. Visual review replaced platform-default
boxy combo/volume controls with the shared NextKde colors and rounded treatment.
Reviewed [light settings](screenshots/062-playback-light.png.settings.png),
[dark settings](screenshots/062-playback-dark.png.settings.png),
[compact settings](screenshots/062-playback-compact.png.settings.png) and
[silent-file guidance](screenshots/062-playback-silent.png.settings.png).

All images are actual software-rendered windows using synthetic media. The cloud
reports no audio output device, so metadata and output-property tests do not prove
audible playback or A/V synchronization. Real KWin capture, microphone/system
recording and capture pause/resume remain unaccepted. A follow-up inspection of the KPipeWire base class found pause()/resume() APIs.
Their local implementation toggles the video source stream without coordinating
audio capture or removing paused time from the media timeline. They are not yet
integrated into NkShot or accepted as seamless recording pause/resume. The earlier
derived-interface-only conclusion that no pause API exists was incomplete.

## 063 — Validate recording output before requesting capture

A focused recording-core audit found that a directory named `clip.mp4` passed the
old parent-folder-only validation. A real temporary-filesystem test now reproduces
and rejects it. The new preflight rejects non-local/relative targets and existing
non-regular files, retains writable existing files, and creates only parent
folders for new files. It never opens/truncates the output itself.

Output and encoder checks now precede compositor stream creation and window
minimization. Stale recorder node/output properties are cleared first; selection
requests are resolved before changing them. An unavailable stream has a runtime
failure path instead of relying on an assertion, checked before frame-size use.

Independent review caught an initial overrestriction requiring write permission
on the parent of an existing writable file. That was corrected and tested, along
with FIFO rejection. It also identified remaining old-stream/state-connection
risks, recorded in the verification guide rather than claimed solved. No broad,
unvalidated lifecycle rewrite was bundled into this small fix.

Final build, CTest 10/10, static 17/17 and whitespace checks pass. Tests verify real
filesystem rejection and byte preservation plus source ordering; they do not
simulate a successful native recording. No UI was redesigned in this stage.
The [playback regression screenshot](screenshots/063-playback-regression.png)
is an actual software window using synthetic media, not evidence that recording
startup or compositor capture succeeded. Real KDE failure injection, disk-full
handling and complete recording lifecycle acceptance remain open.

## 064 — Isolate capture callbacks and recorder attempt cleanup

Capture stream callbacks now verify a generation, including callbacks already
queued before disconnect. A retired source cannot start, fail or stop a later
attempt. Recorder state/error subscriptions are installed once per recorder.
Failures suppress saved-success notifications, and the next completed attempt
uses a clean recorder. Stop-time callbacks are quiesced while the actual source
remains alive until encoder draining completes. Pending source starts can be
canceled, and a late producer-started signal cannot reopen the recording UI after
Stop. No capture UI or public recording API was changed.

Independent static review identified and corrected three introduced edges before
freezing: a queued start escaping an immediate Stop, early compositor-source
delete during encoder draining, and a raw screen pointer surviving until queued
dispatch. Selection replies also carry an epoch. Recorder connections are removed
before destructor member teardown.

Real Qt queued-signal tests exercise stale created/failed/closed callbacks,
replacement, twenty repeated attempts and source retention while quiesced. Source
assertions enforce one recorder state/error subscription and adapter guards.
Build, CTest 11/11, static 18/18 and whitespace checks pass. The unchanged
[playback window regression](screenshots/064-playback-regression.png) was reviewed
using synthetic media; it is not a screenshot of native capture success.

No real KWin/PipeWire start/stop, encoder write failure, audio synchronization or
hot-unplug acceptance is claimed. The recording verification guide records a
pre-existing dependency initialization path that can omit error/completion signals;
it remains open rather than being masked with a fake completion timeout.

## 065 — Fix real KPipeWire initialization-failure shutdown

A standalone program using the installed public PipeWireRecord API reproduced the
null-source shutdown failure: no error/completion was emitted, Stop left Rendering,
and destruction timed out (exit 124). Both relevant local source files were
byte-compared with the published pinned upstream commit. The inspected upstream
master also retained the faulty initialization branch.

The minimal downstream patch retains the failed source for normal cleanup,
emits its error (with a nonempty fallback), and posts asynchronous initialization
after installing all error/completion handlers. The same probe then exited 0.
The patch and original-file hashes are shipped with build instructions, and
`git apply --check` passes against the clean pinned archive. No system binaries
were modified; the isolated development prefix was rebuilt and installed.

The new test exercises the real linked library six times across ordinary failure,
Stop on error and immediate Stop. It verifies exactly one error/completion, no
output creation or change to existing bytes, descriptor closure and returning destruction. Independent review
found no patch blocker and prompted the additional stop cases and exact environment
restoration. Build, CTest 12/12, static 18/18 and whitespace checks pass. Upstream
video encoder checks remain 11 passed/6 skipped; audio checks remain 5 passed.

No UI changes were needed. The actual [playback regression window](screenshots/065-playback-regression.png)
was reviewed using synthetic video. These results validate failed initialization,
not successful KWin capture, recording pause/resume or audio/video synchronization.
The fix applies to the documented patched dependency, not arbitrary installed
KPipeWire versions, and it is not represented as upstream-accepted.

## 066 — Independent quick-note image cards

Quick Note adds a focused 1Capture reference workflow rather than another annotation
mode: write plain text, preview a styled card, then copy the exact image or pin an
immutable copy. Pins and notes share one menu group instead of expanding the
root More list. The draft stays in memory across window close/reopen, with an
explicit reminder that it is not saved across application restarts.

The reusable Qt Gui renderer handles Chinese/plain text and three card palettes,
with bounded dimensions and clear empty/oversized disabled states. Original
screenshots, undo history and already-created pins are isolated. The draft window
adopts an existing note across viewer replacement, restores a minimized note and
focuses the editor on first open. Review caught the initial focus/minimize issues;
the UI test now asserts initial focus instead of artificially giving it focus.

Build, CTest 13/13 and static checks 18/18 pass. Actual software-window interaction
covers the submenu, Chinese input, copy/pin pixel equality, style change, close and
reopen, empty/oversized input and source isolation. Existing pin Save As regression
checks cancel, reopen, one-dialog behavior and PNG pixel equality. Light, dark and
400×560 screenshots were inspected, including empty, limit and Night-card states.
An implicit-width binding loop was eliminated before the final captures.
Review also found that independent draft windows bypassed ordinary capture hiding.
Screenshot/recording startup now hides them without deleting the draft; scrolling
selection includes them in its existing hide/restore list. The hide/reopen helper
is UI-tested, but compositor exclusion timing remains a native-KDE acceptance item.

[Light](screenshots/066-quick-note-light.png),
[dark](screenshots/066-quick-note-dark.png), and
[compact](screenshots/066-quick-note-compact.png).
These are actual QML windows with synthetic content and software rendering, not
native KWin topmost/recording or GPU material acceptance. Quick Note is not a
persistent notes database, cloud sync or full 1Capture parity.

## 067 — Explicit local code-reading workflow

Read text and codes groups OCR and QR/barcode reading without adding a new root
menu entry. The explicit scanner uses a private edited-image snapshot, recognizes
locally and opens the shared plain-text result reader. Copy and UTF-8 Save As are
intentional actions. Payloads containing URLs, markup or commands are never
interpreted or launched by this workflow. The original automatic-scan preference
remains separate.

Recognition is bounded to 20 megapixels and repeated clicks are coalesced. Binary
content and no-code images receive guidance without clipboard changes. Results
are discarded if the viewer is hidden, becomes video or its image changed while
recognition ran. The underlying renderer/helper remains independent of the UI.

Real Prison-generated QR, Chinese, URL/literal markup, binary, Code128 and alpha/DPI
fixtures exercise the decoder. Actual QML menu clicks verify explicit copy/save,
no automatic clipboard mutation, one result per request and unchanged source
pixels/history. Shared OCR reading/export remains a regression gate. Software
screenshots use synthetic fixtures; native dialogs, desktop capture and general
damaged-code accuracy remain unverified. Multiple codes are not enumerated.

Final build, CTest 14/14 and static checks 18/18 pass. Review found that
maximizing during a scan unnecessarily discarded its result; only minimizing now
invalidates the window-state epoch. The final interaction pass also verifies
maximizing, repeat scanning after closing a result, and closing the result with an
outstanding Save As dialog. Hide/show still discards the in-flight result.

[Light](screenshots/067-code-light.png), [dark](screenshots/067-code-dark.png),
[compact](screenshots/067-code-compact.png),
[no code](screenshots/067-code-blank.png),
[binary content](screenshots/067-code-binary.png),
[discarded stale result](screenshots/067-code-stale.png), and
[shared OCR regression](screenshots/067-ocr-regression.png) are actual software
window captures reviewed for this stage.
