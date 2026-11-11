# NkShot product delivery plan

The goal extends beyond a Spectacle reskin: retain reliable capture/editing and
add useful 1Capture-style workflows with the shared NextKde visual language.
Use the [capability matrix](capability-matrix.md) as the single status source;
code, automated checks and real KDE acceptance are separate columns.

## Working development baseline

The project builds with Qt 6.11+, KF6 6.30+ and matching Plasma/KPipeWire 6.8
components. Synthetic-fixture interaction tests cover core editing/export, local
eng/chi_sim OCR, pins, conservative image composition/stacking, sharing-copy
transforms and actual local media playback. This is not a release-tested product.
See [iterations](iterations.md), [building](building.md) and
[recording boundaries](recording-verification.md) for exact evidence/limits.

The 075 startup redesign separates target selection from capture/recording and
retains existing settings and cancellation. Its software UI validation is not a
substitute for the native capture and recording release gates below.

076 adds a post-selection recording countdown and cancellation/retry lifecycle
gate. Ten adapter groups and five software Core UI scenarios passed; the full
CTest suite passed on an isolated test bus. This does not close the native recording release gate. See [recording preparation](recording-preparation.md)
for states, conservative partial-file cleanup and explicit evidence limits.

077 adds explicit recording-output ownership and guarded private cancellation
cleanup; helper/adapter/UI and 21-target CTest validation passed. Explicit user outputs remain preserved, and
same-UID pathname races are not an atomic guarantee. This reliability work does
not replace the native recording acceptance gate. See [output ownership](recording-output-ownership.md).

078 tightens pending cancellation and GUI status-control continuity, including a
registered-host check before recording commit. Private-bus protocol/Core checks pass; protocol
availability does not close native tray visibility or first-frame acceptance.

## Next substantive workflows

1. **Rolling capture:** verify a fixed-region, manually scrolled capture session
   first in a real authorized KDE environment. Require stable coordinates/DPI,
   accepted-frame progress, cancellation and conservative alignment. Only then
   assess automatic and horizontal scrolling. File composition is not scrolling.
2. **Non-destructive sharing:** background/watermark copies and reusable image
   processing must preserve original documents/history and verify exported pixels.
3. **Local text workflows:** whole-image OCR from immutable pins is implemented
   in 060, with explicit rectangle-only OCR in 069; bounded inline OCR word-range selection is implemented in 070. Character-level
   selection and full system Live Text parity remain gaps. Drag-edge scrolling is implemented in 071. Offline English ↔ Chinese translation is implemented in 080 with explicit model installation,
   immutable original and separate translation copy. Broader languages/quality remain open;
   any external provider requires explicit opt-in identifying data/destination.
4. **Recording beyond capture:** local MP4 clip export (072) and bounded silent GIF Maker (073) are implemented; actual KWin/PipeWire recording/A/V acceptance,
   then recording annotation, click effects and camera overlays where feasible.
5. **Other verified reference gaps:** real desktop color-picker acceptance,
   spotlight/magnifier and AI erase/background removal. Do not label ordinary
   editor tools as these workflows or add fake placeholders.

Quick-note image cards are implemented in 066 as a session-only workflow, not a
persistent notes application. Image-local color sampling is implemented in 068 and a desktop portal client in 074;
real native desktop-picker behavior remains a separate acceptance gate.

## Release gates

- Build, all CTest tests, generated-import lint where available and static checks.
- Stage-install manifest isolation from Spectacle; real side-by-side D-Bus,
  settings, shortcuts, notifications and application identity checks.
- Actual region/window/screen captures with normal KWin permissions, repeated
  capture/cancel, mixed-DPI monitors and saved/clipboard pixel verification.
- Full edit interaction and native save/overwrite/portal behavior.
- Real region/window/screen recording, stop/save, audio/device/permission failures,
  A/V synchronization, output decode and accidental floating-UI capture checks.
- Real GPU liquid-material screenshots plus reduced-transparency/software fallback,
  dark/light/narrow layouts and keyboard accessibility.

Do not replace pending gates with synthetic screenshots, encoder initialization
or a count of commits/iterations. Never weaken compositor permission checks.

### Collector foundation

054 adds an isolated, bounded frame-session core with request tickets and retry/
cancel safety. This is not a desktop scrolling feature. Connect it only through
the accepted-frame adapter described in [the plan](scroll-capture-plan.md), and
validate real KWin output identity, DPR and permission failures before exposing UI.

### Verified compositor route

057 verified KWin ScreenShot2 API5 caller-window exclusion in upstream sources
(v6.7.0/v6.8.0; v6.6.0 still API4). The first integrated manual-scroll workflow
should gate on fresh runtime API>=5, explicitly exclude caller windows and avoid
a sleep-based legacy fallback. See the source references and remaining native
acceptance checklist in the rolling capture plan. This capability investigation
and adapter gate are not a delivered user-facing scrolling feature.

### Integrated technical preview

058 connects the actual selector/panel/controller/API5 adapter under an explicit
`NKSHOT_EXPERIMENTAL_SCROLL=1` gate. The default interface remains unchanged.
Unavailable-state/selection-layout tests do not establish native success. The
next gate is real desktop capture, focus/restoration, permissions, fractional DPI
and exported result acceptance; see [preview guide](scroll-preview.md).

082 advances the manual scrolling route with a conservative tolerance matcher,
accepted-composite/seam inspection and explicit rejected-frame retry. These are
validated with controlled synthetic frames, not native desktop acceptance. Real
KWin API5 capture, focus, exclusion and end-to-end scrolling on a user's page remain
next gates; automatic input/scrolling is not introduced.

083 adds opt-in continuous frame collection while the user scrolls, with one
pending request, pause/late-result isolation and retained progress on failure.
This reduces repeated panel switching but is not automatic scrolling, and does
not remove the outstanding native desktop acceptance gate.

### 084 recording pause/resume

Implemented acknowledged recording controls and a downstream A/V timeline patch,
not playback pause. Native KWin/device synchronization, hardware encoders and
static-target refresh after Resume remain the next acceptance boundary.

### 085 reopen local screenshots and recordings

One Open workflow now leads into existing annotation, playback and clip/GIF export.
Imports are validated before confirmed replacement, with source-safe copy defaults.
Scope is static supported images and short H.264/AAC MP4; [limits](open-local-media.md)
are explicit. Drag-and-drop, multiple documents and broader formats are not included.

### 086 default recording file loop

Default VP9/Opus WebM now uses Open → playback → source-preserving copy or
MP4/GIF range export. The contextual clip panel has a timeline-first hierarchy
with compact precise fields and optional Details. Sparse production-encoded
synthetic media and actual Qt interaction are tested; native capture and GPU
visual acceptance are still separate, outstanding work.
