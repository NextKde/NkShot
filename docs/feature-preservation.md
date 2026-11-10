# Functional preservation and verification map

Status is deliberately split between retained source routes and tested behavior.
A retained route is not an end-to-end feature acceptance.

| Capability | Current route | Verification |
| --- | --- | --- |
| Area/window/fullscreen screenshots, delay and pointer options | Capture menu and unchanged platforms/options | Compiled; real desktop capture blocked by unavailable compositor/session |
| Recording and audio | More → New Recording; original KPipeWire 6.8.3 backend | Compiled; platform capability gate retained; recording not run |
| Copy/save/save-as/export | Copy/Save main bar; More menu for secondary actions | Actual ViewerWindow Copy/Save pixels verified after edits; explicit --output input-preservation verified; native Save As dialog pending |
| Pan/crop/select/freehand/arrow/rectangle/pixelate/text | Floating dock | All routes retained; model and viewport regression tests partly cover editing |
| Highlighter/line/ellipse/blur/number | More tools, with selected icon/check state | All routes retained; menu rendered; complete per-tool gesture matrix pending |
| Stroke/fill/strength/font/shadow/number properties | Existing contextual properties | Persistent contextual panel; actual QML stroke change/close/undo/redo verified; remaining property-dialog/IME flows pending |
| Undo/redo/crop | Main bar, shortcuts and original document model | Pixel/model and actual viewport regression tests pass for documented cases |
| Zoom/pan | Minus/plus, popup Fit/Actual/custom value, wheel/pinch/shortcuts | Controls render; original coordinate model retained; extended DPR tests ongoing |
| OCR | More, only when original OCR capability is available | Initialization notification and synthetic English asynchronous recognition/clipboard tested; no general or Chinese accuracy claim |
| Translation, pinned images, scrolling capture | Future staged features | Not implemented; no fake completion claim |

## Reuse boundary

AnnotationDocument remains the authoritative image/annotation/edit-history model.
PortableAnnotationViewport reuses that model and upstream input behavior, with a
software-only rendering adapter. Screenshot capture remains outside this layer.
Further work must inject document/export dependencies into the QML controls before
claiming a standalone photo-editor package. A future photo app can reuse the model
and viewport now, but the complete editor UI is not yet a decoupled library.

## Product review gates

- Prefer simple, discoverable actions; avoid asking the user to operate setup steps.
- Keep high-frequency actions visible and secondary actions explicitly reachable.
- Match the calm shared NextKde surfaces, not decorative glass on every control.
- Compile each incremental change; save actual unmodified runtime captures.
- Review narrow, dark, text-input, interrupted-popup and keyboard flows before acceptance.
- Do not trade away KDE features for a cleaner screenshot.
