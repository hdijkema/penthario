# Local validation: 23 November 2026

Host: Windows x64, MSVC 2022, Release, FLTK 1.4.5. Local sibling library
checkouts were used because their v0.1.0 source tags are not yet published.

Passed:

- Default Qt-free application build and all three CTest tests (core, XLSX,
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
- PE import inspection: native release runtime and libpq, no Qt imports.
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
