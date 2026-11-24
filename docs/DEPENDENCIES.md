# Dependencies and releases

| Component | Selected release | Purpose |
| --- | --- | --- |
| FLTK | 1.4.5 (`release-1.4.5`) | Native widgets, drawing, SVG |
| fltk-css | 0.1.0 (`v0.1.0`) | Styles |
| fltk-cell | 0.1.0 (`v0.1.0`) | Typed cell drawers/editors |
| fltk-dock | 0.1.0 (`v0.1.0`) | Dockable tool windows and rectangular tabs |
| nlohmann/json | 3.12.0 | Workspace and metadata |
| hrantzsch/keychain | 1.3.1 | Windows Credential Manager, macOS Keychain, Linux Secret Service |
| libxlsxwriter | 1.2.4 | XLSX output |
| zlib | 1.3.2 | XLSX compression |
| PostgreSQL | libpq >=15; packaged tools 18.6 | Protocol and administration tools |
| libsecret | 0.21.7 (Flatpak) | Linux Secret Service integration |
| Inno Setup | 7.1.1 | Windows installer compiler |

fltk-trayicon is a separate library at version 0.1.0, also targeting FLTK 1.4.5.
It is not a dependency of pgadmin-jr, which has no tray workflow.

Dependencies use explicit release tags or checksummed release archives rather
than moving branches or arbitrary development commits. An installed FLTK package
must satisfy the minimum supported release. Upgrades are deliberate, tested
changes; builds do not silently choose whatever happens to be newest that day.

## Published library tags

The fltk-css, fltk-cell, fltk-dock and fltk-trayicon `v0.1.0` tags have been
published. The fltk-dock tag identifies commit
`8ca3e3456ebb7821e7cdb77b0e1599d6566eec61`. Application builds can fetch all
three widget dependencies by release tag; fltk-trayicon remains independent.
Publish and validate library tags before selecting them in application builds.

## Remote verification: 24 November 2026

The ten selected source tags (including independent fltk-trayicon and Flatpak's
libsecret) were checked against their GitHub remotes. All are available.
Fresh downloads of zlib, libxlsxwriter, PostgreSQL source and the Inno Setup
compiler match the committed SHA-256 values. The PostgreSQL Windows SDK URL is
also available. The referenced GitHub Action tags and the pinned pull-request
action commit are accessible. This verifies source availability and archive
integrity; it does not replace builds on each target platform.

A clean Windows Release build fetched fltk-cell, fltk-css, fltk-dock, JSON and
keychain from their published tags and passed all four application tests. It used
the installed Release FLTK 1.4.5 SDK and PostgreSQL 18.6 SDK, with no local widget
library source overrides. The thirteen release-helper tests also pass, including
offline Flatpak staging of all three widget libraries. Other platform builds
remain subject to their hosted runners.

## Local development

Local development uses explicit `PGADM_FLTK_CELL_SOURCE_DIR`,
`FLTK_CELL_CSS_SOURCE_DIR` and `PGADM_FLTK_DOCK_SOURCE_DIR` paths. Empty paths mean installed packages or tagged
release downloads. Invalid nonempty overrides fail instead of silently falling
back. `build-fltk.cmd` selects the sibling checkouts explicitly.

Linux requires X11/Xft/OpenGL development packages as selected by FLTK, libpq,
libsecret, fonts and a session Secret Service to remember passwords. Database
access itself does not require a credential vault. Match PostgreSQL command-line
tools to the server major version or newer for backups. Debian's default client
may be older than a PostgreSQL 18 server; install matching tools and put them on
PATH. Distribution notices accompany the packaged dependencies.
