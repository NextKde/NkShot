# Pinned KQuickImageEditor source

Unmodified `src/` snapshot of KQuickImageEditor **v0.7.0**, taken from
<https://invent.kde.org/libraries/kquickimageeditor/>. `SOURCE.json` records a
SHA-256 for every vendored file.

## Why it is vendored

NkShot's annotation stack needs the 0.7 API surface (`AnnotationDocument`,
`AnnotationTool`, `AnnotationViewport`). Distributions that shipped Plasma 6.5
provide at most KQuickImageEditor 0.5.x — 0.6.0 was released 2025-11-12 and
0.7.0 only on 2026-08-31. Depending on the system copy would therefore tie
NkShot to systems newer than the Plasma release it targets.

## What is vendored

| Kept | Dropped |
|---|---|
| `src/` (60 files, ~11.2k lines) | `koko-animation.mp4` (4.4 MB demo video, 96% of the upstream tarball) |
| `CMakeLists.txt` | `autotests/`, `tests/`, `examples/` |
| `LICENSES/` | `metainfo.yaml`, `.gitlab-ci.yml`, packaging |
| `KQuickImageEditorConfig.cmake.in` | |

## Build deltas

The sources under `src/annotations/`, `src/widgets/` etc. are byte-for-byte
unmodified. Five build-system deltas are needed to consume it as a subtree
instead of an installed package; all are recorded in `SOURCE.json`:

| # | Location | Delta | Why |
|---|---|---|---|
| 1 | `CMakeLists.txt` | `set(BUILD_SHARED_LIBS OFF)` | Link the module into the NkShot executable so it is registered by `qt_import_qml_plugins()`. A shared module would need a runtime QML import path that NkShot does not provide. |
| 2 | `CMakeLists.txt` | `add_subdirectory(autotests)` disabled | Not vendored. |
| 3 | `CMakeLists.txt` | `install(EXPORT KQuickImageEditorTargets ...)` and the Config files dropped | A static QML module creates resource object targets that cannot be placed in an export set, and nothing inside NkShot consumes that package. |
| 4 | `CMakeLists.txt` | `kde_clang_format()` / `kde_configure_git_pre_commit_hook()` removed | The former collides with NkShot's own `clang-format` target; the latter would install a git pre-commit hook into the *enclosing* repository, which a vendored subtree must never do. |
| 5 | `src/CMakeLists.txt` | `INSTALLED_PLUGIN_TARGET` → `PLUGIN_TARGET` | ECM does not implement `INSTALLED_PLUGIN_TARGET`; Qt therefore named the plugin `KQuickImageEditorplugin` while `qt_import_qml_plugins()` looked for `KQuickImageEditorPlugin` and silently skipped linking it. |

With these in place `cmake` configures and generates with no errors and no
plugin-link warning. Compiling and running is still to be confirmed.


## Dependencies

KQuickImageEditor 0.7 requires C++23, Qt 6.8, KF6 6.13 and **Highway >= 1.3**
(`libhwy`). Highway was already an implicit dependency through the system
KQuickImageEditor, so vendoring does not add a new one to NkShot.

## Licensing

The vendored sources are BSD-2-Clause, with individual files under
LGPL-2.1-only, LGPL-3.0-only or LicenseRef-KDE-Accepted-LGPL as stated in their
own SPDX headers; CC0-1.0 covers data files. The original license texts are kept
verbatim in `LICENSES/`.
