#!/usr/bin/env bash
# Build trees must use prefix /opt/nkshot and the pinned, patched dependencies.
set -euo pipefail
if [[ $# != 5 ]]; then
 echo 'Usage: package.sh APP_BUILD KPIPEWIRE_BUILD KQUICKIMAGEEDITOR_BUILD OUTPUT DEBIAN_VERSION' >&2
 echo 'A source-appropriate explicit version is required; no default release is assumed.' >&2
 exit 2
fi
version=$5
# Deliberately exclude epochs and path/control characters from artifact names.
if [[ ! $version =~ ^[0-9][0-9A-Za-z.+~\-]*$ ]] || ! dpkg --validate-version "$version"; then
 echo 'Invalid Debian version: use a digit-leading, epoch-free version (for example 0.1.0~preview080).' >&2
 exit 2
fi
app=$1; pipe=$2; editor=$3; out=$(realpath -m "$4")
[[ $(dpkg --print-architecture) == amd64 ]] || { echo 'Only Debian sid amd64 is qualified' >&2; exit 1; }
grep -q '^NKSHOT_ENABLE_UI_CAPTURE:BOOL=OFF$' "$app/CMakeCache.txt" || { echo 'Refusing a UI harness build' >&2; exit 1; }
grep -q '^BUILD_TESTING:BOOL=OFF$' "$app/CMakeCache.txt" || { echo 'Use a production build with tests disabled' >&2; exit 1; }
mkdir -p "$out"
stage=$(mktemp -d "$out/stage.XXXXXX")
trap 'rm -rf "$stage"' EXIT
for build in "$pipe" "$editor" "$app"; do DESTDIR="$stage" cmake --install "$build"; done
prefix="$stage/opt/nkshot"
# Keep runtime payload only. Never copy a development prefix or translation venv.
rm -rf "$prefix/include" "$prefix/lib/x86_64-linux-gnu/cmake" "$prefix/share/doc/HTML"
find "$prefix" -name '*.a' -delete
mkdir -p "$stage/usr/bin" "$stage/usr/share" "$stage/usr/lib/systemd/user" "$stage/DEBIAN"
# Register only NkShot's own desktop/service IDs. No Spectacle shortcuts are changed.
for path in applications metainfo dbus-1 kglobalaccel icons knotifications6; do
 if [[ -d "$prefix/share/$path" ]]; then mv "$prefix/share/$path" "$stage/usr/share/"; fi
done
if [[ -d "$prefix/lib/systemd/user" ]]; then cp -a "$prefix/lib/systemd/user/." "$stage/usr/lib/systemd/user/"; rm -rf "$prefix/lib/systemd"; fi
mv "$prefix/bin/nkshot" "$prefix/bin/nkshot.bin"
cat > "$prefix/bin/nkshot" <<'EOF'
#!/bin/sh
export LD_LIBRARY_PATH=/opt/nkshot/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export QML_IMPORT_PATH=/opt/nkshot/lib/x86_64-linux-gnu/qml${QML_IMPORT_PATH:+:$QML_IMPORT_PATH}
export QML2_IMPORT_PATH=/opt/nkshot/lib/x86_64-linux-gnu/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}
export XDG_DATA_DIRS=/opt/nkshot/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}
exec /opt/nkshot/bin/nkshot.bin "$@"
EOF
chmod 755 "$prefix/bin/nkshot"
ln -s /opt/nkshot/bin/nkshot "$stage/usr/bin/nkshot"
if [[ -L "$stage/usr/share/kglobalaccel/org.nextkde.nkshot.desktop" ]]; then
 ln -sfn ../applications/org.nextkde.nkshot.desktop "$stage/usr/share/kglobalaccel/org.nextkde.nkshot.desktop"
fi
# Exact lower bounds are conservative for this preview, never guessed SONAME deps.
python3 "$(dirname "$0")/runtime-deps.py" "$stage" > "$out/runtime-depends.txt"
deps=$(cat "$out/runtime-depends.txt")
cat > "$stage/DEBIAN/control" <<EOF
Package: nkshot
Version: $version
Architecture: amd64
Maintainer: NextKde developers <noreply@github.com>
Section: graphics
Priority: optional
Depends: $deps
Recommends: tesseract-ocr-eng, tesseract-ocr-chi-sim, breeze-icon-theme
Description: NextKde screenshot and recording preview
 Private patched KPipeWire and KQuickImageEditor, with a separate application ID.
 Experimental build for Debian sid and a recent KDE Plasma Wayland session.
EOF
mkdir -p "$stage/usr/share/doc/nkshot"
cp -a "$(dirname "$0")/../../LICENSES" "$stage/usr/share/doc/nkshot/"
for src in "$pipe" "$editor"; do
 source_dir=$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$src/CMakeCache.txt")
 cp -a "$source_dir/LICENSES" "$stage/usr/share/doc/nkshot/$(basename "$source_dir")-LICENSES"
done
# Remove debug symbols from the deliverable, not from development build trees.
while IFS= read -r -d '' file; do
 if [[ $(od -An -tx1 -N4 "$file" | tr -d ' \n') == 7f454c46 ]]; then strip --strip-unneeded "$file"; fi
done < <(find "$stage" -type f -print0)
find "$stage" -type f -exec chmod go-w {} +
dpkg-deb --root-owner-group --build "$stage" "$out/nkshot_${version}_amd64.deb"
(cd "$out" && sha256sum "nkshot_${version}_amd64.deb") > "$out/SHA256SUMS"
