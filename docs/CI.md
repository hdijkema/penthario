# GitHub Actions and releases

The main `.github/workflows/build.yml` builds the native FLTK application in
Release mode. Windows uses MSVC, Debian uses GCC, and macOS uses Apple Clang.
Native build dependencies and the first source-release order are in
[DEPENDENCIES.md](DEPENDENCIES.md). Clean application builds require published
fltk-css `v0.1.1` and `v0.1.0` tags for fltk-cell, fltk-dock and fltk-toolbar.
A local source override can
hide a missing remote tag; it does not establish that a hosted build can fetch it.

## Jobs

- Windows x64: fetch PostgreSQL 18, build/test, stage the portable app with native
  dependencies and MSVC runtime, verify isolated startup, build an Inno Setup
  installer and exercise install/uninstall under a private test identity.
- Debian 12/13, amd64/ARM64: install native development dependencies, build/test
  under Xvfb with C.UTF-8, verify startup in three languages and create DEBs.
- Flatpak x86_64: Freedesktop 26.08 SDK/runtime, offline source dependencies,
  X11 FLTK and Secret Service, packaged PostgreSQL 18 tools. Direct flatpak-builder
  commands create and smoke-test the bundle.
- macOS Apple Silicon: Homebrew libpq@18 and dylibbundler, native arm64 Release,
  tests, dependency audit, ad-hoc signing and a mounted-DMG startup check.

Actions run the fast core, translation and XLSX tests, plus package/startup checks.
Interactive UI tests are reserved for local testing:
`PGADM_FLTK_UI_TESTS=OFF` avoids building and running them in Windows, Debian and
macOS jobs. Flatpak already disables application tests. Noninteractive tests have
a 60-second timeout; local UI tests have a 120-second timeout.

Normal tests need no database, production credentials or signing secrets.
The disposable local PostgreSQL integration runner is not a hosted CI service.
Reports are Actions artifacts, separate from distributable packages. Other
platforms must be validated by their hosted runners; local Windows success is
not proof of a successful macOS/Linux package.

If configuration fails with `invalid reference: v0.1.0` in a dependency's clone
step, check that library's remote tags with `git ls-remote --tags <repository>`.
Publish the tested library version before rerunning the application workflow.
The Flatpak manifest must also stage each widget library under `vendor/` and set
its explicit source directory: its CMake phase runs without network access.

## Publishing

Push a numeric tag such as `v0.3.5` (or `0.3.5`), or manually run Release build
with a numeric version. A manual branch run creates a v-prefixed tag; a tag run
must request the same version. Blank version on a branch builds artifacts only.
Prerelease suffixes and leading zeroes are rejected. The tagged commit must
contain the current workflows; rerunning an old tag does not pick up later fixes.

`PGADM_RELEASE_VERSION` sets the executable/About and package versions. All
platform jobs must succeed before the publication job runs with contents:write.
It checks the expected eight nonempty files from that workflow and the tag's
commit, creates a missing GitHub Release, or uploads only missing assets to an
existing release. Existing notes and assets are preserved.

Expected files for version 0.3.5:

- `pgadmin-jr-0.3.5-windows-x64.zip`
- `pgadmin-jr-0.3.5-windows-x64-setup.exe`
- `pgadmin-jr_0.3.5-debian12_amd64.deb`
- `pgadmin-jr_0.3.5-debian13_amd64.deb`
- `pgadmin-jr_0.3.5-debian12_arm64.deb`
- `pgadmin-jr_0.3.5-debian13_arm64.deb`
- `pgadmin-jr-0.3.5-linux-x86_64.flatpak`
- `pgadmin-jr-0.3.5-macos-arm64.dmg`

Local helpers test version resolution, asset completeness and packaging audits.
They do not publish releases. Application release versions are changed separately
from dependency fixes.
