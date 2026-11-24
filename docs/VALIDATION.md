# Local validation: 23 November 2026

Host: Windows x64, MSVC 2022, Release, FLTK 1.4.5. Local sibling library
checkouts were used because their v0.1.0 source tags are not yet published.

Passed:

- Native application build and all three CTest tests (core, XLSX,
  native GUI rendering/editor/workspace behavior).
- Disposable PostgreSQL 18.6 integration: independent sessions, cancellation
  including a stalled connection handshake, EXPLAIN rollback/savepoints,
  large preview/full exports, role/privilege SQL, maintenance and database
  create/drop/backup/restore. Tool tests include Unicode/quotes/spaces and
  restored bytea contents. The runner stopped its own cluster afterwards.
- fltk-cell core-only and CSS-enabled tests, installation and installed CMake
  consumers; fltk-css and fltk-trayicon Windows contract tests.
- Windows staging with development PATH removed, ZIP CRC/payload/notices checks,
  and Inno Setup install/reinstall/startup/uninstall with a private identity.
- PE import inspection: native release runtime and libpq.
- Thirteen release-asset helper tests, release-version cases, five macOS
  dependency-audit tests, actionlint, Python/JSON parsing and diff whitespace.

The staged startup check recorded 24 ms to construct its empty window. This is
one local measurement, not an end-to-end startup guarantee or a server benchmark.
The native executable is approximately 2.3 MiB; the portable ZIP is approximately
6.2 MiB including PostgreSQL client tools, DLLs, runtime and notices.

Not validated here: actual macOS bundle/DMG, Debian/ARM64/Flatpak builds, native
macOS/Linux vault behavior, and interactive desktop automation. The Windows
computer-use connection was unavailable; GUI tests render the application's
own widgets programmatically. Hosted workflows must validate other platforms
once fltk-css and fltk-cell v0.1.0 tags have been published. No source tags,
commits, pushes or public releases were created by this work.

## 24 November 2026: version 0.3.9

The local Release build now links a separately installed Release FLTK 1.4.5 SDK.
The previous Debug-only SDK was causing FLTK's WinMain to allocate a console.
An explicit negative configuration check rejects that SDK for Release builds.
All three application tests pass with the Release SDK. PE inspection confirms
Windows GUI subsystem and no AllocConsole import. The staged startup reports
`Console window: none`; ZIP integrity and private installer lifecycle checks pass.

## 24 November 2026: version 0.3.10

Light, Dark and Classic themes, custom CSS reload, invalid-file recovery and
workspace theme persistence pass the native UI tests. Light, Dark and Classic
render artifacts were visually inspected. SQL and pane proportions survive theme
changes, including floating dock windows. All four CTest tests pass, including
the editable JSON translation catalog check.

Obsolete source, build routes and translation generation were removed from the
active tree. Legacy workspace import remains covered by the core tests. Previous
source files are preserved in an ignored local build archive.

The fltk-dock contract test, thirteen release-asset tests, five macOS dependency
audit tests, actionlint and the Razer application boundary check pass. Windows
ZIP staging and isolated Inno Setup install/reinstall/startup/uninstall pass for
0.3.10. The staged application reports no console window. Other platforms still
require their hosted build and runtime checks.
