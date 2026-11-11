# Navigate screenshot details while editing (105)

The reusable image-copy editor now navigates long screenshots without abandoning
the selected annotation tool:

- **Ctrl + wheel** zooms around the pointer.
- **Middle-button drag** temporarily pans. Releasing, cancelling or losing window
  focus ends the drag; it does not draw or switch tools.
- Wheel scrolls; **Shift + wheel** scrolls horizontally.
- **Fit / Ctrl+0** fits the whole current canvas, including a crop.
- Click the zoom percentage or press **Ctrl+1** to reset to 100% document scale.
- **+ / −** and standard zoom shortcuts zoom around the viewport centre.

100% means the document's logical scale, consistent with the main screenshot
editor. Screenshot DPR metadata is respected; this is not a promise of one source
pixel per physical monitor pixel. Zoom is bounded by Fit and 4×. Content remains
within scroll bounds after crop, resizing and zooming. Compare temporarily disables
navigation; returning preserves the editing view, subject to bounds if resized.

Keyboard zoom shortcuts do not take over focused text/IME controls. No global
Space shortcut is introduced, preserving text entry and hold-to-compare. Left-button
annotation, undo history, selected tools, image pixels and file output are unchanged.
The navigation component receives its viewport and image item instead of depending
on SpectacleCore, so another photo editor can reuse it. The existing main screenshot
viewport and original Spectacle capabilities are retained, not replaced.

## Verification limits

Actual Qt software-window events cover pointer anchors, repeated wheel bursts,
logical DPR2/crop geometry, panning, Fit/100%, text-input focus guards, cancellation,
resize bounds and the existing comparison workflow. This does not certify native
KDE/Wayland pointer grabs, real trackpad/touch devices or physical-monitor scaling.
The preview100 binary predates this source feature. Offline translation is untouched.

## Window lifetime correction

During the navigation regression, explicit QML garbage collection reproduced a
pre-existing bug: a visible extracted-frame pin could be deleted because an
unparented window returned from a QML invokable had JavaScript ownership. This
was not a native KDE capture failure or an image-file corruption.

Pins, image-copy editors, Quick Notes, Color Picker and Scroll Preview now declare
C++ ownership before QML exposure. Their existing close/deleteLater and draft-hide
policies still control lifetime. Tests explicitly release JS return wrappers,
collect garbage and drain deferred deletes, then check visible windows, draft
preservation/reopen and normal close destruction. No OCR/translation window was
changed. The earlier preview100 binary does not contain this correction.

## UI review

Reviewed actual Qt software-window captures in light, dark and 420-pixel compact
layouts. Save remains the primary action; navigation is one quiet group below
the canvas, and secondary annotation tools remain aggregated. Fit shows the whole
long screenshot and 100% lets the same document be read/panned without changing
the tool. Narrow layouts keep navigation, undo, Copy, Save and Done reachable.
These are rendered application windows with synthetic image fixtures, not design
mockups or native macOS/KDE screenshots.

- [Light Fit](screenshots/105-editor-fit-light.png)
- [Dark detail navigation](screenshots/105-editor-details-dark.png)
- [Compact detail navigation](screenshots/105-editor-details-compact.png)

Final verification: incremental C++/QML build passed; CTest 27/27 (166.99 s);
identity/static checks 30/30; light/dark/420px navigation + comparison + explicit-GC
local-media workflows passed; video-annotation/export UI regression passed.
The earlier intermittent window deletion was reproduced deterministically with
GC before the fix and the final lifetime checks passed after C++ ownership was
set. Isolated-session warnings about absent kglobalaccel and unsupported offscreen
window features remain expected; they do not establish native KDE functionality.
