# NkShot

A macOS-inspired QML interface fork of KDE Spectacle for the KDE Plasma desktop.
This is an early compiled prototype, not a release-tested product or an Apple product.

See the [capability matrix](docs/capability-matrix.md) for the 1Capture comparison
and separate implementation, automated-test and real-KDE acceptance status.

## Current scope

- Quick Note: session-only plain-text cards with three styles, image copy and
  immutable pins, grouped under Pins and notes. Save the pinned PNG to keep it.

- Canonical NextKde Kos.Ui floating liquid panels with same-window image sampling,
  blur/refraction and opaque fallbacks (software visuals reviewed; GPU optics unverified).
- Neutral selection outline and contrasting resize handles.
- Rounded tool-button hover/pressed/selected states and visible keyboard focus.
- Bottom capture controls separated from the screen edge.
- Existing Spectacle capture, recording, annotations, exports and shortcuts retained.
- Local English/Chinese OCR results, text selection/copy and UTF-8 saving, including independent pinned images.
- Image-local color sampling with pixel loupe, sRGB/alpha values and explicit HEX/RGB copy.
- Explicit local QR/barcode reading with plain-text results; links and commands are never opened automatically.
- Separate-image pinning, conservative overlap composition and explicit vertical stacking.
- Non-destructive sharing copies: proportional resizing, backgrounds and text watermark.
- Local video/animation playback with software-frame fallback and failure/retry UI.

These changes do not add compositor blur, replace
KWin's capture implementation or claim pixel-identical macOS 27 behavior.

## Base and compatibility

Based on upstream Spectacle **v6.8.3**, commit
`5f5af4b83f5329396117af03234c012464404257`.
The upstream build requires Qt 6.11+, KDE Frameworks 6.30+, and Plasma 6.8
components; see `CMakeLists.txt` for the full dependency list.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Use a matching Plasma development environment. The first complete build, staged install, filename test and image-library tests
passed. Full capture/runtime and visual acceptance are still pending. Do not install this prototype over your working Spectacle package.
The executable is `nkshot`, the desktop/D-Bus ID is `org.nextkde.nkshot`, and
settings are stored in `nkshotrc`. Notification, icon, translations and metadata
install names are separate from Spectacle. Upstream migration scripts and manuals
are not installed. Default global shortcuts are deliberately unassigned so the
fork does not steal Spectacle's Print key; assign them in KDE System Settings.
The staged install produced only NkShot-named application artifacts. A real
side-by-side launch test is still needed before declaring coexistence verified.

Source-level checks (no Qt dependencies):

```sh
python3 tests/nkshot/test_identity.py -v
```

## Security and licensing

KWin restricts screenshot access. Packaging must preserve the appropriate capture
permission declarations and work with the target KWin version. Do not disable
KWin permission checks as a workaround for distributing this application.

Upstream copyright notices and per-file SPDX licenses are retained. See
`LICENSES/` and [upstream documentation](README.upstream.md). Changes remain under
the licenses of their respective files. The bundled Kos.Ui library is GPL-3.0;
the combined distribution is GPL-3.0, with original GPL/LGPL notices preserved. No proprietary Apple assets are included.

See [validation checklist](docs/nkshot-validation.md) for outstanding verification.

## Delivery scope

The first release targets screenshot capture and post-capture image editing.
Desktop recording still needs real KWin/PipeWire acceptance. Actual local H.264,
GIF and WebP playback and local English/Chinese OCR have passed synthetic-fixture
interaction tests; this does not certify desktop capture, audio, general OCR
accuracy or GPU presentation. Translation is not implemented. See the
[iteration log](docs/iterations.md) and [recording boundaries](docs/recording-verification.md).

See [liquid-material integration](docs/liquid-ui.md) for provenance, limits and
required visual checks.

Build requirements and the first verified results: [building](docs/building.md),
[iteration log](docs/iterations.md).

## Experimental manual scrolling capture

An explicit, default-off [technical preview](docs/scroll-preview.md) integrates a
single-display selector, manual next-frame panel and API5-only KWin backend. It
has not passed native desktop acceptance; do not treat it as a released scrolling
feature or enable it in packaging defaults. No automated scrolling is provided.
