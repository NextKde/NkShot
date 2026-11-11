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

## Isolated collector foundation (054, not a released capture workflow)

`Imaging/ScrollCaptureSession` is an independently tested, single-owner-thread
state machine. It has **no ImagePlatform connection, timer, capture button or
input injection**. A future adapter must supply real accepted frames, not claim
that the model itself captures the desktop.

Contract:
- `begin` fixes physical rect, stable output identity and DPR. Negative desktop
  coordinates are valid. Starting over while collecting/pending is refused.
- `requestFrame` allows exactly one pending capture and returns a monotonically
  increasing ticket. The adapter carries that ticket through asynchronous work.
- Matching capture failure consumes that request and allows retry. Stale tickets,
  including callbacks after cancel/restart, cannot consume a newer request.
- Frame geometry/DPR and profile changes, invalid images, duplicates, ambiguous
  overlap, no overlap and output overflow reject only that candidate. Accepted
  pixels/count remain intact. Bounds are 16 accepted frames and existing output
  width/height/pixel limits. The model retains only the cumulative result, not a
  growing full-resolution frame list.
- Captured buffers are detached; backend reuse cannot mutate accepted pixels.
- `finish` requires two accepted frames and no pending capture. It returns an
  isolated value image, releases session storage, and prevents another request.
  `cancel` releases only the collector. Neither touches the editor/export model.

Test fixtures cover exact assembly, single pending request, failed/repeated/stale
callbacks, cancel/restart, mismatched region/output/DPR/profile, no-progress and
ambiguous overlaps, bounds, frame cap, high DPI, buffer ownership and result
lifetime. These tests establish a collector contract only. Real capture routing,
permission/UI states, repaint exclusion, native output geometry and user-scrolled
frame acceptance remain unimplemented and require the desktop milestone above.
