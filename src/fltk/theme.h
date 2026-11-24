#pragma once
#include <fltk_css_widgets.h>
#include <filesystem>
#include <string>
namespace pgjr
{
// Load a built-in CSS base plus optional local overrides. The FLTK palette also
// updates native menus/dialogs; this is not a browser CSS/layout implementation.
class Theme
{
  public:
    // Attach a stylesheet owned by the Window; initialize its Light base and palette.
    explicit Theme(FLTK_Css &sheet);
    // Load a known base and optional UTF-8 file (at most 256 KiB). Throws on failure;
    // parsing succeeds before the active stylesheet, palette and preferences change.
    void load(const std::string &name, const std::filesystem::path &file = {});
    // Apply resolved CSS to native properties, recursing through owned child widgets.
    void apply(Fl_Widget &) const;
    // Apply a named style to one widget; the caller retains ownership.
    void apply(Fl_Widget &, const char *type) const;
    // Resolve a CSS node used by cell, syntax and toolbar renderers.
    FLTK_Css_Style style(const char *type, std::vector<std::string> classes = {}) const;
    const std::string &name() const
    {
        return name_;
    }
    const std::filesystem::path &file() const
    {
        return file_;
    }
    const std::string &source() const
    {
        return source_;
    }

  private:
    FLTK_Css &sheet_;
    std::string name_, source_;
    std::filesystem::path file_;
};
} // namespace pgjr
