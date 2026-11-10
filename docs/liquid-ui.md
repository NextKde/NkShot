# NextKde liquid-material integration

## Reference and reuse

Inspected NkNotes' real use of `Kos.Ui` and its pinned NextKde/nextkdeAppDemo
revision `608847c9231121b888f11c4e5f84850ac95deda4`. Instead of approximating
liquid glass with a translucent rectangle, NkShot now links that same portable
module. Its unchanged QML and paired GLSL/QSB resources are under `third_party`.
The original GPL-3.0 license and a hash manifest accompany the snapshot.

## Capture composition

- One `LiquidBackdrop` samples only the existing image/annotation canvas in the
  capture window. It cannot sample its own consumers or another window.
- `LiquidPanelSurface` supplies the canonical panel blur, refraction, highlights,
  continuous corners and background-only material opacity of 0.95.
- Text and controls remain opaque. Buttons inside panels remain ordinary controls,
  not nested liquid layers; their keyboard and action bindings are retained.
- When a selection exists, the main panel becomes opaque, reserving material
  layers for the annotation and finalizer panels.
- Joined annotation-options panels retain the original independently rounded
  opaque background because the canonical surface has one corner-radius value.
- Recording, unavailable image source, software rendering or disabled liquid
  material use the shared opaque fallback. No KWin blur request or new desktop
  capture permission is introduced. The captured image is not an invented desktop
  wallpaper; it is the screenshot already obtained by Spectacle's capture path.
- General Settings exposes liquid-material and reduced-control-animation toggles.

## Scope and remaining work

This integrates the capture overlay first. The post-capture editor layout and
native settings window are retained, not falsely described as a complete macOS
27 reproduction. Apple assets are not copied. The design direction is the user's
macOS-inspired NextKde family, not a claim of pixel-identical Apple behavior.

The complete C++/QML build now passes. A standalone software-backend panel test
rendered its opaque fallback; this does not validate the app UI or GPU optics.
Runtime verification must still cover module/plugin imports, shader loading, backdrop mapping while
moving toolbars, 100–200%/mixed DPI, light/dark themes, reduced transparency,
reduced motion, joined panels and software fallback. Verify saved/copied image
pixels exclude all overlay controls and glass effects, and compare the rendered
result with the reference apps before calling the appearance complete.
