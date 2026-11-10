# Functional preservation and verification map

Status is deliberately split between retained source routes and tested behavior.
A retained route is not an end-to-end feature acceptance.

| Capability | Current route | Verification |
| --- | --- | --- |
| Area/window/fullscreen screenshots, delay and pointer options | Capture menu and unchanged platforms/options | Compiled; real desktop capture blocked by unavailable compositor/session |
| Recording and audio | More → New Recording; original KPipeWire 6.8.3 backend | Compiled; software video initialization/resize and synthetic Opus pipeline tested with documented skips; actual session recording not run |
| Copy/save/save-as/export | Copy/Save main bar; More menu for secondary actions | Actual ViewerWindow Copy/Save pixels verified with and without edits; explicit --output input-preservation verified; native Save As dialog pending |
| Pan/crop/select/freehand/arrow/rectangle/pixelate/text | Floating dock | Routes retained; actual rectangle/pixelate viewport matrices, text keyboard/Qt IME, crop and history tested; complete tool coverage still pending |
| Highlighter/line/ellipse/blur/number | More tools, with selected icon/check state | Routes retained; basic model tests and actual blur gestures/scale matrix pass; number and complete selection gestures pending |
| Stroke/fill/strength/font/shadow/number properties | Existing contextual properties | Persistent contextual panel; actual QML stroke change/close/undo/redo verified; second-document property isolation and Qt IME commit tested; native font/color dialogs and real IME candidate panels pending |
| Undo/redo/crop | Main bar, shortcuts and original document model | Actual crop drag/cancel/accept and keyboard undo/redo pass; pixel/model tests pass for documented cases |
| Zoom/pan | Minus/plus, popup Fit/Actual/custom value, wheel/pinch/shortcuts | Controls render; original coordinate model retained; rectangle/blur/pixelate checked at 3 zooms × 4 image DPRs across 8 process configurations; pan/pinch gestures pending |
| OCR | More, only when original OCR capability is available | Initialization notification and synthetic English and one Chinese phrase recognized asynchronously; clipboard and temporary-language restoration tested, not general accuracy |
| Translation, pinned images, scrolling capture | Future staged features | Not implemented; no fake completion claim |

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
