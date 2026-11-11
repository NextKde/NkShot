# Manual scrolling capture: opt-in technical preview

**Not enabled by default and not yet accepted on a real KDE desktop.** This is an
integrated implementation to exercise the real selector/session/API5 route, not
an automatic scrolling feature or a successful cloud capture demonstration.

## Opt in

Start a development build explicitly:

```sh
NKSHOT_EXPERIMENTAL_SCROLL=1 nkshot --new-instance
```

The new-instance flag avoids reusing a process started without the opt-in.
From an existing screenshot's More menu, choose **Scrolling capture (preview)**.
Without that exact opt-in value, the entry is hidden and the open method refuses
to create a window. Do not enable this variable in packaged defaults yet.

## Capability and workflow

1. The preview first checks the actual KWin ScreenShot2 Version property. API5 is
   required (upstream Plasma 6.7+; use runtime capability, not the desktop label).
   An unavailable desktop gets an explanation and a disabled Select region button.
2. Choose one display and Select region. NkShot editor/pin/text windows are hidden
   for access to the target, with their previous visibility state retained.
3. Drag a region on that display, then fine-tune it and choose Use region or Enter.
   Mouse release no longer commits immediately. Esc cancels and restores hidden
   windows. Keep sticky headers, animation and other changing content outside it.
4. Choose Capture first. Scroll the target yourself, then Capture next. No wheel
   injection or automatic scrolling happens. Accepted-frame count and failure
   guidance are in the compact panel. Cancellation and timeouts preserve safety.
5. After at least two accepted frames, Finish opens an independent pin preview
   with Copy/Save As. Original image, undo history and export state are unchanged.
   Failed result-window creation retains the result for a Finish retry.

Every actual frame independently rechecks API5 and explicitly asks the compositor
to omit all windows from the caller PID. This is not a fixed-delay repaint guess.
Changed output layout/scale, failed captures, unsafe/ambiguous/no overlap and size
limits reject the candidate. There are at most 16 accepted frames and existing
40 MP/8192-width/32000-height output limits. Exact matching is deliberately
conservative and may reject ordinary pages with animations or sticky content.

## What has and has not been checked

Cloud tests cover default-off visibility/method gating, actual opt-in menu opening,
window reuse, unavailable-backend explanation, disabled actions, Escape close and
reopening, unchanged editor pixels/history, and dark/narrow layouts. A separate
visual-only test temporarily renders the selector component and drags its rectangle;
it leaves the C++ phase in Setup and cannot accept a region or frame. Its screenshot
is labeled a layout diagnostic, not a real desktop capture. Controller/core tests
cover numerical coordinates, frame assembly, cancellation, stale callbacks and
actual timer expiry.

**Not yet verified:** successful KWin capture or caller-window exclusion on native
hardware; permission-denial UI; real fullscreen selection/focus; display unplug or
fractional-DPI geometry; hidden-window restoration under native window-management
rules; end-to-end Finish/export from real scrolling. Selector keyboard movement, resizing, Tab focus and explicit confirmation are
implemented and exercised with offscreen Qt input. Actual native compositor focus
and screen-reader acceptance remain pending.

The cloud environment has no KWin and forbids Unix socket creation, so even a test
DBus could not be started. No mock image is routed through the product as a real
capture. See [source evidence and architecture](scroll-capture-plan.md) and the
[capability matrix](capability-matrix.md).

## Fine-tuning a region (059)

- Arrow keys move the selection by one logical point. The first arrow seeds a
  centered region when none exists. Shift + arrows moves ten points.
- Alt + arrows changes width/height from the bottom/right edges. Add Shift for
  ten-point size adjustments. Movement and resizing stay inside the display.
- Physical-pixel dimensions are shown in the floating confirmation bar. Too-small
  or over-limit regions disable Use region and explain the limits.
- Tab moves between the selector and Cancel/Use region. Enter activates the
  focused button or confirms while the selector has focus; Esc always cancels.
- Mouse drag and keyboard adjustment can be mixed before confirming. Keyboard
  steps use logical points; the displayed dimensions use the output DPR.

The layout has accessible names/descriptions including position, size and keys.
That is implementation evidence, not a claim that an actual screen reader or
native Wayland focus behavior has been certified.
