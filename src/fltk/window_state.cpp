#include "window_state.h"
#include <FL/Fl.H>
#include <algorithm>
#include <cmath>

namespace pgjr
{
std::string monitorConfiguration()
{
    std::vector<std::string> screens;
    for (int i = 0; i < Fl::screen_count(); ++i)
    {
        int x, y, w, h, wx, wy, ww, wh;
        Fl::screen_xywh(x, y, w, h, i);
        Fl::screen_work_area(wx, wy, ww, wh, i);
        screens.push_back(Json::array({x, y, w, h, wx, wy, ww, wh,
                                       static_cast<int>(std::lround(Fl::screen_scale(i) * 100))})
                              .dump());
    }
    std::sort(screens.begin(), screens.end());
    return Json(screens).dump();
}
Json windowRectangle(const Fl_Window &window)
{
    return {{"x", window.x()}, {"y", window.y()}, {"w", window.w()}, {"h", window.h()}};
}
void restoreRectangle(Fl_Window &window, const Json &state, const Fl_Window *parent, bool restore_size)
{
    // Constrain corrupt or extreme coordinates before passing them to FLTK screen selection.
    auto field = [&state](const char *key, int fallback)
    {
        if (!state.is_object() || !state.contains(key) || !state[key].is_number_integer())
            return fallback;
        if (state[key].is_number_unsigned() && state[key].get<std::uint64_t>() > 100000)
            return fallback;
        auto value = state[key].get<std::int64_t>();
        return value >= -100000 && value <= 100000 ? static_cast<int>(value) : fallback;
    };
    int w = window.w(), h = window.h();
    if (restore_size)
    {
        int minimum_w = 160, minimum_h = 100;
        window.get_size_range(&minimum_w, &minimum_h);
        w = std::max(std::max(160, minimum_w), field("w", w));
        h = std::max(std::max(100, minimum_h), field("h", h));
    }
    int x = parent ? parent->x() + (parent->w() - w) / 2 : window.x();
    int y = parent ? parent->y() + (parent->h() - h) / 2 : window.y();
    x = field("x", x);
    y = field("y", y);
    int sx, sy, sw, sh;
    Fl::screen_work_area(sx, sy, sw, sh, x, y);
    if (restore_size)
    {
        w = std::min(w, std::max(160, sw - 16));
        h = std::min(h, std::max(100, sh - 48));
    }
    x = std::clamp(x, sx, sx + std::max(0, sw - w));
    y = std::clamp(y, sy + 30, sy + std::max(30, sh - h - 8));
    window.resize(x, y, w, h);
}
DialogPlacement::DialogPlacement(Fl_Window &window, Json &dialogs, std::string id, const Fl_Window &parent)
    : window_(window), dialogs_(dialogs), id_(std::move(id))
{
    if (!dialogs_.is_object())
        dialogs_ = Json::object();
    restoreRectangle(window_, dialogs_.value(id_, Json::object()), &parent, window_.resizable() != nullptr);
}
DialogPlacement::~DialogPlacement()
{
    // Retain any dialog-specific layout fields beside its rectangle.
    try
    {
        auto &state = dialogs_[id_];
        if (!state.is_object())
            state = Json::object();
        state.update(windowRectangle(window_));
    }
    catch (...)
    {
        // Scope exit must never hide an earlier operation failure.
    }
}
} // namespace pgjr
