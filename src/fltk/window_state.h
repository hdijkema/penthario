#pragma once
#include "core.h"
#include <FL/Fl_Window.H>

namespace pgjr
{
// Identify the current arrangement by sorted screen/work rectangles and scale.
// Uses FLTK logical coordinates; no platform APIs or hardware identifiers are needed.
std::string monitorConfiguration();
// Capture a native window's client rectangle. Call on the GUI thread.
Json windowRectangle(const Fl_Window &);
// Restore valid coordinates within a current screen's work area. Missing/invalid
// fields keep defaults; fixed-size dialogs retain their designed dimensions.
void restoreRectangle(Fl_Window &, const Json &, const Fl_Window *parent = nullptr, bool restore_size = true);

// Restore one application dialog before show(), then remember its final rectangle
// on scope exit. The dialog and JSON map must outlive this guard. No file I/O.
class DialogPlacement
{
  public:
    DialogPlacement(Fl_Window &, Json &dialogs, std::string id, const Fl_Window &parent);
    ~DialogPlacement();
    DialogPlacement(const DialogPlacement &) = delete;
    DialogPlacement &operator=(const DialogPlacement &) = delete;

  private:
    Fl_Window &window_;
    Json &dialogs_;
    std::string id_;
};
} // namespace pgjr
