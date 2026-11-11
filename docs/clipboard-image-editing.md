# Edit a clipboard image (093)

More → **Edit clipboard image…**, or **Ctrl+Shift+V** in the main window, opens
an independent editing session from the current image clipboard. It is available
from startup as well as image/video viewing. Ordinary Ctrl+V remains available to
text fields rather than being intercepted for image import.

The workflow reuses the shared image-copy editor, annotation tools, undo/redo,
PNG Save and in-app Done/pin actions. It never replaces the active capture,
recording, original document/history or clipboard. Subsequent clipboard changes
do not change an already opened editor; repeating the explicit action opens a
new independent snapshot. This is useful for images copied from another app
without first writing them to a file.

Only explicit invocation reads the clipboard. There is no clipboard watcher,
history store, URL fetching, cloud upload or translation. Plain text and file/URL
lists get guidance rather than being interpreted as images. Recording/import/
export busy gates are shared with Open. A successfully decoded image must fit
40 MP, width 8192 and height 32000 before editor construction. Qt and the clipboard
provider still perform the platform-specific image transfer/decoding; this limit
is not a promise of bounded allocation before the provider returns an image.

## Evidence and limits

The actual Qt menu and shortcut paths check pixel identity, independent repeated
sessions, changing the clipboard after import, text/URL rejection, width-limit
rejection, and original recording/document/history preservation. The second
snapshot uses alpha. Light/dark/420px software-window screenshots use a generated,
non-personal image fixture, not a mock editor or retouched screenshot.

Native KDE clipboard ownership, other applications' MIME compatibility, Wayland
permission/focus, large provider latency and GPU rendering need native acceptance.
No persistent editable project format or clipboard manager is claimed.

UI review also corrected the viewer's More popup anchor: it now opens beside its
upper-right trigger instead of appearing at the opposite corner. Actual menu
coordinates are checked at normal and narrow widths. Editor standard shortcuts
bind all platform-provided alternatives without stealing the main text-paste key.

Reviewed views: [light editor](screenshots/093-clipboard-light.png),
[dark editor](screenshots/093-clipboard-dark.png),
[narrow editor](screenshots/093-clipboard-compact.png), and
[anchored menu](screenshots/093-clipboard-menu.png). Screenshot capture waits for
multiple known foreground pixels to be visible, not just an allocated canvas.
The clipboard-to-PNG round trip is also exercised through the actual save dialog.
