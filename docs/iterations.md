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
