# Source layout

The current application is in `src/fltk/`:

| Files | Responsibility |
| --- | --- |
| main, window | Startup, native UI, tabs, workspace orchestration |
| core | STL values, SQL splitting, files, profile JSON and vault bridge |
| session | Worker-owned libpq sessions, cancellation, previews and full export |
| result_table | Continuous result model and fltk-cell integration |
| sql_editor | Editing, indentation, search/replace and completion |
| query_library, library_ui | SQL files, categories, widths and history/sidebar |
| admin_sql, admin_dialogs | Validated administrative SQL and native dialogs |
| tool_job | PostgreSQL subprocesses, backup/restore and elapsed/log progress |
| xlsx_writer | Minimal libxlsxwriter wrapper and atomic file publication |
| legacy_workspace | Read-only Qt workspace migration without linking Qt |
| i18n | Embedded native translation catalog |

`cmake/FltkApplication.cmake` defines the active targets and dependencies;
`FltkLinuxPackaging.cmake` defines the native DEB and `Xlsx.cmake` the export
dependencies. `assets/` holds icons/notices, `translations/fltk-*.json` the UI
catalog, `tools/` platform packaging and `tests/fltk_*` the native tests.

Earlier Qt source folders and tests remain for reference. They are excluded
from the default build and packages; `cmake/QtLegacy.cmake` is only included when
`PGADM_BUILD_QT_LEGACY=ON` is explicitly requested.
