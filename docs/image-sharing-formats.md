# JPEG and lossless WebP sharing from the copy editor (107)

The independent screenshot editor keeps **Save…** as PNG. A small adjacent arrow
opens the less frequent **Export JPEG / WebP…** action, without adding another
primary toolbar command. Video-annotation mode does not offer this image export.

- JPEG offers High (90), Highest (95) and Smaller (80) quality presets. It is lossy;
  transparent pixels are explicitly composited onto **white**.
- WebP uses lossless encoding and retains alpha. No misleading quality slider is
  shown for the lossless format.
- Continue opens a normal local Save dialog. The filename must match the selected
  format. Missing Qt encoders are reported instead of silently changing formats.

## Image and session contract

Export commits pending annotation options and snapshots the requested edited
revision before showing the file chooser. Later edits cannot silently replace
that accepted snapshot. The original image, document history and PNG save point
are unchanged. In particular, exporting a JPEG does **not** dismiss unsaved-change
protection. PNG remains a rendered-image save, not an editable project format.

The reusable `Imaging/ImageExport` helper has no SpectacleCore dependency. It
converts tagged source colors to sRGB (untagged pixels are treated as sRGB), keeps
physical pixel dimensions and writes atomically with QSaveFile; direct-write
fallback is disabled. Existing ordinary files can be replaced through the Save
dialog's normal confirmation. Invalid format/quality/extension and unsupported
encoders fail before touching the destination. Links and non-file destinations
are refused. This is not a claim of atomic protection against every same-user
filesystem race.

The input limits are 8192 pixels wide, 32000 high and 40 million pixels. Sharing
formats do not promise to retain original text metadata, DPR or monitor-profile
semantics. Lossless WebP preserves visible color/alpha, not hidden RGB values of
fully transparent pixels. Exports are local; no image or recognized text is sent
to an external service. Translation is unchanged.

## Acceptance boundary

Synthetic encode/decode and Qt software-window tests do not establish native KDE
portals, physical display color accuracy, GPU or live capture acceptance. Existing
Spectacle save/export routes and the default PNG workflow are retained.

## Reviewed UI

Real software-rendered application windows with a synthetic design-notes image:
[light JPEG](screenshots/107-jpeg-sharing-light.png),
[dark WebP](screenshots/107-webp-sharing-dark.png), and
[420px JPEG](screenshots/107-jpeg-sharing-compact.png).
The familiar Save action remains primary; the optional-format arrow and dialog
keep quality/alpha tradeoffs out of the everyday annotation toolbar.

Final verification: C++/QML build passed; full CTest 29/29 (176.29 s), static
checks 32/32, focused real-encoder QtTest 7/7 with no skips. Actual light/dark/420px
export workflows passed both from decoded-video-frame editing and standalone
image editing. Original PNG save/close, video-annotation export and
comparison/navigation/explicit-GC UI regressions passed. The first build caught
strict QString literal conversions and was corrected; the UI harness was corrected
to wait for a closed file dialog's actual deferred destruction before requesting
another editor close. These are final rerun results, not assumptions from source.
Preview105 remains the latest separately built binary and does not include 106–107.
