---
title: Validation report
description: "Local build, packaging, PostgreSQL and interface validation results for pgadmin-jr, including platform limits."
---

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

## 25 November 2026: homepage, About link and schema browser

Windows x64 Release build completed with MSVC 2022 and the existing FLTK 1.4.5
SDK. The stale local `PGADM_RELEASE_VERSION=0.3.10` cache override was cleared;
the application uses the source version, 0.3.18. All four CTest tests passed.

The native UI checks now cover counted schema-object groups, partitioned tables,
routine overload signatures/OIDs, category-to-database query targets, translated
category captions and refresh cleanup without a database connection. The About
dialog renders its fixed homepage hyperlink without scrollbars. Its browser
launcher was not invoked automatically by the test.

The exact catalog and routine-inspector SQL extracted from the application
source was also exercised against an isolated PostgreSQL 18.6 cluster. Checks
covered ordinary/partitioned tables, views, materialized views, sequences,
foreign tables, overloaded functions, procedures, trigger functions, a quoted
schema name and reconstructed routine definitions. The temporary server was
stopped afterward; no existing database or credentials were used.

The buildless homepage was rendered locally in headless Chrome. Both themes
passed layout/asset checks at 320, 390, 768 and 1440 logical pixels, without
horizontal page overflow. Theme toggling, persisted preference, system dark
preference and CSS fallback with the page script blocked passed. The desktop
dark rendering and the SVG racing-elephant illustration were visually inspected.
Local asset references and matching application logo/favicon copies were checked.

GitHub Pages settings, remote publication and native macOS/Linux execution were
not changed or validated by these checks.

## 25 November 2026: generated documentation and installer scopes

The website built locally with Jekyll 3.10.0, Kramdown 2.4.0,
jekyll-relative-links 0.6.1 and jekyll-optional-front-matter 0.3.2. The ten source
guides produced HTML pages with shared light/dark styling, sidebar navigation,
source links and links back to the homepage. All 278 local references, assets
and anchors across the eleven HTML pages passed checks. An isolated extra
Markdown file without front matter also received the layout and a sidebar entry.

Headless Chrome served the actual generated output over local HTTP. All eleven
pages passed narrow-screen overflow, image and navigation checks at 320 pixels.
The documentation was also rendered and visually inspected in light/dark mode
at 390 and 1440 pixels; the preference persisted between guide pages.

The Windows 0.4.1 Release build and all four CTest tests passed. Inno Setup 7.1.1
compiled the installer with current-user/all-users selection and mode-dependent
paths. An isolated current-user test passed install/reinstall/startup/uninstall,
checked HKCU registration and preserved separate settings and an untracked query.
The private test identity is kept short to avoid Inno's registry-key shortening.
Actual all-users installation was not run: this session is not elevated.

No GitHub Pages settings or remote deployments were changed. The preview server
and browser were stopped; website build tools remain in the ignored local build
directory and are not application dependencies.
