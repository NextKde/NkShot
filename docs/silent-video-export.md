# Export a silent video copy (090)

Open **Playback and clips → Export a clip → Details**, then clear **Include audio
in exported copy**. The always-visible range summary changes to **Audio removed**,
including when Details is collapsed. Export creates a new MP4 with no audio track,
not an audio track at zero volume. Original video/audio and playback volume remain
unchanged. This is distinct from the player's **Mute playback** setting.

The choice is available only for MP4 output when the source contains audio. GIF
remains silent by definition. The option is locked during an export or destination
selection, and the destination dialog verifies that the audio choice has not
changed before accepting it. A newly opened clip session resets to including the
source audio; switching MP4/GIF inside the same session retains the explicit choice.

Verification inspects the actual exported stream list and decodes the result,
while checking original hashes. MP4 and VP9/Opus WebM fixtures cover audio removal,
re-enabling audio for another copy and rejecting mid-export setting changes.
Actual Qt menu/checkbox/save actions check that the resulting MP4 has no audio
track and that reopening restores the default. Existing timing/clip/GIF and
source-preservation tests remain in scope.

This is local file post-processing, not a recording mute switch or automatic
privacy guarantee. Speech or other information visible in the picture is not
removed. Native KDE capture, microphone/device audio and GPU acceptance remain
outstanding. No external service is used and offline translation is unchanged.

## Reviewed software-window evidence

- [Light options](screenshots/090-silent-light.png)
- [Dark options](screenshots/090-silent-dark.png)
- [420-pixel options](screenshots/090-silent-compact.png)
- [Collapsed audio-removed summary](screenshots/090-silent-summary.png)

These are actual Qt windows with generated non-personal test media, not native
KDE screenshots. Details can be collapsed to restore more preview space.
