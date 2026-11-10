# Building NkShot

## Verified dependency versions

The first complete development build used:

- GCC 16.2, CMake 4.4.5 and Ninja.
- Qt 6.11.2, including Base/Declarative private headers, Wayland, Multimedia,
  QuickDialogs2 and ShaderTools, from official Debian sid packages.
- KDE Frameworks / Extra CMake Modules 6.30.0, plus the development packages listed
  by the top-level CMake file and their normal dependencies.
- LayerShellQt 6.7.4, Wayland / Plasma Wayland protocols, XCB, Tesseract 5.5.4.
- KQuickImageEditor v0.7.0.1 from KDE, commit
  `ac7568044defc89c494b0d6edb60073cb0ad8b25`, built with Highway 1.3.0.
- KPipeWire v6.8.3 from KDE, commit
  `f30b39c53092673d136660c8deaf24d3935723eb`, built with official FFmpeg/Mesa
  development dependencies.

Sources: https://deb.debian.org/debian,
https://github.com/KDE/kquickimageeditor and https://github.com/KDE/kpipewire.
The source libraries were installed in a separate development prefix. No host
system upgrade or screenshot-permission bypass was used.

**KPipeWire 6.7 is not sufficient.** Spectacle 6.8 uses its new microphone and
system-audio API. CMake now rejects an older version instead of letting configure
succeed and failing much later in compilation. Do not remove audio functionality
to work around this requirement.

## Build sequence

In an environment with the matching development packages installed, first build
KQuickImageEditor and KPipeWire into your chosen `PREFIX`. Before building the
pinned KQuickImageEditor v0.7.0.1, apply the reviewed one-line repaint fix:

```sh
git -C "$KQUICKIMAGEEDITOR_SOURCE" apply "$NKSHOT_SOURCE/third_party/patches/kquickimageeditor-shadow-repaint.patch"
```

This fixes a reproduced incremental-shadow boundary difference. The patch targets
commit `ac7568044defc89c494b0d6edb60073cb0ad8b25`; do not blindly reapply it to a
newer upstream version. `shadow_repaint_test` detects whether the linked library
still exhibits the issue. Then build NkShot:

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DBUILD_TESTING=ON
cmake --build build --parallel 4
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
cmake --install build
python3 tests/nkshot/test_identity.py -v
```

Set `QML2_IMPORT_PATH` to the prefix's installed QML directory when launching if
it is not a standard system prefix. For this build that was `lib/x86_64-linux-gnu/qml`.
Also include the prefix's `share` directory in `XDG_DATA_DIRS`, and ensure its
libraries are resolvable by the runtime linker. Do not replace the system
Spectacle installation while testing.

## Validation boundary

Building, installing and passing CTest do not prove screenshot permissions,
recording output, editing interactions or visual quality. Those require a real
KWin session and the checklist in `nkshot-validation.md`. The cloud build was
isolated from the host; local display sockets are currently unavailable there.

OCR regression testing requires Tesseract English data (`tesseract-ocr-eng`) and
DejaVu Sans fonts. The test creates a synthetic English image and uses a temporary
configuration directory; it does not submit images to a network service.

## Production configuration

Use `-DBUILD_TESTING=OFF -DNKSHOT_ENABLE_UI_CAPTURE=OFF` for a production build.
Qt Test is needed only for tests or the explicit developer capture hook. A fresh
production build has also been verified with
`-DCMAKE_DISABLE_FIND_PACKAGE_Qt6Test=TRUE`; do not enable the capture hook in
distributed builds.
