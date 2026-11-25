# FLTK implementation

The default build is a native C++17/FLTK application. It uses fltk-css for
styling, fltk-cell for table rendering, fltk-toolbar for buttons and SVG images,
STL data structures, libpq for database
sessions and a small libxlsxwriter wrapper for XLSX output.

## Available functionality

- Lazy server/database/schema browser with counted groups for tables (including
  partitioned tables), views, materialized views, sequences, foreign tables,
  functions, procedures and trigger functions. Relations expand to columns;
  routine overloads show identity arguments and are inspected by their catalog OID.
  Object properties and definition SQL, database creation/deletion and independent query tabs.
- SQL coloring, line numbers, indentation, comments, search/replace, explicit
  completion, named queries/categories, history and workspace restoration.
- Continuous result sections, column widths, wrapped/paged text and binary cell
  details, clipboard, preview and streaming full CSV/XLSX export.
- EXPLAIN options including ANALYZE, with rollback or savepoint protection.
- Backup/restore with elapsed time, logs and cancellation; maintenance;
  role attributes/memberships and database privileges.
- Native password vault, Dutch/English/German interface, SVG icons and About logo.

## Deliberate limits

Preview limits are 50,000 rows, approximately 32 MiB and 2 MiB per cell.
Clipping is marked. Full exports execute the SQL again and stream independently
of these limits. Exporting again can repeat writes in the script. Failed or
cancelled exports preserve an existing destination. XLSX cells are written as
strings to preserve exact PostgreSQL values and avoid spreadsheet formulas;
Excel's 32,767-character cell limit still applies.

At most eight query sessions are opened, plus catalog/administration sessions.
Scripts are limited to 8 MiB and 2,000 statements; execution stops at the first
error. Use explicit BEGIN/COMMIT for transactional scripts. Results are read-only.
COPY protocol streaming is not supported by the editor; use PostgreSQL tools.
Large values must still be transferred by libpq before clipping the preview.

EXPLAIN ANALYZE executes SQL. Rollback cannot undo sequence increments or external
effects. Completion is a local keyword/catalog aid, not a PostgreSQL parser.
Technical error messages may remain English or use the server's language.

## Workspace migration

The native workspace uses JSON, keeping the preview frontend's location:

- Windows: `%APPDATA%/Hans/pgadmin-jr-fltk/settings.json`
- macOS: `~/Library/Application Support/pgadmin-jr-fltk/settings.json`
- Linux: `$XDG_CONFIG_HOME/pgadmin-jr-fltk/settings.json`, falling back to
  `~/.config/pgadmin-jr-fltk/settings.json`.

Use the import command to read an legacy workspace INI and its saved query
library. Original files are retained; conflicting named queries are skipped.
Profile passwords are not imported. Enter them again and select
Remember password to save them in the native operating-system vault. Passwords
are excluded from JSON. Importing/restoring tabs never executes saved SQL.

`--settings-file <path>` isolates the workspace. `--startup-check` opens and
closes without saving, and `--startup-report <path>` records toolkit/version
and construction time. These checks do not connect to PostgreSQL.

Windows builds, native rendering tests, disposable PostgreSQL integration and
packaging can be checked locally. macOS, Debian and Flatpak workflow changes
require hosted platform validation; Windows tests do not establish their success.
