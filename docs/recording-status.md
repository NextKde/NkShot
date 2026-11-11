# Recording status (087)

During recording, a focused Kos.Ui status card replaces unrelated editor actions.
Elapsed time and the primary **Finish recording** action stay visible. Pause or
Resume uses the existing acknowledged A/V pause contract; while an acknowledgement
is pending its control is disabled but Finish remains available. Unsupported pause
is omitted rather than presented as a broken action.

During encoder finalization, the card says **Finishing recording** and hides both
capture actions. This is not a saved-success claim. Existing platform completion
and error handling still decide the result; no capture/backend semantics changed.
The previous media preview and playback input are hidden while capture is active.

## Verification and limits

The independent build and focused recording tests pass. Actual Qt Quick controls
were exercised with a controlled platform and a private test status-notifier host:
pause, pending acknowledgement, resume, Stop while paused, finalization feedback,
late acknowledgement rejection, and retired-action lifetime. Light, dark and
420-pixel compact software-rendered windows were reviewed for clipping, action
hierarchy and legibility. Existing WebM Open/playback/copy/MP4/GIF export UI also
passed against synthetic media.

Screenshots are real offscreen Qt windows, not design mockups or desktop captures.
They contain no personal desktop content. Native KDE/KWin recording, device audio,
GPU material rendering, and native shell placement remain unverified. This change
is a focused recording workflow improvement, not completion of all 1Capture
features. Offline translation remains outside this stage.

- [Light paused state](screenshots/087-recording-light.png)
- [Dark paused state](screenshots/087-recording-dark.png)
- [Compact paused state](screenshots/087-recording-compact.png)
- [Finishing state](screenshots/087-recording-finishing.png)
