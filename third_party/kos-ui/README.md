# Pinned Kos.Ui source

Unmodified `shared/qml` snapshot from NextKde/nextkdeAppDemo, revision
`608847c9231121b888f11c4e5f84850ac95deda4`, as used by the inspected NkNotes
checkout. Repository: https://github.com/NextKde/nextkdeAppDemo

`shared/Icons` comes from revision `86a9c66`, the change that introduced the
Framework7 based `Kos.Icons` module. That module did not exist yet at
`608847c`, and revision `86a9c66` is the one NkMail vendors, so keeping it at
that revision keeps the glyph set identical across applications. `SOURCE.json`
records the per-component revision split under `revisions`.

The source checkout was clean when copied. `SOURCE.json` records hashes for every
vendored source/resource. The original `LICENSE` is preserved. This is GPL-3.0
code, not an LGPL component. The combined NkShot distribution uses GPL-3.0;
Spectacle's original per-file GPL/LGPL notices remain unchanged.

`shared/Icons` carries its own upstream attribution in `provenance.json` and
`LICENSE.Framework7`: the font is Framework7 icons (MIT) plus NextKde
extensions (GPL-3.0-or-later).

Only the portable QML module is vendored, not the Quickshell demo or native
NextKde platform services. GLSL and matching prebuilt QSB resources are retained
without modification. No shaders were rebaked in this environment.
