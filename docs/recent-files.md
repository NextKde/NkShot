# Local recent files (096)

More → **Recent files…** provides quick access to the last twenty supported local
images and recordings opened or successfully saved by NkShot. It is available
from startup and the main viewer. Open an entry directly, or use arrow keys and
Enter in the list. High-frequency capture/edit controls remain on the main screen;
file recall is grouped in More instead of adding another toolbar.

Entries come from successful validated Open, normal screenshot/recording Save,
image-pin PNG Save, independent-editor PNG Save and MP4 clip export. Only PNG,
JPEG, static WebP, MP4 and WebM extensions are recorded. GIF exports are not listed
because the existing validated Open route does not import animated GIF. The normal
Open file-size/format/duration limits still apply when reopening a recent item.

## Safety and privacy

- Only local path metadata is stored, on this device. There is no background
  filesystem scan, thumbnail cache, clipboard monitoring or upload.
- Newest entries are first; canonical paths are deduplicated and capped at twenty.
- Reopening goes through the same validation, unchanged-source check, capture/busy
  gates and explicit current-content replacement dialog as Open. Cancelling keeps
  the current document/history and recent ordering unchanged.
- Missing/moved items remain identifiable but cannot be opened from the list.
  Refresh checks known entries only; it does not search for relocated files.
- Remove and Clear list modify only this list. They do not delete media or clear
  KDE's separate system recent-document integration.
- Metadata writes are atomic. Errors are visible in the panel, and failed writes
  retain the old list. Malformed/oversized metadata and non-local URLs are rejected.

This is a bounded recent-file list, not an automatically archived screenshot
library, crash recovery, a persistent editable project or a cloud history service.
Unsaved images still need an explicit save to be available after closing the app.

## Verification

Model tests cover bounded persistence, canonical aliases, duplicates, all accepted
extensions, missing-file refresh, malformed/oversized/non-local metadata, write
failures, non-regular/symlink metadata rejection and file-preserving remove/clear. Actual Qt UI checks successful save/open
hooks, reopening a generated clip, replacement Cancel, reopening an image through
the shared validation route, row removal, missing entries and clearing the list
without modifying underlying files. Light/dark/narrow windows are reviewed.
Native KDE portals, clipboard providers, GPU and real desktop A/V remain unaccepted.


Reviewed actual Qt/software-rendered windows (synthetic test media, not a macOS
mockup or native KDE desktop acceptance):

- [Light](screenshots/096-recent-light.png)
- [Dark](screenshots/096-recent-dark.png)
- [420px compact](screenshots/096-recent-compact.png)
- [Empty list](screenshots/096-recent-empty.png)

The panel reuses NextKde's Kos theme/buttons. A subtle accent selection, clear
filename hierarchy and quiet path subtitles keep reopening primary; removal is a
separate accessible control and Clear list stays in the footer. Actual screenshot
review replaced platform-default footer buttons that had poor dark contrast.
