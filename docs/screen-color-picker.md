# Screen color picker (074)

**More → Pick a color…** opens the color tool from either the launch-only start
page or an open viewer. The start page needs no screenshot or file first; its
More menu reuses the former Help button position, with Help still inside.
Choose **Screenshot** to
sample the immutable edited-image snapshot, or **Screen** for the desktop portal.
Screen works without an image and in the video view. Changing the source does not
start a desktop request. Click **Pick** explicitly.

The Screen view shows no captured desktop image, pixel coordinate or artificial
magnifier. The operating system owns the native selection cursor. After a
successful response, NkShot shows a swatch and HEX/RGB values; only explicit Copy
or the Copy shortcut changes the clipboard. Screen colors are opaque sRGB,
quantized to 8-bit values for HEX/RGB display. This is not HDR/display-profile
certification.

## Cancellation is deliberately honest

While choosing, use **Esc in the desktop picker** to cancel. NkShot's **Stop
waiting**, window hide/minimize/close, source change and timeout invalidate the
pending result and send a best-effort portal Request.Close. They do not promise
that the compositor's picker or cursor has closed. KDE's portal backend delegates
to KWin's ColorPicker; its native lifecycle does not provide that guarantee.
On systems where the cursor remains active, use Esc or right-click there.

A cancellation or error does not reuse an old result as a newly chosen color.
Retry is always explicit. Late responses, changed portal ownership and responses
for other requests cannot change the current selection. No request automatically
copies anything or changes screenshot pixels, annotation history or the current
video.

## Protocol and parent windows

NkShot calls the standard `org.freedesktop.portal.Screenshot.PickColor` method,
with a random legal request token and response subscription established before
the method call. It binds work to the portal's unique service owner and validates
the request handle. Only a successful, strictly typed finite `(ddd)` color in
[0,1] is accepted. Cancellation and errors are distinct outcomes; version numbers
or available screenshot targets are not treated as proof of color-picking support.

X11 passes its native window identifier. Wayland exports the toplevel
asynchronously with KWindowSystem, releases it after use and ignores stale export
completion. If a usable handle cannot be obtained promptly, the protocol permits
an empty parent identifier. No desktop screenshot or input injection is used.

## Validation limits

Automated tests use a dedicated private message bus and a test-only portal
process, with real QtDBus serialization rather than direct calls to a result slot.
The test fake is not compiled into the production app and no production
environment-variable injection path exists. UI screenshots show simulated portal
results, **not a live native desktop selection**.

Real KDE/OpenGL color selection, native cancellation/focus restoration, multi-
monitor coordinates, mixed DPI, HDR and color-profile accuracy remain unverified.
KWin's current ColorPicker requires OpenGL; backend failures may also appear as
cancellation. An offscreen software-window test cannot certify any of those cases.

Protocol references:
- [Screenshot portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Screenshot.html)
- [Request lifecycle](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Request.html)
- [Window identifiers](https://flatpak.github.io/xdg-desktop-portal/docs/window-identifiers.html)
