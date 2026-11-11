# Rolling capture feasibility and acceptance plan

Current status: [discoverable manual preview](scroll-preview.md) (079 entry, 058
integration, 059 keyboard refinement) connects the selector, panel, controller and
API5 adapter. The entry no longer needs an environment flag; the API5 safety gate
and missing native KWin acceptance remain. Numbered sections below record the evolving implementation;
statements about missing UI in 054–057 describe those earlier stages.

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

## Dedicated KWin request adapter (055, not yet a user workflow)

`Platforms/ScrollFrameKWin` creates its own `ScreenShotSourceScreen2` per request,
never sharing `SpectacleCore::m_imagePlatform` signals. It requests native pixels
without cursor/decorations, checks the output/geometry/DPR again when complete,
and crops only after exact full-output dimension and scale validation. Cropping
uses output-local physical pixels, never guessed global DIP conversion. It does
not rescale, pad or silently accept an altered region. Missing session DBus refuses
the request; ordinary KWin denial/error remains an error. One request can be active;
a six-second wrapper timeout/cancel drops that request without touching an editor.
Internal generation and external ticket checks suppress late response reuse.

The reused source's read continuation is now bound to its QObject context. This
prevents completion from invoking a deleted source and dispatches the result on
the owning thread. This does **not** claim to forcibly abort a kernel pipe read
already running in the worker: the result is ignored after source destruction.
A future adapter must not schedule unbounded retries of a hung source.

The adapter and collector are compiled into NkShot, but no user action instantiates
them yet. Tests cover crop/metadata/buffer invariants and Qt context-bound callback
lifetime; an actual offscreen app check covers unavailable-backend refusal with
unchanged editor pixels/history. Successful KWin capture, permission denial,
timeout during real capture and screen hotplug are **not** proven by those tests.

### Manual workflow design and remaining controller obligations

- Begin from a clearly named “Scrolling capture” task only when the complete route
  is available. Do not call ordinary New Screenshot as an implementation shortcut.
- Select a single-output region; preserve the original editor and undo/export
  state. Store initial output identity, logical screen geometry, DPR and explicit
  output-local physical rectangle. Reject configuration changes across requests,
  not just during an individual request. The adapter's per-request snapshot alone
  is insufficient to enforce the whole session baseline.
- One small controller should show accepted frame count and status. The primary
  action is “Capture next frame”; Finish remains disabled until two frames are
  accepted and no capture is pending. Cancel is always available. The user scrolls
  the target normally: no wheel injection or hidden automatic scrolling.
- Before calling the adapter, hide the controller, editor and other NkShot windows
  that intersect the region and establish a compositor repaint barrier. A fixed
  timer alone is not evidence of exclusion. Restore the controller on each result.
- On `request` returning false, release the collector's matching pending ticket as
  failure. On response, pass the original ticket and baseline metadata to the
  collector. Duplicate/no-progress asks the user to scroll farther; ambiguous
  content asks for a smaller step. Neither loses accepted work.
- Finish opens a separate sharing preview; Cancel discards only the session.
  Interrupted/denied/hotplug states must restore hidden windows. A second session
  must not be affected by pending callbacks from the first.

Do not expose this workflow until the controller/region selector/repaint exclusion
and real desktop acceptance have been implemented and checked. Current source is
backend groundwork, not a disabled fake UI or an automatic scrolling claim.

The source also rejects incomplete raw pixel reads instead of accepting a partly
initialized QImage. Native pipe transport/short-read behavior remains a real KWin
acceptance item; cloud tests do not emulate that compositor protocol.

## Manual controller foundation (056, not exposed to users)

`Imaging/ScrollCaptureController` now coordinates the collector through injected
backend request/cancel functions whose owner is monitored with QPointer. It has
no SpectacleCore/ExportManager/editor references. Backend destruction cancels the
session; controller destruction aborts an active request and restores any hidden
windows through the restoration signal. No UI constructs this class yet.

The explicit states are Idle → Ready → WaitingForHidden → Capturing → Ready,
with Finished/Cancelled end states. Capture-next does **not** call a backend
until the matching `confirmHidden(ticket, currentLayout)` arrives. A four-second
barrier deadline is a failure timeout, not a presumed repaint delay. An eight-
second capture deadline aborts the backend and preserves accepted frames. A late
confirmation, frame or failure cannot consume a newer ticket. Reentrant clicks
are rejected during transition notifications; synchronous failure/cancellation
is covered by tests. Restore is emitted exactly once per hide request on tested
completion, failure, cancellation, timeout and destruction paths.

`scrollSelectionPixels` accepts a global logical selection only if wholly inside
one output. It computes both physical edges relative to that output, preserving
negative desktop coordinates and fractional DPR without independently scaling
width and origin. The expected native output size must agree with geometry/DPR.
The controller snapshots the full output layout and compares it both before
request and on frame delivery, not merely across one backend call. Unit tests
cover DPR 1/1.25/1.5/2, crossing-output rejection and native-size mismatches. These
numerical tests do not certify actual fractional-scale KWin behavior.

### Still required before a user entry point

A real UI/compositor coordinator must wire hide/restore signals, actually exclude
all intersecting NkShot windows, obtain a trustworthy repaint/exclusion barrier,
and pass current layout to confirmation and response calls. The test code calls
confirmation explicitly to exercise the state machine; that is not proof of a
compositor barrier. Need accepted single-output selection, actual KWin wiring,
localized user statuses, accessible keyboard interaction, real denial/hotplug/
interruption checks and successful image export before enabling the feature.

## Verified compositor exclusion path (057, supersedes guessed repaint barriers)

Primary-source inspection establishes a better route than a sleep after hiding:

- [KWin v6.7.0 ScreenShot2 XML](https://github.com/KDE/kwin/blob/v6.7.0/src/plugins/screenshot/org.kde.KWin.ScreenShot2.xml)
  documents `hide-caller-windows` for screen/area/workspace captures, default true,
  available since **API version 5**.
- [v6.7.0 DBus implementation](https://github.com/KDE/kwin/blob/v6.7.0/src/plugins/screenshot/screenshotdbusinterface2.cpp)
  resolves the DBus caller PID and passes it to the screenshot renderer.
- [v6.7.0 renderer](https://github.com/KDE/kwin/blob/v6.7.0/src/plugins/screenshot/screenshot.cpp)
  creates a separate scene view, filters windows with the calling PID (and windows
  excluded from capture), then renders that filtered scene. This is compositor-side
  exclusion, not a claim that Qt `hide()`, `frameSwapped` or a timer proves repaint.
- Checked release tags v6.3.0, v6.4.0, v6.5.0 and v6.6.0 advertise API4 and lack
  this option; v6.7.0 and v6.8.0 advertise API5. Runtime capability, not a guessed
  desktop version string, remains the gate (distributions can backport changes).

Each ScrollFrameKWin request now asynchronously reads the actual DBus `Version`
property with a one-second deadline. It does not use the global cached version,
which could survive a compositor restart. Only version >=5 proceeds, explicitly
setting `hide-caller-windows=true` and `native-resolution=true`. Failed/old-version
probes fail closed and never issue the frame request. Probe and frame are covered
by the same cancellation generation/deadline. Missing output metadata is rejected.

### Product integration decision

The first supported workflow should require API5. Do not implement a guessed
sleep fallback for API4. Preflight capability before asking for a region, then
still recheck per frame. The selector must choose one output; the small session
panel offers Capture next, Finish and Cancel. The main editor may be hidden for
physical access to the target app, but output safety relies on the compositor's
PID filter, not that local hide completing. The existing controller's preparation
confirmation API should be adapted to this explicit exclusion contract when the
real coordinator is connected; tests invoking confirmation are still not desktop
acceptance. Window restoration and unchanged editor/history remain mandatory.

Cloud validation cannot replace that integration: this environment has no KWin,
and trying an isolated local test DBus failed because Unix socket creation is
prohibited. The implementation builds and the unavailable-backend path passes,
but API5 success, renderer exclusion, denied permission, probe/frame cancellation
on a real bus and fractional-DPI output remain unverified. Next work should be
one integrated selector/panel/backend path with native acceptance, rather than
more standalone model layers or a fake demonstration.

## Integrated preview (058)

The standalone preview now connects the real request adapter to the controller.
Preparation confirmation authorizes the adapter's enforced API5 compositor exclusion
contract, not a timed local repaint assumption. The backend verifies the capability
again before each actual capture. No production-default entry is enabled; see the
preview guide for the opt-in route, verified interactions and native test gaps.

## Selector interaction refinement (059)

Drag release now leaves the region editable. A small confirmation bar shows
physical dimensions and provides Cancel/Use region; arrows move, Alt+arrows resize,
Shift accelerates, and Enter/Tab activation are wired. Outside-only dimming keeps
the selected desktop area unobscured. Offscreen Qt tests exercise the actual input
handlers and limit/focus behavior, while maintaining C++ Setup state and zero real
frames. Native compositor focus, transparency and screen-reader acceptance remain
separate gates. This does not enable the feature by default.
