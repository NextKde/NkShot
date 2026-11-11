# Rolling capture feasibility and acceptance plan

## Inspected source boundary

`ImagePlatform::doGrab()` emits accepted image/failure/cancel signals. The ordinary
`SpectacleCore` handlers for `newScreenshotTaken` and `newCroppableScreenshotTaken`
clear annotations, replace the editor image, reset export state and may invoke
auto-export or selection windows. A timer that repeatedly calls
`takeNewScreenshot()` would therefore mutate the current editing workflow; it is
not a safe rolling-capture implementation.

A proper session needs an isolated collector/controller, not a loop around that
normal action. Capture is subject to KWin permissions and real output geometry.
The current cloud workspace lacks a live KWin/Wayland/session-DBus environment,
so no verified region frame stream is available. No rolling-capture menu has been
added that pretends fixture composition is live capture.

## First real desktop milestone

1. Select one region through the existing accepted permission/selection path.
2. Record its physical-pixel geometry, output identity and DPR. Keep the current
   editor/history separate. Show a small session control outside the region.
3. The user scrolls the target normally; an explicit capture-next action collects
   one frame only after the controls are hidden and the compositor has repainted.
   No injected scroll events or privileged input assumptions.
4. Reject region/output/DPR changes, capture failures, ambiguous/no-progress
   content and frames that would exceed bounds. Keep accepted frames and explain
   the next useful action without replacing the original editor.
5. Cancel frees only this session. Finish constructs an isolated result preview,
   verifies exported pixels and offers Copy/Save. Test repeated sessions.

## Subsequent milestones

After actual manual-scroll acceptance, assess overlap tolerance for sticky
headers and animations without inventing content. Then evaluate horizontal
orientation and explicit automatic scrolling only where an authorized backend
actually supports input. Desktop permission denial and absent capabilities must
have honest states. Unit-tested frame assembly alone is not acceptance of any
of these milestones.
