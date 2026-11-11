# Manual scrolling capture: discoverable technical preview

**The preview entry is available by default; native KDE capture is not yet accepted.**
This is the existing integrated selector/session/API5 route, not automatic scrolling
or a successful cloud capture demonstration.

## Open the preview

Start NkShot normally. From an existing screenshot's More menu, choose
**Manual scrolling capture (preview)**. No environment flag is needed. The entry
is image-mode only. Unsupported desktops can open the panel and read the reason;
they cannot select a region, capture a frame or produce a result.

The user scrolls the target. Capture each frame manually, or explicitly enable
**Collect while scrolling** for periodic collection. API5 window
exclusion remains mandatory; making the entry discoverable does not remove that
safety check or establish native acceptance.

## Capability and workflow

1. The preview first checks the actual KWin ScreenShot2 Version property. API5 is
   required (upstream Plasma 6.7+; use runtime capability, not the desktop label).
   An unavailable desktop gets an explanation and a disabled Select region button.
2. Choose one display and Select region. NkShot editor/pin/text windows are hidden
   for access to the target, with their previous visibility state retained.
3. Drag a region on that display, then fine-tune it and choose Use region or Enter.
   Mouse release no longer commits immediately. Esc cancels and restores hidden
   windows. Keep sticky headers, animation and other changing content outside it.
4. Choose Capture first, then scroll and Capture next, or turn on **Collect while
   scrolling**. Collection is off by default. It requests one frame at a time,
   waiting about one second after each completed frame. Scroll the target yourself:
   no wheel injection or automatic scrolling happens. Unchanged frames are skipped.
   Turn the same control off to pause, including during a pending request; late
   results from that request are discarded. Accepted pixels remain available.
   Unsafe alignment, capture errors or changed display geometry stop collection
   with an explanation. Adjust the target and retry manually or enable collection
   again. At 16 accepted frames collection stops; inspect and Finish.
5. Review the accepted composite in **Alignment preview**. Fit shows the page;
   **100%** enables pixel inspection and scrolling using only visible 1024×1024
   image tiles, rather than uploading a whole tall image as one GPU texture. **Seams** marks append joins
   only in the preview, never in saved pixels. If a frame is rejected, the accepted
   image stays visible and **Retry frame** captures again after you adjust the scroll.
6. Finish stops collection and any pending request. After at least two accepted
   frames, it opens an independent pin preview
   with Copy/Save As. Original image, undo history and export state are unchanged.
   Failed result-window creation retains the result for a Finish retry.

Every actual frame independently rechecks API5 and explicitly asks the compositor
to omit all windows from the caller PID. This is not a fixed-delay repaint guess.
Changed output layout/scale, failed captures, unsafe/ambiguous/no overlap and size
limits reject the candidate. There are at most 16 accepted frames and existing
40 MP/8192-width/32000-height output limits. Each frame is limited to 8192 pixels
per side and 16 MP. The scrolling-only matcher tolerates bounded pixel noise and
small changed islands, but rejects weak texture/coverage, competing offsets,
reverse/no progress and detected fixed headers. Keep animation and sticky content
outside the selected area. Ordinary low-information pages can still be rejected;
a smaller scroll step or more distinctive region is safer than a guessed join.

Matching uses consecutive actual accepted frames. A successful estimate appends
only the new tail to the accepted composite; no averaging, blending, warping or
reconstruction is performed. Previously accepted pixels are retained. A changed
small overlap island may therefore retain the older appearance. The exact
file-composition matcher is separate and unchanged.

The bounded search samples at most 48×24 positions per signed displacement,
requires 94% overall and textured-anchor agreement, broad spatial coverage within the actual textured footprint (not the surrounding white margins) and an
8-point runner-up separation, then checks the complete chosen overlap. A near tie may be fully checked only
when there are at most eight competing candidates and no more than 32 million
compared pixels in total; otherwise it is rejected. Per-channel
tolerance is 8 levels. These are conservative heuristics, not a proof of semantic
identity or a probability of correctness; inspect the joins. Search is bounded by
frame dimensions and does not compare every pixel at every displacement.

## What has and has not been checked

The focused cloud test path covers default menu opening, image-only gating, rejected
Capture/Finish and unchanged clipboard/source when unavailable,
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

The cloud environment has no KWin. Dedicated tests can use an isolated loopback
D-Bus session, but that does not provide a compositor. The 082 test-only friend
replaces the controller's capture dependency with controlled synthetic frames to
exercise the real session, provider, preview, retry, Finish and pin export. It is
compiled only into the opted-in UI harness; no production environment override
can make the native backend accept synthetic frames. Such screenshots are labeled
synthetic alignment diagnostics, not successful desktop captures. See [source evidence and architecture](scroll-capture-plan.md) and the
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
