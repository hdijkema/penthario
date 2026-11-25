---
title: Themes and custom CSS
description: "Choose light, dark or classic themes and customise pgadmin-jr with local CSS overrides."
---

# Themes and custom CSS

Choose **View → Theme → Light, Dark or Classic** to change the application's
appearance. The choice is saved with the workspace. Switching themes preserves
SQL, query results, column widths and docking positions.

**Load CSS…** adds a local UTF-8 stylesheet to the selected built-in theme.
**Reload CSS** rereads that file after editing it. **Save stylesheet…** exports
the current combined stylesheet, which is a useful starting point. Selecting a
built-in theme clears custom overrides. The saved workspace remembers both the
base theme and the custom file path; keep the file available when moving settings.

For example, load this file over the Dark theme:

```css
editor { background: #15202b; color: #e7edf3; font-size: 15px; }
syntax.keyword { color: #8acaff; font-weight: bold; }
cell.header { background: #294259; }
icon.run { color: #8fe0ad; }
```

The built-in sources live in `assets/themes/`. They are embedded at build time,
so installed applications do not need separate theme files. Custom files are
limited to 256 KiB. Invalid CSS leaves the active theme unchanged; if a saved
file cannot be loaded at startup, the application uses Light and reports why.

## Styling scope

The application uses fltk-css's supported CSS subset, together with native FLTK
widget properties and palette colors. This controls colors, fonts and supported
borders; it does not provide browser layout, web fonts or arbitrary HTML styling.
Dock panels use ordinary FLTK properties and require no additional CSS dependency.

Selectors used by the application include:

| Selectors | Purpose |
| --- | --- |
| `window`, `group`, `label`, `menu`, `tabs` | Main surfaces and text |
| `panel`, `header`, `splitter`, `scrollbar` | Docking and query pane surfaces |
| `button`, `input`, `tree`, `editor`, `display`, `table` | Widget appearance |
| `cell`, `cell.header`, `cell.section`, `cell.status`, `cell.null` | Result cells |
| `syntax.keyword`, `syntax.comment`, `syntax.string` | SQL highlighting |
| `icon.connect`, `icon.profile`, `icon.new`, `icon.run`, `icon.cancel` | Toolbar strokes |

The built-in files demonstrate `color`, `background`, `selection-color`,
`border-color`, `border`, `border-radius`, `font-family`, `font-size` and
`font-weight`. CSS button states include `:hover`, `:active` and `:disabled`.
State styling applies to CSS widgets; ordinary native controls use their FLTK
appearance and the shared palette. Native menus, tooltips and newly opened dialogs
follow that palette. Operating-system title bars and file pickers retain the
system appearance.

## Development

`Theme::load` parses the base stylesheet plus optional overrides before changing
the FLTK palette. `Theme::apply` delegates native widget properties to fltk-css;
`Window::applyTheme` updates dock panels, toolbar SVGs and each query tab's syntax
colors. Result cells resolve their own fltk-cell styles from the same stylesheet.
Toolbar layout and SVG image ownership are supplied by fltk-toolbar. Icon color
selection remains application-specific.

Edit `translations/fltk-ui.json` directly for interface translations. Source keys
are English, with Dutch and German values. `tests/check-translations.py` checks
duplicate messages, completeness and placeholders as part of CTest.
