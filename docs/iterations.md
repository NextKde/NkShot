# NkShot verified iteration log

The requested long-running quality target is up to 200 useful iterations, not a
claim of completed work. Each code increment must compile before the next broad
change. Runtime/visual checks are recorded separately from source checks.

## 001 — Obtain a real build and fix dependency validation

Change: require KPipeWire >= 6.8 explicitly. Its 6.7.4 package configured but failed
compilation because four microphone/system-audio APIs were absent. Build the
matching official 6.8.3 library; retain the recording features unchanged.

Verified:
- Complete NkShot C++/QML build and staged installation succeeded.
- Application version command returned `nkshot 6.8.3`.
- NkShot filename CTest passed (1/1).
- Official KQuickImageEditor processing tests passed (3/3).
- Source identity/material checks passed (14/14).
- Installed application artifacts are independently named NkShot.

Not accepted yet:
- Real screenshot/recording capture, editing interaction and GPU material visuals.
- Side-by-side coexistence with a running system Spectacle.
- Xvfb could not open local sockets in the cloud runtime, including a supported
  elevated attempt. No security settings were disabled to force it.

Screenshot: none for this compile-only iteration. A standalone material fallback
image is not presented as a screenshot of the application.

Next: render the actual editor with a deterministic test image in a test-only
configuration, inspect its pixels, then make small UI/action-hierarchy changes.

## 002 — Correct runtime application identity

The first real offscreen editor launch revealed a warning that KAboutData still
advertised `org.kde.nkshot` even though the QGuiApplication ID was
`org.nextkde.nkshot`. The early QCoreApplication setting alone was overwritten by
KAboutData's defaults. Set both the organization domain (`nextkde.org`) and desktop
ID on KAboutData explicitly, and strengthen the source regression test.

Verified: incremental compile/link passed; filename CTest passed (1/1), source
checks passed (14/14), and the actual editor process stayed running in an offscreen
smoke launch without the identity-mismatch warning. The separate test-root MIME
cache was generated, removing the missing PNG MIME warning.

Not accepted yet: a live D-Bus service/coexistence test and screenshot backend.
The smoke launch was stopped by its test timeout, not by a full interactive test.
A pre-existing zoom-field context-menu signal warning remains to investigate.
