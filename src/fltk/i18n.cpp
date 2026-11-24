#include "i18n.h"
#include "ui_catalog.h"
#include <nlohmann/json.hpp>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Menu_.H>
#include <FL/fl_ask.H>
#include <map>
#include <stdexcept>
namespace pgjr
{
namespace
{
std::string locale = "en";
const nlohmann::json catalog = nlohmann::json::parse(uiCatalog);
const std::map<std::string, std::string> originals = []()
{
    std::map<std::string, std::string> values;
    for (auto it = catalog.begin(); it != catalog.end(); ++it)
    {
        values[it.key()] = it.key();
        for (const auto &value : it.value())
            if (value.is_string())
                values[value.get<std::string>()] = it.key();
    }
    return values;
}();
} // namespace
void language(std::string value)
{
    if (value != "en" && value != "nl" && value != "de")
        throw std::runtime_error("Language must be nl, en or de");
    locale = std::move(value);
    fl_ok = tr("OK");
    fl_cancel = tr("Cancel");
    fl_yes = tr("Yes");
    fl_no = tr("No");
    fl_close = tr("Close");
}
const std::string &language()
{
    return locale;
}
const char *tr(const char *value)
{
    if (!value)
        return nullptr;
    auto source = originals.find(value);
    if (source == originals.end())
        return value;
    auto it = catalog.find(source->second);
    auto translated = it->find(locale);
    if (translated != it->end())
        return translated->get_ref<const std::string &>().c_str();
    return source->second.c_str();
}
void localize(Fl_Widget &widget)
{
    if (widget.label())
        widget.copy_label(tr(widget.label()));
    if (widget.tooltip())
        widget.tooltip(tr(widget.tooltip()));
    if (auto *menu = dynamic_cast<Fl_Menu_ *>(&widget))
    {
        for (int i = 0; i < menu->size(); ++i)
            if (menu->menu()[i].text)
            {
                const std::string label = tr(menu->menu()[i].text);
                if (label != menu->menu()[i].text)
                    menu->replace(i, label.c_str());
            }
    }
    if (auto *group = dynamic_cast<Fl_Group *>(&widget))
        for (int i = 0; i < group->children(); ++i)
            localize(*group->child(i));
    widget.redraw();
}
int chooseAction(const char *message, const char *cancel, const char *action, const char *alternative)
{
    return fl_choice("%s", tr(cancel), tr(action), tr(alternative), tr(message));
}
const char *safeInput(const char *message, const char *value)
{
    return fl_input("%s", value, tr(message));
}
const char *safePassword(const char *message, const char *value)
{
    return fl_password("%s", value, tr(message));
}
} // namespace pgjr
