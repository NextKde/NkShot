# NkShot capability and acceptance matrix

This is the unified status map. “Implemented” means code exists; automated tests
use local synthetic fixtures and do not equal real KDE desktop acceptance.
Reference scope was checked against [1Capture](https://1capture.com/), its
[changelog](https://1capture.com/docs/changelog) and
[Mac App Store listing](https://apps.apple.com/us/app/screenshot-recorder-1capture/id6742318274?mt=12).
This is a workflow comparison, not a claim of source reuse or full parity.

| Capability | Implementation | Automated evidence | Real KDE acceptance |
| --- | --- | --- | --- |
| Region/window/screen capture, delay/pointer options | Spectacle routes retained | Build; synthetic selection-window interaction | Pending KWin permission/capture checks |
| Core annotation/crop/text/undo/redo | Existing document model and new shared UI | Actual gestures, English/Chinese IME, export pixel checks; not every gesture combination | Pending compositor/input/DPI matrix |
| Copy/save and immutable pins | Implemented; view-only zoom/pan for long images (061) | Pixels, PNG Save As, cancellation/reopen, multiple pins/lifetime, actual Fit/zoom/drag/wheel/keyboard and full-image copy | Native portals/stacking/high-DPI/touch pending |
| Opaque cover | Rectangle preset, not destructive source erasure | Opaque interior, unchanged selected mark/outside pixels, undo/redo and exports | Pending real desktop use |
| Local OCR | Implemented result view, installed-model choice, UTF-8 save | Real Tesseract eng/chi_sim, copy/save and error boundaries | General accuracy not certified |
| Text extraction from pins / Live Text | Local whole-image (060) and explicit rectangle OCR (069) implemented; inline word highlighting/selection not implemented | Real eng/chi_sim recognition, actual whole/area pin actions, DPR=2 pan/zoom bounds, result copy/save, close/hidden lifecycle and error states | Native focus/accessibility and full Live Text parity not accepted |
| Vertical overlapping-image composition | Implemented, manual file input | Exact matching and conservative rejection; actual picker | No desktop scrolling claim |
| Explicit vertical stacking | Implemented, manual file input | Retained pixels, numeric order, repeated content, metadata/limits | No desktop scrolling claim |
| Fixed-region manual-scroll capture session | Opt-in preview (058), keyboard-adjustable selector (059), runtime API5 gate; default off | Synthetic core/timeout/coordinate tests, unavailable-state/menu/cancel and keyboard-selection UI; no live capture acceptance | Needs real KWin session |
| Automatic vertical/horizontal scrolling capture | Not implemented | None; do not assume Wayland input injection | Needs authorized desktop feasibility work |
| Resize sharing copy | Implemented reusable helper | Actual options/preview/clipboard/PNG and source-history isolation | Native portals pending |
| Background styling and text watermark | Implemented non-destructive sharing copy (053) | See 053 tests; local render/preview/export, not source mutation | GPU/native desktop acceptance pending |
| Quick notes | Separate session-only text card editor; Paper/Blue/Night, image copy and immutable pin | 066: real QML menu, Chinese input, styles, pixel-exact copy/pin, reopen and size-limit checks; renderer tests | Software-window interaction verified; native desktop pin behavior not accepted. No persistent notes database |
| Desktop recording with system/mic audio | KPipeWire backend retained | Encoder initialization, synthetic audio pipeline with documented skips; output preflight filesystem/ordering checks (063), queued callback lifetime checks (064), actual patched-library failed initialization/Stop/destruction (065) | Real capture, A/V sync and device/permission failure pending; initialization fix requires documented dependency patch |
| Local MP4/GIF/WebP playback | Implemented, software-frame fallback; MP4 playback sound/speed controls (062) | Actual decode, playback/seek, format transitions, failure recovery, audio-track/settings UI | Audible output, A/V synchronization, GPU/performance pending |
| Capture pause/resume | Not implemented; current recording path is start/finish | Playback pause is separate and does not pause capture | Requires backend design and real A/V timing acceptance |
| Recording annotation/click effects/camera overlay | Not implemented | Pointer capture option is not a click-effect feature | Not accepted |
| Image-local color sampler | Immutable edited-image snapshot, physical-pixel loupe, sRGB/alpha values and explicit HEX/RGB copy (068) | Color/profile/DPR/ownership/edge tests and actual mouse/keyboard/copy/lifetime UI checks | Software workflow only; monitor profiles/HDR/native DPI pending |
| Dedicated screen color picker | Not implemented | Image-local sampling is not a global desktop eyedropper | Not accepted |
| Spotlight/presentation magnifier | Not implemented | Capture-selection magnifier source exists, not an independent presentation mode | Not accepted |
| QR / barcode reading | Explicit local scan of an immutable edited-image snapshot; plain-text result, copy/save, bounded size and no-result/binary guidance (067). Original automatic route retained | Real Prison QR/Code128 fixtures, Chinese/literal markup, binary/invalid bounds; actual menu and result interactions | Software-window workflow verified; real-world damaged-code accuracy and native dialogs pending |
| AI erase/background removal | Not implemented | No model/provider integration | Not accepted |
| Translation (user request) | Not implemented | Not part of the confirmed 1Capture comparison above | No default external transmission |

## Blocking environment boundary

The cloud workspace has no live Wayland/X11 session, KWin or session DBus. A
fixed-region rolling capture would need actual accepted frames and consistent
coordinates/DPI over a user-scrolled window, cancellation and deduplication.
Generating fixture frames or combining chosen files is not that feature. Input
injection must not be assumed available or permissions bypassed.

## Privacy and reuse

OCR and implemented image transforms are local. Future translation/AI providers
must be explicit about local vs external processing; no screenshots or recognized
text may silently leave the device. Original edit history is preserved, so sharing
copies must be reviewed before distribution. ImageTransforms/VerticalStitcher are
UI-independent Qt helpers; the document/annotation model remains reusable but is
not yet a separately packaged photo-editor library.
