# Edit an independent image copy (091)

Image pins now expose **Edit a copy…**. This includes screenshot pins, finished
scrolling compositions, sharing copies and frames extracted from completed videos.
The new window owns its own AnnotationDocument, so editing cannot replace the
main viewer's screenshot, active video or undo history. Repeatedly opening the
same pin reuses its active edit session; a new session after closing starts again
from the original immutable pin.

The canvas, annotation tool palette, options and undo/redo reuse the existing
components rather than maintaining another screenshot editor implementation.
Frequent tools stay visible, while the existing secondary-tool menu retains
highlighter, line, ellipse, blur and numbered annotations. Fit/zoom controls and
scrollbars allow larger pictures to be inspected. The hand tool in More permits
panning while other tools keep pointer input for editing.

**Copy** copies rendered edited pixels. **Done** creates a separate image pin and
closes the editing session; its existing PNG Save As, copy and OCR actions remain
available. Closing a window with edits asks before discarding, and Cancel keeps
the session. The result is a flattened image; the editing session history is not stored in
the result pin. The original pin never changes. These are session-only windows,
not a persisted photo library or crash-recovery document format; save the result
as PNG to keep it outside the app.

The editor/controller does not depend on SpectacleCore and works with an injected
image and its own document. This is a useful reuse boundary for a future photo
application, not yet a separately packaged public SDK.

## Verification boundaries

Generated media exercises a real pin entry, independent document, actual drawing,
and a painted canvas-pixel check after redo (not only an in-memory model check),
color selection, undo/redo, copy pixels, Cancel on unsaved close, cropped-model
Discard, Done/result
isolation and a clean reopened session. The main video, document/history and
original pin are checked unchanged. Source-level checks retain all twelve tool
routes, but this does not certify every gesture/tool combination in this new
window. Native KDE dialogs, input methods, multi-monitor scaling and GPU behavior
still require native desktop acceptance. No translation or external service is
involved.

## Reviewed actual software-window screenshots

- [Pin entry](screenshots/091-editor-entry.png)
- [Light editor](screenshots/091-editor-light.png)
- [Dark editor](screenshots/091-editor-dark.png)
- [420-pixel editor](screenshots/091-editor-compact.png)

The colorful image is generated non-personal video test content. The pink line
is an actual pointer-drawn edit; screenshots are not retouched design mockups.
