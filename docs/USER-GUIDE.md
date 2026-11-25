---
title: User guide
description: "Connect to PostgreSQL, browse objects, write SQL and export query results with pgadmin-jr."
---

# User guide

## Windows installation

The installer offers installation for the current user or for all users.
The current-user mode is the default and uses
`%LOCALAPPDATA%\Programs\pgadmin-jr`; all-users mode requests administrator
rights and defaults to `C:\Program Files\pgadmin-jr`. Start-menu and optional
desktop shortcuts follow the selected scope. Workspace files and credentials
remain private to each user in either mode.

For unattended installation, select the scope explicitly with `/CURRENTUSER`
or `/ALLUSERS`. Updates normally reuse the previous installation scope and
directory. An explicit scope switch is a separate installation, not a migration
or removal of the other scope. The portable ZIP remains an alternative.

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

## Browsing schema objects

Expand a schema to see counted groups for Tables, Views, Materialized views,
Sequences, Foreign tables, Functions, Procedures and Trigger functions. Tables
include partitioned tables. Relations expand to their columns; routines are leaves.
Expand a group to see its objects. Category names are translated; SQL identifiers
retain their exact spelling.

Function and procedure names include their identity arguments, so overloaded
routines remain distinct. **Inspect object** shows properties and **Definition
SQL** shows the reconstructed CREATE OR REPLACE statement, which can be copied or
saved. Inspection uses the routine's OID in the selected database; opening a query
from a routine or category does not invoke a function or procedure.

The browser currently covers these relation/routine groups, not every object
category offered by pgAdmin 4. Aggregates, types/domains, operators, full-text
search objects and system-schema navigation are not yet separate browser sections.

## Editing and saved queries

F5 runs the selection, or the entire script when there is no selection. Execution
stops at the first SQL error. Ctrl+Space offers keyword/catalog completion;
Ctrl+F searches and Ctrl+H opens replacement. Tab/Shift+Tab indent a selection,
Ctrl+/ toggles line comments. Ctrl+S saves the SQL file or named query. F2 renames
the tab; Ctrl+W closes it with a prompt for unsaved changes.

Save named query stores SQL in the query library, with an optional category and
column widths. The right panel opens these queries and earlier history entries.
Use Query > Saved queries > Refresh after changing SQL files outside the application.
Closing the application normally restores tab SQL, names, database targets,
cursors and widths on the next launch. Open connections are not persisted.

The icon toolbar provides connection/profile, new query, run and cancel actions.
Hover for translated tooltips. Search and editing commands are under Edit; named
query operations are grouped under Query > Saved queries. Language is under
View > Language, with Help as the final main menu.

Drag the horizontal separator above the output tabs to give the editor or results
more height; double-click it to reset. The ratio is saved separately for each tab.

## Results and exports

Intermediate results retains the result sections of each statement in a single
continuous table. Add `-- @resultaat: A descriptive heading` before a statement
to name its section. Drag column-header boundaries to adjust widths. For a single
result, the fixed column header contains the actual column names. Double-click
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

## Appearance

Use **View → Theme** to select Light, Dark or Classic. You can load, reload and
save custom CSS there too. The workspace remembers your theme and custom file.
See [themes](THEMES.md) for examples and supported selectors.

## Settings and migration

The workspace remembers the main window's position, normal size and maximized
state, plus docked/floating panel placement, sizes, grouping and visibility.
These layouts are stored separately for each monitor arrangement, resolution,
work area and scaling configuration. The appropriate layout is restored at
startup; up to sixteen configurations are kept. A new configuration starts with
a centered main window and the last saved panel arrangement. Floating windows
are kept within an available screen.

Cell details, object properties and task-log dialogs remember their position and
resizable dimensions. Fixed forms, Find/Replace and About remember position;
their control layout keeps its designed size. Object properties also remembers
column widths and its selected page. Native file pickers and standard message
boxes retain their platform behavior; brief credential/export progress indicators
do not save geometry. Query splits, column widths and the selected result page
are restored with each tab. Settings are written on normal exit and existing
workspace-save actions; forced process termination can lose recent changes.

All of this uses the existing JSON workspace; no additional preferences library
or separate settings file is needed. Dialog rectangles are checked against the
current screens before display, including after a monitor is disconnected.

See [workspace migration](FLTK.md#workspace-migration) for locations and import
from a legacy workspace. Original settings and SQL files are preserved;
reenter passwords in the native vault. Use `--language nl`, `en` or `de` for an
explicit interface language. Technical/server messages may remain English.

See [development](DEVELOPMENT.md) for building and tests, and
[CI](CI.md) for package/release generation.
