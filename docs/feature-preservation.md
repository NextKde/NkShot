# Functional preservation and verification map

Status is deliberately split between retained source routes and tested behavior.
A retained route is not an end-to-end feature acceptance.

The [unified capability matrix](capability-matrix.md) is the current feature and
acceptance map, including the verified 1Capture reference scope. This avoids an
older table falsely calling shipped local OCR or composition “untested/no menu.”

Preservation responsibilities remain:

- Keep original area/window/screen, delay, pointer and recording/audio routes.
- Keep all annotation tools, contextual options, history and exports reachable.
- Test source immutability and exact export pixels when adding sharing transforms.
- Distinguish whole-image editor/pin OCR from inline Live Text, manual file composition from rolling
  capture, and static image annotations from annotations during recording.
- QR decoding remains inherited source without a focused acceptance test.
- Native dialogs, global shortcuts, real KWin capture and hardware presentation
  require their own acceptance; local offscreen tests cannot stand in for them.

## Reuse boundary

AnnotationDocument remains the authoritative image/annotation/edit-history model.
PortableAnnotationViewport reuses that model and upstream input behavior, with a
software-only rendering adapter. Screenshot capture remains outside this layer.
Canvas, tool palette, property controls and undo/redo now take an explicit document;
actual second-document tool/history/property isolation is tested. Window helpers,
theme, localization and export integration still need a packaging boundary before
claiming a standalone photo-editor library.

## Product review gates

- Prefer simple, discoverable actions; avoid asking the user to operate setup steps.
- Keep high-frequency actions visible and secondary actions explicitly reachable.
- Match the calm shared NextKde surfaces, not decorative glass on every control.
- Compile each incremental change; save actual unmodified runtime captures.
- Review narrow, dark, text-input, interrupted-popup and keyboard flows before acceptance.
- Do not trade away KDE features for a cleaner screenshot.
