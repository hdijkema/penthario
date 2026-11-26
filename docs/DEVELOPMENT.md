---
title: Development
description: "Build and test penthario, and understand its native FLTK interface, PostgreSQL sessions and data flow."
---

# Development

The root CMake project builds the FLTK application by default. It requires a
C++17 compiler, CMake, libpq and the native libraries listed in
[dependencies](DEPENDENCIES.md).

## Windows

Use Visual Studio 2022 C++ tools, CMake and Ninja. The local scripts discover
Visual Studio with vswhere. `PGADM_CMAKE` and `PGADM_NINJA` override tool paths;
the defaults use CMake and Ninja from PATH.

For sibling penthario/fltk-cell/fltk-css/fltk-dock/fltk-toolbar development checkouts:

```powershell
.\build-fltk.cmd -DFLTK_DIR=C:/path/to/FLTK/CMake
ctest --test-dir build/fltk --output-on-failure
.\build\fltk\penthario.exe
```

For the distribution build, use `build-release.cmd`; it writes `build/Release`.
Pass the same explicit source overrides if the library releases are not yet
published. Windows uses `third_party/postgresql18/pgsql`, overridable with
`PGADM_POSTGRESQL_ROOT`. Runtime files and PostgreSQL notices are copied beside
the executable.

```powershell
.\build-release.cmd -DPGADM_FLTK_CELL_SOURCE_DIR=C:/devel/FLTK/fltk-cell -DFLTK_CELL_CSS_SOURCE_DIR=C:/devel/FLTK/fltk-css
powershell -NoProfile -ExecutionPolicy Bypass -File tools/package-windows.ps1 -InnoCompiler build/tools/inno-setup/ISCC.exe -TestInstaller
```

## Portable CMake entry point

```sh
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel 2
ctest --test-dir build/native --output-on-failure
```

Linux GUI tests need a display, for example `xvfb-run -a ctest ...`. macOS uses a
native Apple Silicon bundle. Platform scripts document their system packages.

## Ownership and concurrency

`window_state` uses FLTK screen/work rectangles and scale to identify monitor
configurations. `Window` stores its normal rectangle, maximized flag, docking
snapshot and dialog rectangles under that key in the existing workspace JSON.
`DialogPlacement` restores one dialog before display and captures its final
rectangle on scope exit; it performs no file I/O. Fixed forms retain their
designed dimensions. Layouts are selected at startup and captured when saving;
monitor hot-plug does not rearrange an active workspace automatically.

Appearance is handled by `Theme`, which loads embedded CSS and optional local
overrides. Native property mapping and the shared palette are supplied by
`FLTK_Css::apply` and `apply_palette`. The application retains its theme policy.
`Window::applyTheme` refreshes dock panels, toolbar SVGs and each tab's syntax
colors. See [themes](THEMES.md) for selectors and the editable translation catalog.

`fltk-toolbar::Toolbar` owns and places the toolbar buttons and separators.
`Fl_Css_Widget<fltk_toolbar::SvgButton>` adds CSS painting to SVG buttons without
making fltk-toolbar depend on CSS. SVG image ownership and disabled images belong
to the toolbar library; penthario chooses icon colors and binds commands.

Each Session owns its libpq connection on one worker. The GUI consumes bounded
event batches, discarding stale execution generations. Catalog and administrative
work do not borrow a query transaction. Cancellation also covers connection
handshakes. Preview memory is bounded; full export streams on the session worker.
ToolJob runs PostgreSQL tools with private child environment credentials and
merged progress output. Passwords are never command-line arguments.

`Window::populateCatalog` consumes completed catalog rows on the GUI thread.
Schema requests combine `pg_class` relations and `pg_proc` routines into one
result; counted category nodes retain the database/schema target but are not SQL
objects. Routine leaves keep their OID and identity arguments, separating
overloads. `inspectObject` retrieves routine properties and `pg_get_functiondef`
by OID; relation definitions continue through pg_dump. Category captions are
localized separately from object identifiers. Browser UI tests feed isolated
catalog fixtures through this path without opening database connections.

`Window::renameDatabase` uses `renameDatabaseSql` to validate and quote identifiers,
then runs ALTER DATABASE through `sessionDialog` on a different maintenance
database. It refuses busy target sessions, closes idle sessions for that profile,
and leaves foreign connections to PostgreSQL's normal checks. Only server success
calls `databaseRenamed` to update and save the profile and query-tab targets;
catalog refresh reloads the affected server. Failed operations retain old targets.

STL models contain no widget pointers. ResultTable maps values to fltk-cell
renderers. Editor overlays belong to fltk-cell, separately from drawing. Keep
wrappers small; the XLSX wrapper only writes result cells and closes the workbook.

New query tabs use an explicit profile/database when supplied. Otherwise
`Window::newQuery` takes the selected browser node's target, then falls back to
the active tab or first profile. Creating a tab does not connect or execute SQL.
`Window::refreshLibrary` rebuilds the saved-query and history trees. FLTK's
`Fl_Tree::clear` deletes the root as well as its children, so each rebuilt tree
gets a new root before any parent-based `add` call. Profile removal follows the
same rule when rebuilding the connection browser.

`fltk-dock::Host` owns the central query tabs and the browser, query-library and
history panels. Panels can move to any edge, float, hide, or share a tabbed docking
area. Each frame owns its content even when a floating window becomes its physical
parent. `Window::save` serializes `Host::save_layout` into the workspace's `docking`
array; loading restores it by panel ID without opening database connections.
Query tabs and result tabs use `fltk_dock::Tabs`, which reuses `Fl_Tabs` event and
selection behavior. Query close requests pass through the existing busy,
transaction and unsaved-work checks. `View` reopens hidden panels or resets their
placement. Until the first fltk-dock source tag is published, configure with
`-DPGADM_FLTK_DOCK_SOURCE_DIR=C:/devel/FLTK/fltk-dock` (or your checkout path).

## Project homepage and About dialog

`docs/index.html` and its local stylesheets form the GitHub Pages homepage.
Jekyll converts the Markdown guides alongside it into HTML, using
`docs/_config.yml` and `docs/_layouts/documentation.html`. The documentation
layout supplies navigation, links back to the homepage and the same theme
control. Its logo and favicon are copies of `assets/icons/penthario.svg`
and `assets/icons/penthario.ico` under `docs/assets/`; refresh those copies when
the application branding changes. The site uses system fonts and no third-party
services. `dark.css` is enabled by default, including without JavaScript. The small
`theme.js` control switches between dark and light and remembers an explicit
choice in local browser storage when available. The racing elephant illustration is recovered from the
original artwork; provenance is recorded in `docs/assets/README.md`.

To publish at <https://hdijkema.github.io/penthario/>, select **Deploy from a
branch** in the repository's **Settings > Pages**, then choose the default
branch and `/docs`. There is no `.nojekyll` marker: GitHub runs its built-in
Jekyll build on publication. Application release workflows do not need to build
or deploy the website.

Markdown files remain the documentation source, with a small YAML title header.
`jekyll-relative-links` converts their existing `.md` cross-references to HTML
links, including anchors. The default layout and `jekyll-optional-front-matter`
also cover new Markdown pages without a header; the sidebar lists those pages
automatically. `assets/README.md` is source-only artwork information and is
excluded. The hand-written homepage has no documentation wrapper.
Its **Docs** navigation link jumps to a documentation overview containing direct
links to every guide. Jekyll generates this list from the documentation pages,
so adding a Markdown guide also adds it to the homepage and sidebar. The
homepage's `layout: null` header enables this Liquid rendering without wrapping
the complete HTML document in another layout.
Both navigation lists explicitly accept only `.md` and `.markdown` source files,
and the default documentation layout is scoped to those extensions. Theme
stylesheets, scripts and other generated assets are never documentation entries.

`LIBRARIES.md` links to the API references and usage examples maintained in each
widget library's own `docs/API.md`. This site documents Penthario's integration;
the library repositories own their APIs, examples and installation instructions.
Keep dependency release selection in `DEPENDENCIES.md`.

For local website builds, use Jekyll 3.10 and its `jekyll-relative-links`,
`jekyll-optional-front-matter` and `kramdown-parser-gfm` gems. These are website
tools, not application dependencies. From the repository root:

```sh
jekyll build --source docs --destination build/pages/penthario
python tests/check-site.py build/pages/penthario
python -m http.server 8000 --directory build/pages
```

Open `http://localhost:8000/penthario/`. The destination deliberately includes
the configured `/penthario` base path so local asset and documentation URLs
match GitHub Pages. Preview the generated site: the source `docs/index.html`
contains Liquid markup for the documentation list and is not a standalone HTML
preview.

### Search discovery and Google verification

`_includes/seo.html` supplies page titles, descriptions, canonical URLs and
Open Graph/social preview metadata. The homepage also describes the project as
a SoftwareApplication using JSON-LD. Guide descriptions live in Markdown front
matter; `_config.yml` supplies the fallback description and preview image.

Jekyll generates `sitemap.xml` from the homepage and Markdown documentation,
using the configured HTTPS host and `/penthario` base path. Assets and Google
verification files are not sitemap entries. No additional Jekyll plugins are
required. After publication, submit this URL in Google Search Console:

<https://hdijkema.github.io/penthario/sitemap.xml>

Google HTML verification files in `docs/google*.html` are copied byte-for-byte.
Use the URL-prefix property `https://hdijkema.github.io/penthario/` in Search
Console, and retain the verification file after verification succeeds.

Robots rules belong at `https://hdijkema.github.io/robots.txt`, outside this
project site's publishing root. A `/penthario/robots.txt` file would not control
crawling. No robots file is needed to allow crawling; if the domain-root site
already has one, its owner can add the sitemap URL while preserving existing
rules. This repository does not change that separate site. See Google's
[robots.txt location requirements](https://developers.google.com/crawling/docs/robots-txt/create-robots-txt).

`Window::about` keeps the application logo and uses FLTK's `Fl_Help_View` for
the fixed HTTPS homepage link. FLTK supplies link hit-testing, the hand cursor
and the platform browser launcher. Only application-owned markup is supplied;
no general HTML renderer or CSS-library changes are required.

## Verification

`ctest` covers core behavior, real XLSX ZIP/XML output and native GUI rendering,
editor behavior, translations and workspace restoration. It needs no database
or credentials.

Interactive UI tests are enabled by default for local builds. Use
`-DPGADM_FLTK_UI_TESTS=OFF` for the smaller noninteractive test suite used by
Actions. Configure with `-DPGADM_FLTK_TESTS=OFF` to omit all application tests.
The package scripts keep their isolated application startup checks enabled.

Local integration is explicit and is not a CI database service:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/run-fltk-integration.ps1
```

It creates its own PostgreSQL cluster and disposable databases, checks ownership
before mutations, and shuts that cluster down. Never point it at an existing
server. Packaging checks use private settings and installer identities.

## Penthario product identity

The product, executable and package name is Penthario (`penthario` in paths and
commands). Renaming the source checkout directory requires reconfiguring
existing CMake build directories.

The application ID `io.github.hdijkema.AltPgAdmin`, workspace directories and
credential service remain stable for existing installations.
Profile IDs, credentials and saved queries therefore need no migration. The
Windows installer keeps its upgrade identity, replaces the old executable and
shortcuts, and retains user data. Debian packages replace/conflict with the old
package names. The website also retains its existing theme-preference storage
key; public links and canonical URLs use `/penthario/`.

Internal `pgjr` namespaces and `PGADM_*` build options remain source/build
compatibility names. New package artifacts and CMake application/test targets
use `penthario`. The existing screenshot is explicitly labelled as an earlier
build; its pixels and the existing application artwork are unchanged.

## Windows FLTK SDK configuration

A Release application must use a Release FLTK SDK. CMake normally allows an
imported Debug library as a fallback when no Release library was installed.
FLTK's Debug WinMain intentionally allocates a console, even for a Windows GUI
executable. The application disallows that fallback and the Windows packager
checks `Console window: none` in the isolated startup report. Build FLTK itself
in Release, or let FetchContent build the selected FLTK release with the app.
Keep a Debug SDK in a separate prefix when both configurations are needed.

## Website theme control

The homepage and documentation layout start with `data-theme="dark"`.
`theme.js` restores a saved light/dark choice before the body paints and attaches
the keyboard-accessible header button. The preference uses the site's own
`data-theme-key` in local storage; storage failures do not prevent switching.
The default is dark even when the operating system prefers light. Without
JavaScript, the pages retain their default dark appearance.
