# Spectacle feature preservation audit (090)

This is a source/automated check, not certification that every feature has passed
on a real KDE desktop. The native release gates in `nkshot-validation.md` remain.
The capability matrix distinguishes implementation, automated evidence and native
acceptance rather than treating a count of iterations as completion.

- Capture-mode models and original region/window/screen routes remain present.
  Native KWin permission, multi-monitor/DPI, hide/capture ordering and cancellation
  still need desktop acceptance.
- Accept, Cancel, Save, Save As, Copy, OCR and Record capture actions remain wired.
  Editing after capture still uses the existing AnnotationDocument.
- Selection, crop, freehand, arrow, rectangle, ellipse, pixelate, blur, text,
  highlighter, line and numbered annotations remain represented in the shared
  toolbar. Static coverage now explicitly includes all twelve names; their
  presence is not proof of every gesture combination.
- Undo/redo and edit controls receive a document explicitly. Image sharing
  transforms use independent snapshots and never replace document history.
- Save/copy continue through the existing window actions. Import and sharing
  workflows additionally check source preservation and actual exported pixels.
- Recording targets, audio choices and platform capability gates remain.
  New processing features operate on completed files rather than claiming native
  capture has been validated. OCR remains gated on local availability.
- Separate application/settings/shortcut identities and normal KWin permissions
  remain required. No original Spectacle installation or capture security is
  replaced by this work.

## Important reference gaps

Real capture/A/V acceptance, fully automatic/horizontal scrolling, live camera
and click overlays, presentation spotlight/magnifier and AI erase/background
removal are not completed. Controlled frames, local post-processing and ordinary
annotation tools must not be labeled as these missing workflows. Offline
translation is currently outside active iteration.
