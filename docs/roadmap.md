# NkShot delivery plan

## 1. Screenshot and image-editing MVP (current scope)

Retain Spectacle's existing capture and KQuickImageEditor pipeline. The first
release should let a user capture a region, window or screen, enter Edit, crop,
add arrows/shapes/text, obscure a region, undo/redo, then save or copy the result.
The visual changes are presentation, not a rewrite of image processing.

### Release gates

1. Configure/build with Qt >= 6.11, KF >= 6.30 and matching Plasma 6.8 libraries.
2. Run upstream CTest and generated-import QML lint, plus our static identity tests.
3. Stage installation into an empty directory and compare its manifest against
   the installed Spectacle package: no overlapping files outside shared dirs.
4. In KWin Wayland, launch both applications and verify separate D-Bus services,
   configurations, desktop entries and notifications, with normal permissions.
5. Capture a known test image; add text/arrow, crop, undo, redo, save PNG and copy.
   Reopen both outputs and check dimensions, annotation appearance and clipboard.
6. Cancel capture/edit, repeat capture, then test dark/light themes, keyboard-only
   navigation, mixed-DPI multi-screen selection and narrow viewport layout.

Static checks are useful but **do not satisfy these runtime gates**. No release
binary has been built or tested in the current preparation environment.

## 2. Recording

The existing KPipeWire/Wayland recording backend and controls remain in source.
After the screenshot MVP is verified, validate region/screen/window recording,
audio availability, stop/save, interruption handling and repeated sessions.
Confirm exported video playback and that floating UI is not included in output.
Do not imply a video editor exists: this phase is recording, not video editing.

## 3. OCR and translation

The selected upstream revision already contains a Tesseract OCR integration.
It is retained but **not validated or delivered as a working NkShot feature**.
Validate missing-language-data behavior, Simplified Chinese/English recognition,
long images, timeouts, cancellation and repeated requests before advertising it.
Use local OCR by default and show recognized text for correction/copying.

Translation is not implemented. Prefer a local provider where practical. If an
online provider is added, require explicit opt-in identifying the provider and
text being sent; never upload screenshots or recognized private text silently.
Keep credentials outside the repository. Translation errors must not lose the
recognized text or interrupt basic capture/edit/save.
