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
- Select 2–16 files. Numeric filename ordering is used (1, 2, 10), not click order.
- Vertical inputs need the same width; horizontal inputs need the same height.
  Both require matching pixel scale/DPR and color profile. Alpha is retained.

The result opens in an independent pin for inspection, zoom/pan, Copy, Save As or
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
