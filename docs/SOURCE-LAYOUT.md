# Source layout

The current application is in `src/fltk/`:

| Files | Responsibility |
| --- | --- |
| main, window | Startup, native UI, tabs, workspace orchestration |
| window_state | Screen-aware window and dialog geometry; monitor layout keys |
| core | STL values, SQL splitting, files, profile JSON and vault bridge |
| session | Worker-owned libpq sessions, cancellation, previews and full export |
| result_table | Continuous result model and fltk-cell integration |
| sql_editor | Editing, indentation, search/replace and completion |
| query_library, library_ui | SQL files, categories, widths and history/sidebar |
| admin_sql, admin_dialogs | Validated administrative SQL and native dialogs |
| tool_job | PostgreSQL subprocesses, backup/restore and elapsed/log progress |
| xlsx_writer | Minimal libxlsxwriter wrapper and atomic file publication |
| legacy_workspace | Read-only legacy workspace migration |
| i18n | Embedded native translation catalog |
| theme, themes.h.in | Embedded CSS themes, custom stylesheets and native widget styling |

Generic native widget styling lives in fltk-css. Toolbar layout and SVG image
ownership live in fltk-toolbar. The application binds commands, selects icon
colors and supplies its own theme files; those libraries do not depend on
PostgreSQL or the workspace format.

`cmake/FltkApplication.cmake` defines the active targets and dependencies;
`FltkLinuxPackaging.cmake` defines the native DEB and `Xlsx.cmake` the export
dependencies. `assets/` holds icons/notices, `translations/fltk-ui.json` the UI
catalog, `tools/` platform packaging and `tests/fltk_*` the native tests.
