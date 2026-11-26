#include "theme.h"
#include "themes.h"
#include "core.h"
#include <stdexcept>
namespace pgjr
{
Theme::Theme(FLTK_Css &sheet) : sheet_(sheet)
{
    load("dark");
}
FLTK_Css_Style Theme::style(const char *type, std::vector<std::string> classes) const
{
    FLTK_Css_Node node;
    node.type = type;
    node.classes = std::move(classes);
    return sheet_.style(node);
}
void Theme::load(const std::string &name, const std::filesystem::path &file)
{
    std::string source;
    if (name == "light")
        source = lightCss;
    else if (name == "dark")
        source = darkCss;
    else if (name == "classic")
        source = classicCss;
    else
        throw std::invalid_argument("Unknown theme: " + name);
    if (!file.empty())
    {
        if (std::filesystem::file_size(file) > 256 * 1024)
            throw std::runtime_error("CSS files are limited to 256 KiB");
        source += "\n" + readFile(file);
    }
    // parse() commits only on success; keep the old palette/preferences too on failure.
    if (!sheet_.parse(source))
        throw std::runtime_error("Invalid CSS: " + (sheet_.errors().empty() ? std::string("parse error")
                                                                            : sheet_.errors().front()));
    name_ = name;
    file_ = file;
    source_ = std::move(source);
    Fl::scheme(name == "classic" ? "base" : "gtk+");
    sheet_.apply_palette("window", "editor");
}
void Theme::apply(Fl_Widget &widget, const char *type) const
{
    sheet_.apply(widget, type);
}
void Theme::apply(Fl_Widget &widget) const
{
    sheet_.apply(widget);
}
} // namespace pgjr
