# Size and orientation (089)

**More → Sharing → Size and orientation…** extends the existing resize-copy
workflow instead of adding another main-toolbar feature. Rotate clockwise in
90-degree steps, optionally mirror horizontally in the resulting orientation,
then choose output width. Height follows the rotated aspect ratio. Rotating
resets width to the natural oriented width, capped at 8192 pixels; set your final
sharing width after choosing orientation. The degree/mirror summary and final
pixel dimensions make the result explicit.

**Create copy** opens an independent image pin for review, Copy and PNG Save As.
Existing annotations are included in the snapshot; the original image, annotations
and undo history are never replaced. Closing or canceling has no document effect.
Each new dialog starts with no rotation/mirroring. Vertical mirroring is available
by combining a 180-degree rotation with horizontal mirror.

The implementation is the UI-independent `Imaging::orientedCopy` helper, suitable
for reuse by a future photo app. Quarter-turns and mirroring are pixel-preserving
before optional proportional resampling. Alpha, color profile and DPR are retained.
Orienting bounds input and output to 40 megapixels; output also retains the
existing width<=8192 and height<=32000 limits. Plain resize retains its previous
input behavior. This is a sharing-copy transform, not a new destructive document
edit, arbitrary-angle rotation or perspective correction.

Validation uses independently calculated asymmetric pixel coordinates across all
eight orientation/mirror combinations, metadata/alpha and invalid-size checks.
Actual menu, cancel/reopen, repeated rotate/mirror clicks, output dimensions,
copy/PNG pixels and original-document history are checked in Qt software windows.
Native KDE focus, GPU materials and real desktop capture are not thereby accepted.

## Reviewed actual software windows

- [Light controls](screenshots/089-orientation-light.png)
- [Dark controls](screenshots/089-orientation-dark.png)
- [420-pixel controls](screenshots/089-orientation-compact.png)
- [Independent saved result](screenshots/089-orientation-result.png)

The image is a generated non-personal test fixture. The preview is deliberately
scaled to fit its window; PNG/clipboard assertions compare full output pixels.
