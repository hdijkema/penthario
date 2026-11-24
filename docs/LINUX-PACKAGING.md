# Linux packaging

The native Linux build uses FLTK/X11, libpq and libsecret. It has no Qt or KDE
runtime dependency. A running Secret Service is needed only for remembering
passwords. The existing application ID `io.github.hdijkema.AltPgAdmin` is retained
for desktop/Flatpak continuity despite the display name pgadmin-jr.

## Debian

`tools/build-debian.sh` targets Debian 12 and 13, with amd64 and ARM64 hosted
jobs. ARM64 supports a Raspberry Pi running a matching 64-bit Debian environment;
these are not 32-bit armhf packages. The workflow installs build dependencies,
uses C.UTF-8 throughout, runs GUI tests/startup checks under Xvfb, and invokes
CPack for the Runtime component. Packages install the executable, desktop file,
icons and license notices under standard system locations.

Install with `sudo apt install ./pgadmin-jr_*.deb`. PostgreSQL client programs
are distro dependencies; for a PostgreSQL 18 server, install matching-major
(or newer) pg_dump and companion tools on PATH. An older pg_dump refuses to
back up a newer server. No database service is installed or started by pgadmin-jr.

## Flatpak

`packaging/flatpak/io.github.hdijkema.AltPgAdmin.json` uses the Freedesktop
26.08 SDK/runtime, released FLTK/css/cell sources, libsecret and PostgreSQL 18
client tools. FetchContent sources are supplied offline. The workflow registers
Flathub, installs the SDK/runtime, runs flatpak-builder directly and creates a
single-file bundle with flatpak build-bundle.

```sh
flatpak install --user ./pgadmin-jr-*-linux-x86_64.flatpak
flatpak run io.github.hdijkema.AltPgAdmin
```

The sandbox allows X11, network access, host files for SQL/backups and Secret
Service access. It does not require a KDE runtime. Builder and installed-sandbox
smoke checks need hosted Linux validation after source tags are published.
