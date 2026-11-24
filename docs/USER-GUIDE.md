# User guide

## Connecting

Create a server profile with host, port, user and a maintenance database (usually
`postgres`). Connecting loads the database browser. Right-click a database to
open a query tab there; each tab displays its own database target and owns a
separate PostgreSQL session. Tabs reconnect when their profile is connected,
but saved SQL is never executed automatically.

Remember password uses the operating system's credential vault, not the JSON
workspace. Removing a profile does not drop a database. A missing role during
restore must be created with suitable privileges using Server roles and privileges
or SQL; PostgreSQL roles belong to the server, not to one database.

## Editing and saved queries

F5 runs the selection, or the entire script when there is no selection. Execution
stops at the first SQL error. Ctrl+Space offers keyword/catalog completion;
Ctrl+F searches and Ctrl+H opens replacement. Tab/Shift+Tab indent a selection,
Ctrl+/ toggles line comments. Ctrl+S saves the SQL file or named query. F2 renames
the tab; Ctrl+W closes it with a prompt for unsaved changes.

Save named query stores SQL in the query library, with an optional category and
column widths. The right panel opens these queries and earlier history entries.
Use Refresh saved queries after changing SQL files outside the application.
Closing the application normally restores tab SQL, names, database targets,
cursors and widths on the next launch. Open connections are not persisted.

## Results and exports

Intermediate results retains the result sections of each statement in a single
continuous table. Add `-- @resultaat: A descriptive heading` before a statement
to name its section. Drag column-header boundaries to adjust widths. Double-click
a cell for wrapped text or paged binary/hex details; copy/save accesses the retained
value, with a warning if it was clipped.

Clipboard and preview CSV/XLSX export use the retained results. Full CSV/XLSX
export executes the selection/script again and streams to a file, beyond preview
limits. This can repeat writes: choose your script accordingly. A failed/cancelled
export leaves an existing destination intact. Multiple results get headings and
completion information. XLSX values are strings to preserve precision and prevent
formula evaluation. Read the limits in [FLTK implementation](FLTK.md).

## Execution plans

Select Execution plan before running to request EXPLAIN, choosing format and
options such as ANALYZE, BUFFERS and TIMING. ANALYZE actually executes SQL;
the application rolls back the plan transaction or uses a savepoint inside an
existing transaction. Sequences and external effects are not reversible.

## Administration

Use the database/object context menu for backup, restore, properties, maintenance
and database creation/deletion. Backups use pg_dump; archives restore with
pg_restore, SQL files with psql. Restore stops on the first error and uses a single
transaction. Inspect the log, not just elapsed time. Cancellation can take time
while a tool/server finishes its current work.

VACUUM/ANALYZE/REINDEX run separately from query transactions. Role management
covers attributes, memberships and database privileges; the connected user needs
PostgreSQL's required permissions. General object creation remains available via
SQL. Use PostgreSQL tools for COPY protocol streaming.

## Settings and migration

See [workspace migration](FLTK.md#workspace-migration) for locations and import
from the earlier Qt version. Original settings and SQL files are preserved;
reenter passwords in the native vault. Use `--language nl`, `en` or `de` for an
explicit interface language. Technical/server messages may remain English.

See [development](DEVELOPMENT.md) for building and tests, and
[CI](CI.md) for package/release generation.
