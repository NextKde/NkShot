# Review an edited image against its input (104)

In **Edit a copy**, hold **Compare** beside the zoom controls to see the full input
image. Release to return to the edited view with the same zoom and pan. Keyboard
users can focus Compare with Tab and hold Space. Escape, focus loss, hiding the
window, cancellation and output actions end the preview.

The preview is labelled **Before editing · Full image**. It fits the entire input
without stretching it to the current crop. It is not pixel-aligned split comparison;
a cropped result may have a different aspect ratio. “Before” means the image that
entered this editing session: a pin or extracted frame may already contain edits.
It is not a recovery mechanism for an earlier capture or redacted pixels.

The input is immutable and separate from the document, undo history and saved
revision. Comparison does not apply undo, reset the base image or commit tool
options. New pointer, keyboard and IME editing input is blocked while comparing.
Copy and Save leave preview and use the **edited** image, never the preview cache.
The input preview can show content hidden by current redaction, so use comparison
only when it is safe for people looking at the screen to see that input.

This component belongs to the independent reusable image-copy editor, used by
pins, clipboard images and extracted frames. It is hidden in video-annotation
mode. It does not replace Spectacle tools or add an external service. Native KDE
focus/IME/high-DPI/GPU acceptance and every unfinished-text/crop gesture combination
remain unverified; screenshots document software-window behavior only.

## Recorded checks

Build and source checks pass (28/28). Actual light, dark and 420×520 workflows
verify hold/release, keyboard Space including auto-repeat release, drag cancellation,
focus change, hide/reopen, Ctrl+C output, blocked undo/IME commit, crop/full-input
aspect ratio, mutable caller isolation, transparency/DPR2, and preserved history
and viewport. Existing PNG save/undo/redo/close protection and independent output
flows pass. Video-annotation mode also passes its real editor/export regression.

Screenshots show actual software-rendered windows. While previewing, the zoom
label says Fit rather than incorrectly displaying the edited viewport's zoom.

Final serial full CTest passed **27/27 in 166.17 seconds**. Local checks do not
stand in for remote CI. The existing preview100 binary predates this feature.
