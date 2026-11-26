---
title: Library documentation
description: "Links to the API references and usage examples maintained in Penthario's FLTK library repositories."
---

# Library documentation

Penthario uses four reusable C++17 libraries around FLTK 1.4.5. Each library
maintains its API reference and usage examples in its own repository:

| Library | Purpose | Documentation |
| --- | --- | --- |
| fltk-css | Stylesheets for native FLTK controls and CSS-painted widgets | [API and examples](https://github.com/hdijkema/fltk-css/blob/main/docs/API.md) |
| fltk-cell | Typed cell rendering and separate cell editors | [API and examples](https://github.com/hdijkema/fltk-cell/blob/main/docs/API.md) |
| fltk-dock | Dockable panels, floating windows and rectangular tabs | [API and examples](https://github.com/hdijkema/fltk-dock/blob/main/docs/API.md) |
| fltk-toolbar | Horizontal toolbars and SVG buttons | [API and examples](https://github.com/hdijkema/fltk-toolbar/blob/main/docs/API.md) |

The references cover each library's public headers, ownership and lifetime,
CMake integration and complete C++ examples. Library documentation follows the
source in that repository; Penthario's selected releases are listed in
[Dependencies and releases](DEPENDENCIES.md).

For how Penthario combines these components, see [Development](DEVELOPMENT.md)
and [Source layout](SOURCE-LAYOUT.md).
