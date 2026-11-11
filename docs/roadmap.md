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

## Next substantive workflows

1. **Rolling capture:** verify a fixed-region, manually scrolled capture session
   first in a real authorized KDE environment. Require stable coordinates/DPI,
   accepted-frame progress, cancellation and conservative alignment. Only then
   assess automatic and horizontal scrolling. File composition is not scrolling.
2. **Non-destructive sharing:** background/watermark copies and reusable image
   processing must preserve original documents/history and verify exported pixels.
3. **Local text workflows:** whole-image OCR from immutable pins is implemented
   in 060; inline word highlighting/selection remains a Live Text gap. Translation
   is requested separately and is not yet implemented. Prefer local processing;
   any external provider requires explicit opt-in identifying data/destination.
4. **Recording beyond capture:** actual KWin/PipeWire recording/A/V acceptance,
   then recording annotation, click effects and camera overlays where feasible.
5. **Other verified reference gaps:** dedicated color picking,
   spotlight/magnifier and AI erase/background removal. Do not label ordinary
   editor tools as these workflows or add fake placeholders.

Quick-note image cards are implemented in 066 as a session-only workflow, not a
persistent notes application.

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
