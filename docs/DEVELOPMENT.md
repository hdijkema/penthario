---
title: Development
---

# Development

The root CMake project builds the FLTK application by default. It requires a
C++17 compiler, CMake, libpq and the native libraries listed in
[dependencies](DEPENDENCIES.md).

## Windows

Use Visual Studio 2022 C++ tools, CMake and Ninja. The local scripts discover
Visual Studio with vswhere. `PGADM_CMAKE` and `PGADM_NINJA` override tool paths;
the defaults use CMake and Ninja from PATH.

For sibling pgadmin-jr/fltk-cell/fltk-css/fltk-dock/fltk-toolbar development checkouts:

```powershell
.\build-fltk.cmd -DFLTK_DIR=C:/path/to/FLTK/CMake
ctest --test-dir build/fltk --output-on-failure
.\build\fltk\pgadmin-jr.exe
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
to the toolbar library; pgadmin-jr chooses icon colors and binds commands.

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
control. Its logo and favicon are copies of `assets/icons/pgadmin-jr.svg`
and `assets/icons/pgadmin-jr.ico` under `docs/assets/`; refresh those copies when
the application branding changes. The site uses system fonts and no third-party
services. `dark.css` follows the system preference without JavaScript; the small
`theme.js` control allows a manual override and remembers it in local browser
storage when available. The racing elephant illustration is recovered from the
original artwork; provenance is recorded in `docs/assets/README.md`.

To publish at <https://hdijkema.github.io/pgadmin-jr/>, select **Deploy from a
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

For local website builds, use Jekyll 3.10 and its `jekyll-relative-links`,
`jekyll-optional-front-matter` and `kramdown-parser-gfm` gems. These are website
tools, not application dependencies. From the repository root:

```sh
jekyll build --source docs --destination build/pages/pgadmin-jr
python -m http.server 8000 --directory build/pages
```

Open `http://localhost:8000/pgadmin-jr/`. The destination deliberately includes
the configured `/pgadmin-jr` base path so local asset and documentation URLs
match GitHub Pages. Opening `docs/index.html` directly still previews the home
page, but its generated guide links require the Jekyll build.

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

## Windows FLTK SDK configuration

A Release application must use a Release FLTK SDK. CMake normally allows an
imported Debug library as a fallback when no Release library was installed.
FLTK's Debug WinMain intentionally allocates a console, even for a Windows GUI
executable. The application disallows that fallback and the Windows packager
checks `Console window: none` in the isolated startup report. Build FLTK itself
in Release, or let FetchContent build the selected FLTK release with the app.
Keep a Debug SDK in a separate prefix when both configurations are needed.
