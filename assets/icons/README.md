# Toolbar icons

SVG artwork from [Lucide](https://github.com/lucide-icons/lucide), tag 0.468.0,
commit f12b0de177fbc2a6795e99be065887e72b237123, under the ISC licence. Some
Feather-derived artwork is covered by the MIT notice included in LICENSE.txt.
The full upstream licence is retained beside these files and deployed with the app.

Source: icons/{name}.svg in that commit. Only stroke colours have been changed
for the toolbar; shapes and the original 24 by 24 view boxes are unchanged.
Blue indicates query/file actions, amber opening files, purple profiles, green
connect/run and red disconnect/stop. Disabled rendering uses the native FLTK style.

The SVGs are embedded through a generated C++ header and rendered by
`Fl_SVG_Image`. Toolbar colors follow the active CSS theme.

## Application icon

`penthario.svg` is the application artwork supplied by Hans, separate from the
Lucide toolbar set. The Windows executable uses the derived ICO; the application
uses the embedded SVG for its window and About dialog.
Linux packaging installs the SVG under the existing desktop application ID.

`penthario.ico` contains 16, 24, 32, 48, 64, 128 and 256 pixel images for the
Windows executable. `penthario.icns` supplies macOS bundle sizes through 1024
pixels, including retina variants. Both are generated from the SVG and committed.
To regenerate them after changing the SVG, run:

```powershell
py -3 tools/generate-app-icons.py
```

If the SVG and Windows ICO were supplied together, preserve the supplied ICO and
update only the macOS derivative:

```powershell
py -3 tools/generate-app-icons.py --icns-only
```

The Windows resource explicitly depends on the ICO so incremental builds refresh
the executable icon.

The generator uses Inkscape (or an explicit `--inkscape` path) and Python's standard
library. Normal builds use the checked-in assets and need neither Inkscape nor
additional Python packages. Native macOS/Linux icon display still needs validation
on those platforms.
