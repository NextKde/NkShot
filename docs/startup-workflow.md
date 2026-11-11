# Screenshot and recording setup

The launch-only window separates choosing a target from starting an operation.
Its two modes share a compact, resizable setup surface. The window starts at
480 × 440 logical pixels (bounded by the available screen), with a 360-pixel minimum width. The minimum height follows the font, fixed
controls and visible error messages, reserving usable scrolling space. Options scroll instead of changing the window size when a group expands.
The editor remains a separate workflow with its own sizing.

## Retained behavior map

| Existing behavior | Setup contract |
| --- | --- |
| Screenshot targets | Every available entry from `CaptureModeModel`, not a hand-maintained subset |
| Recording targets | Every available entry from `RecordingModeModel` |
| Target choice | Select a target, then activate the primary action; changing a target does not start capture |
| Screenshot shortcuts | Existing application/global shortcuts remain registered; model shortcut hints remain available |
| Capture delay | 0–99 seconds, including fractional seconds; stored in the existing preference |
| Capture on click | Only offered when the backend supports it; disables delay without discarding the saved delay |
| Countdown | The cancel operation remains available while switching setup modes |
| Pointer | Separate existing screenshot and recording preferences |
| Window decorations/shadow | Shadow is disabled when decorations are disabled; its preference is preserved |
| Current pop-up only | Only offered when `TransientWithParent` is supported |
| Quit after Save/Copy | Existing preference and downstream behavior retained |
| System audio/microphone | Existing preferences, disabled with an explanation when the selected format cannot carry audio |
| Unavailable recording | Recording explains the unavailable backend; no start operation is dispatched |
| Configure | Direct access and the existing Preferences shortcut |
| More | Independent screen color picker and Help remain available without capturing first |

Production uses the existing `SpectacleCore`, `Settings`, image/video platforms,
and mode models. The presentation component's test fixture can supply synthetic
models and state, but it has no production environment-variable capability override.
No screenshot or recording backend is replaced by this UI change.

## Design and evidence

The design was compared with the actual NkNotes and NkMail application windows,
and the NextKde application demo's Settings/Gallery guidance and published runtime
Gallery evidence. Those references inform ordinary content surfaces, concise type
hierarchy, compact spacing and a single primary action. They are not copied into
the repository as new NkShot screenshots, and the component gallery's card wall
is not used as an application layout.

The `075-before-*` images were captured from the unchanged 074 application binary
with `--launchonly` before editing the startup QML. They are real software-rendered
application windows, not reconstructed mockups. Dark mode uses a synthetic desktop
palette supplied by the opt-in development harness. No private desktop or media
was read. The old recording tab was unavailable in this environment; no fake
before recording image is presented.

Fixture tests demonstrate UI dispatch and state contracts, not native capture.
Real KDE screenshot selection, recording, audio devices, compositor/GPU appearance
and native screen-color picking still require desktop acceptance.

## Model and teardown contracts

Both production mode models emit standard `beginResetModel`/`endResetModel`
notifications when their rows are rebuilt, including equal-count replacements.
The selector restores a valid mode after a reset and follows externally changed
screenshot preferences. Test code obtains the actual QML-bound model instance,
not a separately constructed C++ singleton.

Normal exit also tears down the owned QML windows before their engine/models,
and destroys the Core before QApplication's GUI services disappear. This order
fixes a reproduced pure-launch-only shutdown crash exposed by the new selectors.
Runtime window replacement continues to use the original deferred-deletion path.
