#include "theme.h"
#include "themes.h"
#include "core.h"
#include "result_table.h"
#include <fltk_dock.h>
#include <FL/Fl_Input_.H>
#include <FL/Fl_Menu_.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Scrollbar.H>
#include <FL/Fl_Tooltip.H>
#include <FL/Fl_Check_Button.H>
#include <stdexcept>
namespace pgjr
{
Theme::Theme(FLTK_Css &sheet) : sheet_(sheet)
{
    load("light");
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
    const auto window = style("window"), surface = style("editor");
    unsigned char r, g, b;
    Fl::get_color(window.background, r, g, b);
    Fl::background(r, g, b);
    Fl::get_color(surface.background, r, g, b);
    Fl::background2(r, g, b);
    Fl::get_color(window.color, r, g, b);
    Fl::foreground(r, g, b);
    Fl::get_color(window.selection_color, r, g, b);
    Fl::set_color(FL_SELECTION_COLOR, r, g, b);
    Fl_Tooltip::color(window.background);
    Fl_Tooltip::textcolor(window.color);
}
void Theme::apply(Fl_Widget &widget, const char *type) const
{
    const auto value = style(type);
    fltk_css_detail::apply(widget, value);
    if (auto *view = dynamic_cast<Fl_Text_Display *>(&widget))
    {
        view->textcolor(value.color);
        view->cursor_color(value.color);
        view->textfont(FL_COURIER);
        view->textsize(value.font_size);
        view->linenumber_bgcolor(value.background);
        view->linenumber_fgcolor(style("cell", {"null"}).color);
    }
}
void Theme::apply(Fl_Widget &widget) const
{
    const char *type = widget.as_window() ? "window" : widget.as_group() ? "group" : "label";
    if (dynamic_cast<Fl_Menu_ *>(&widget))
        type = "menu";
    else if (dynamic_cast<Fl_Text_Display *>(&widget))
        type = "editor";
    else if (dynamic_cast<Fl_Input_ *>(&widget))
        type = "input";
    else if (dynamic_cast<Fl_Tree *>(&widget))
        type = "tree";
    else if (dynamic_cast<ResultTable *>(&widget))
        type = "table";
    else if (dynamic_cast<fltk_dock::Tabs *>(&widget))
        type = "tabs";
    else if (dynamic_cast<fltk_dock::Panel *>(&widget))
        type = "panel";
    else if (dynamic_cast<Fl_Scrollbar *>(&widget))
        type = "scrollbar";
    else if (dynamic_cast<Fl_Button *>(&widget))
        type = "button";
    apply(widget, type);
    const auto value = style(type);
    if (auto *menu = dynamic_cast<Fl_Menu_ *>(&widget))
    {
        menu->textcolor(value.color);
        menu->textsize(value.font_size);
    }
    if (dynamic_cast<Fl_Check_Button *>(&widget))
        widget.selection_color(value.selection_color);
    if (auto *tabs = dynamic_cast<fltk_dock::Tabs *>(&widget))
        tabs->selection_color(style("editor").background);
    if (auto *tree = dynamic_cast<Fl_Tree *>(&widget))
    {
        tree->item_labelfgcolor(value.color);
        tree->item_labelbgcolor(value.background);
        tree->item_labelsize(value.font_size);
        tree->connectorcolor(value.border_color);
        for (auto *item = tree->first(); item; item = tree->next(item))
        {
            item->labelfgcolor(value.color);
            item->labelbgcolor(value.background);
            item->labelsize(value.font_size);
        }
        tree->redraw();
        return;
    }
    // Table children are its internal scroll implementation; cells resolve their own CSS nodes.
    if (dynamic_cast<ResultTable *>(&widget))
    {
        widget.redraw();
        return;
    }
    if (auto *group = widget.as_group())
        for (int i = 0; i < group->children(); ++i)
            apply(*group->child(i));
    widget.redraw();
}
} // namespace pgjr
