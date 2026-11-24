# Dependencies and releases

| Component | Selected release | Purpose |
| --- | --- | --- |
| FLTK | 1.4.5 (`release-1.4.5`) | Native widgets, drawing, SVG |
| fltk-css | 0.1.0 (`v0.1.0`) | Styles |
| fltk-cell | 0.1.0 (`v0.1.0`) | Typed cell drawers/editors |
| fltk-dock | 0.1.0 (local development; tag pending) | Dockable tool windows and rectangular tabs |
| nlohmann/json | 3.12.0 | Workspace and metadata |
| hrantzsch/keychain | 1.3.1 | Windows Credential Manager, macOS Keychain, Linux Secret Service |
| libxlsxwriter | 1.2.4 | XLSX output |
| zlib | 1.3.2 | XLSX compression |
| PostgreSQL | libpq >=15; packaged tools 18.6 | Protocol and administration tools |

fltk-trayicon is a separate library at version 0.1.0, also targeting FLTK 1.4.5.
It is not a dependency of pgadmin-jr, which has no tray workflow.

Dependencies use explicit release tags or checksummed release archives rather
than moving branches or arbitrary development commits. An installed FLTK package
must satisfy the minimum supported release. Upgrades are deliberate, tested
changes; builds do not silently choose whatever happens to be newest that day.

## First publication order

The fltk-css, fltk-cell and fltk-trayicon `v0.1.0` tags have been published.
The new fltk-dock library is still under local validation. Before a clean network
build of the docking-enabled application:

1. Validate and publish fltk-dock as `v0.1.0`.
2. Build/publish pgadmin-jr. fltk-trayicon is independent.

The library workflows create a source GitHub Release for a matching version tag
after their build matrix succeeds. No third-party repository is modified.

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
