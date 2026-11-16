# Debian and RPM packaging

Packages are produced by CI, not by hand: `.github/workflows/release.yml` builds
NkShot in a `debian:sid` container and a `fedora:rawhide` container, then attaches
`.deb`, `.rpm` and an AppImage to the GitHub release.

## Why these base images

The build needs Qt 6.8, KF6 6.13 and KPipeWire 6.4 (see
[building](../../docs/building.md)). Debian 13 "trixie" ships kpipewire 6.3.6,
which lacks `start()`/`stop()`, so a trixie build is impossible; sid ships 6.7.4.
Fedora rawhide ships kpipewire 6.8.3.

## Files

| File | Purpose |
|---|---|
| `control.in` | `dpkg-deb` control template; `@VERSION@` is substituted at build time |
| `postinst.sh` / `postrm.sh` | desktop, icon and MIME cache refresh |
| `../rpm/nkshot.spec.in` | RPM spec template for the Fedora job |

Both packages install the tree that `cmake --install` produces:

```
/usr/bin/nkshot
/usr/share/applications/org.nextkde.nkshot.desktop
/usr/share/icons/hicolor/scalable/apps/nkshot.svg
/usr/share/knotifications6/nkshot.notifyrc
/usr/share/config.kcfg/nkshot.kcfg
/usr/share/metainfo/org.nextkde.nkshot.appdata.xml
/usr/share/dbus-1/services/org.nextkde.nkshot.service
/usr/share/dbus-1/interfaces/org.nextkde.nkshot.xml
/usr/share/kglobalaccel/org.nextkde.nkshot.desktop
/usr/share/locale/*/LC_MESSAGES/nkshot.mo
/usr/lib/systemd/user/app-org.nextkde.nkshot.service
```

## Building a package locally

```sh
./install.sh check
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF -DNKSHOT_ENABLE_UI_CAPTURE=OFF \
      -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build --parallel 4
DESTDIR="$PWD/stage" cmake --install build --strip

# .deb
root="$PWD/debroot"; mkdir -p "$root/DEBIAN"
cp -a stage/usr "$root/"
sed "s/@VERSION@/0.1.0/" packaging/debian/control.in > "$root/DEBIAN/control"
install -m 0755 packaging/debian/postinst.sh "$root/DEBIAN/postinst"
install -m 0755 packaging/debian/postrm.sh "$root/DEBIAN/postrm"
dpkg-deb --root-owner-group --build "$root" nkshot_0.1.0_amd64.deb
```

## What a package build does and does not prove

A successful `dpkg-deb`/`rpmbuild` proves the tree was staged and the metadata is
well formed. It does not prove the runtime dependency names in `control.in` are
exact for every target distribution, nor that the application captures, records or
annotates correctly on a real KWin session. Both still need a manual install on the
target distribution.

`./install.sh check` runs first in every CI job: if a container is missing a
development package it prints the component and the package to install, instead of
failing somewhere inside the compiler output.
