# Local image composition (100)

Choose **More → Combine images…**, then direction and join method. One contextual
panel replaces the separate overlapping/stacking menu actions. **Choose images…**
opens the normal local file picker. Cancel preserves the choices for this window,
not a persistent project. The current screenshot and its edit history are unchanged.

- **Vertical** orders images top to bottom; **Horizontal** orders them left to right.
- **Match overlap** removes a unique, exact unchanged overlap of at least 24 pixels.
  Repeated, fully duplicated, ambiguous or unmatched content is rejected rather than
  guessed. Original PNG screenshots are preferred; recompression can defeat matching.
- **Keep all pixels** joins images edge to edge, retaining every pixel, including
  duplicates. It does not attempt matching or remove content.
- Select 2–16 files. Numeric filename ordering (1, 2, 10) is the initial order;
  the review window added in 114 lets you change it before creating a copy.
- Vertical inputs need the same width; horizontal inputs need the same height.
  Both require matching pixel scale/DPR and color profile. Alpha is retained.

After reviewing the order and composite preview, Create copy opens an independent
pin for inspection, zoom/pan, Copy, Save As or
shared image-copy editing. Original files are never overwritten by composition.
Input decoding is bounded by a total 40-million-pixel budget. Outputs are at most
40 million pixels; vertical dimensions are at most 8192×32000, horizontal at most
32000×8192. Error messages describe the selected direction and preserve the editor.

## Reuse and limits

`VerticalStitcher` now also exposes `stitchHorizontal` and `stackHorizontal` as
UI-independent QImage functions. Horizontal matching hashes packed columns, verifies
candidate pixels exactly, and uses a height-sized scratch buffer rather than
transposing all inputs. The existing vertical implementation remains unchanged.

This is file-based local composition, **not automatic horizontal or vertical
scrolling capture**. It does not scroll a website, inject pointer events, discover
scrollable areas or bypass desktop permission gates. Animated/changing areas,
sticky headers, inconsistent scaling and compressed screenshots may not match.
Native KDE dialogs, GPU display and real capture integration remain unaccepted.

## Verification

Build passed; full CTest **27/27** (148.94 seconds), static checks **25/25**.

The focused stitching suite passes **16/16**, covering retained vertical behavior,
three-frame horizontal exact pixels, transparency, profile/DPR, input isolation,
duplicates, ambiguous/no overlap, mixed pixel formats and size bounds.
Actual Qt light/dark/420×520 workflows exercise the single menu entry, configuration
cancel/reopen and repeated selected-option clicks, file-dialog reuse, reversed
selection vs numeric filename order, exact result pixels and unchanged editor.
Horizontal/vertical mismatch, unreadable file, one-file and missing-overlap warnings
are also checked. These use synthetic local inputs, not native desktop capture.

- [Light chooser](screenshots/100-combine-light.png)
- [Dark edge-to-edge chooser](screenshots/100-combine-dark.png)
- [420×520 chooser](screenshots/100-combine-compact.png)
- [Actual horizontal result pin](screenshots/100-horizontal-result.png)

Production preview100 now includes this feature; see [package checks](../packaging/debian/README.md).
No native KDE acceptance is implied. Pixel limits are not a process-memory cap:
the caller and combining operation may hold several image buffers concurrently.

## Review order and composition (114)

The file picker now opens a separate review window rather than immediately
creating the output. Real thumbnails and names show the ordered inputs. Select a
row and use Move up/Move down, or Alt+Up/Alt+Down with the list focused. Ordinary
arrow keys select a row. Remove excludes an accidental selection from this copy;
it never deletes a source file. Direction and join method remain editable beside
the actual composite preview. Create copy is available only for the latest valid
result, not an earlier image while another calculation is pending.

Choose images replaces the selected group only after accepting a new file picker;
canceling that picker preserves the order and current review. This also allows
recovery after removing too many images. At least two inputs are needed to create
a copy. Unmatched overlap or incompatible dimensions remain in the review with
a recoverable explanation: reorder, change direction/method or remove the wrong
input instead of silently guessing. Closing review leaves the main edit unchanged.

Inputs are decoded once into immutable snapshots, including their EXIF orientation,
pixel ratio, alpha and color profile. Further reordering does not reread files.
The allowed formats are PNG, JPEG, static WebP and BMP; regular local files only,
not symlinks, FIFOs, remote URLs or animations. Each file is at most 64 MiB, total
encoded input at most 256 MiB, and decoded inputs at most 40 million pixels total.
File identity/size/timestamps are checked before and after reading. Existing
composition output geometry and profile/scale compatibility limits still apply.

A shared single-worker queue bounds concurrent decode/composition work. Cancel or
a newer revision invalidates late results; an already-running bounded decoder or
matcher is not forcibly interrupted. Preview images are bounded and transient,
not disk caches. These limits are not a guarantee of constant process memory.
The source files, current screenshot and undo history are never overwritten.

### 114 verification

Full build passed; full CTest **38/38** (224.63 seconds), static checks **38/38**.
The new session suite covers immutable file snapshots, numeric order, alpha/profile,
reorder/direction, latest-result wins, incompatible-edge recovery, bounds and
nonregular files, shared queue/lifetime/reentrant observers, supported static
formats, animation containers and strict local URLs.

Actual Qt light, dark horizontal edge-to-edge, and 420-pixel-wide workflows verify
continuous keyboard reordering, selected-row controls, options, cancel/reopen,
removing to one image, canceling/accepting a replacement picker, exact result
pixels, untouched source hashes/main edit/history/clipboard, and independent
result creation. Error cases cover duplicated/absent overlap, cross-edge
mismatch, unreadable files and selection counts. Inputs are synthetic fixtures.
The file-picker test waits for directory loading and verifies the actual selected
paths before accepting; it does not inject a prebuilt result into the review.

- [Light review](screenshots/114-combine-light.png)
- [Dark horizontal review](screenshots/114-combine-dark.png)
- [Compact controls](screenshots/114-combine-compact.png)
- [Compact scrolled preview](screenshots/114-combine-compact-preview.png)

These are software-rendered Qt windows. Native KDE dialogs, real screen/recording
capture and GPU acceptance are still outstanding. This is not automatic scrolling
capture and does not implement paused offline translation.
