#include "window.h"
#include "legacy_workspace.h"
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <algorithm>
#include <cctype>
#include <set>
namespace pgjr
{
void Window::importQtWorkspace()
{
    Fl_Native_File_Chooser chooser;
    chooser.title("Import former Qt workspace");
    chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.filter("Qt workspace\t*.ini");
    if (chooser.show())
        return;
    auto source = std::filesystem::u8path(chooser.filename());
    if (chooseAction(
            "Import profiles, tabs and saved queries? Existing tabs are kept, query-file collisions are "
            "skipped and no SQL runs. Old settings remain untouched. Enter passwords again when connecting.",
            "Cancel", "Import", nullptr) != 1)
        return;
    try
    {
        auto state = readQtWorkspace(source);
        auto imported = state["profiles"].get<std::vector<Profile>>();
        auto categories = readQtCategories(source.parent_path() / "queries/categories.ini");
        save();
        std::map<std::string, std::string> ids;
        for (auto p : imported)
        {
            auto old = p.id;
            if (p.id.empty() || profile(p.id))
                p.id = newId();
            ids[old] = p.id;
            profiles_.push_back(p);
            auto *item = tree_->add(tree_->root(), p.name.c_str());
            nodes_[item] = {0, p.id, p.database, "", "", false};
        }
        for (const auto &entry : state["tabs"])
        {
            auto old = entry.value("profile", "");
            newQuery(ids.count(old) ? ids[old] : "", entry.value("database", "postgres"),
                     entry.value("sql", ""), entry.value("name", "Query"));
            current()->path = entry.value("path", "");
            current()->saved_name = entry.value("library", "");
            current()->bom = entry.value("bom", false);
            current()->dirty = entry.value("dirty", true);
            if (!current()->saved_name.empty())
            {
                current()->path.clear();
                current()->bom = false;
            }
            current()->rename(current()->name);
            current()->result->widths(entry.value("widths", std::vector<int>{}));
            current()->editor->insert_position(
                std::clamp(entry.value("cursor", 0), 0, current()->sql.length()));
        }
        QueryLibrary library(settings_.parent_path() / "queries");
        std::set<std::string> existing;
        for (const auto &entry : library.entries())
            existing.insert(entry.name);
        auto directory = source.parent_path() / "queries";
        int copied = 0, skipped = 0;
        if (std::filesystem::exists(directory))
            for (const auto &file : std::filesystem::directory_iterator(directory))
                if (file.is_regular_file() && file.path().extension() == ".sql")
                {
                    auto name = file.path().stem().u8string();
                    if (existing.count(name) || !library.existingName(name).empty())
                    {
                        ++skipped;
                        continue;
                    }
                    QueryLibrary::validate(name);
                    library.save(name, readFile(file.path()), categories[name]);
                    existing.insert(name);
                    ++copied;
                }
        for (const auto &entry : state["history"])
        {
            if (history_.size() == 100)
                break;
            auto history = entry;
            auto old = history.value("profile", "");
            history["profile"] = ids.count(old) ? ids[old] : "";
            history_.push_back(std::move(history));
        }
        refreshLibrary();
        save();
        tree_->redraw();
        fl_message("Imported %d query files; skipped %d existing files. No SQL was executed.", copied,
                   skipped);
    }
    catch (const std::exception &e)
    {
        fl_alert("Import failed: %s", e.what());
    }
}
void Window::refreshLibrary(bool reload)
{
    library_items_.clear();
    history_items_.clear();
    library_tree_->clear();
    history_tree_->clear();
    // clear() also deletes the root; parent-based add() requires a live root.
    library_tree_->root(new Fl_Tree_Item(library_tree_));
    history_tree_->root(new Fl_Tree_Item(history_tree_));
    auto lower = [](std::string value)
    {
        for (char &c : value)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return value;
    };
    auto filter = lower(library_filter_->value());
    try
    {
        std::map<std::string, Fl_Tree_Item *> groups;
        if (reload)
            library_cache_ = QueryLibrary(settings_.parent_path() / "queries").entries();
        for (const auto &entry : library_cache_)
        {
            if (!filter.empty() && lower(entry.name + " " + entry.category).find(filter) == std::string::npos)
                continue;
            auto category = entry.category.empty() ? "Uncategorized" : entry.category;
            auto *group = groups[category];
            if (!group)
            {
                group = library_tree_->add(library_tree_->root(), category.c_str());
                groups[category] = group;
            }
            auto *item = library_tree_->add(group, entry.name.c_str());
            library_items_[item] = entry;
        }
    }
    catch (const std::exception &e)
    {
        status_->copy_label(e.what());
    }
    for (std::size_t i = 0; i < history_.size(); ++i)
    {
        auto text = history_[i].value("sql", "");
        std::replace(text.begin(), text.end(), '\n', ' ');
        if (text.size() > 65)
        {
            std::size_t end = 65;
            while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
                --end;
            text = text.substr(0, end) + "...";
        }
        auto label = std::to_string(i + 1) + ". " + text;
        auto *item = history_tree_->add(history_tree_->root(), label.c_str());
        history_items_[item] = history_[i];
    }
    library_tree_->redraw();
    history_tree_->redraw();
}
void Window::libraryAction()
{
    auto item = library_items_.find(library_tree_->first_selected_item());
    if (item == library_items_.end())
    {
        fl_message("Select a saved query first.");
        return;
    }
    auto entry = item->second;
    int action = chooseAction(entry.name.c_str(), "Cancel", "Rename / category", "Delete");
    try
    {
        QueryLibrary library(settings_.parent_path() / "queries");
        if (action == 1)
        {
            auto *value = safeInput("Query name:", entry.name.c_str());
            if (!value)
                return;
            std::string name = value;
            auto *category = safeInput("Category:", entry.category.c_str());
            if (!category)
                return;
            std::string group = category;
            if (name != entry.name)
                library.rename(entry.name, name);
            library.categorize(name, group);
            for (auto &tab : queries_)
                if (tab->saved_name == entry.name)
                    tab->saved_name = name;
        }
        if (action == 2 &&
            chooseAction("Delete the selected saved query file? Open editor tabs are retained.", "Cancel",
                         "Delete", nullptr) == 1)
        {
            library.remove(entry.name);
            for (auto &tab : queries_)
                if (tab->saved_name == entry.name)
                    tab->saved_name.clear();
        }
        refreshLibrary();
    }
    catch (const std::exception &e)
    {
        fl_alert("%s", e.what());
    }
}
void Window::searchReplace()
{
    auto *tab = current();
    if (!tab)
        return;
    Fl_Double_Window dialog(550, 210, "Find and replace");
    Fl_Input needle(110, 15, 420, 28, "Find"), replacement(110, 50, 420, 28, "Replace with");
    Fl_Check_Button match(110, 86, 180, 25, "Match case"), whole(300, 86, 230, 25, "Whole words");
    Fl_Button previous(12, 130, 90, 30, "Previous"), next(109, 130, 90, 30, "Next"),
        one(206, 130, 90, 30, "Replace"), all(303, 130, 110, 30, "Replace all"),
        close(420, 130, 110, 30, "Close");
    Fl_Box status(12, 170, 518, 25);
    struct Search
    {
        QueryTab *tab;
        Fl_Input *needle, *replacement;
        Fl_Check_Button *match, *whole;
        Fl_Box *status;
    };
    Search state{tab, &needle, &replacement, &match, &whole, &status};
    next.callback(
        [](Fl_Widget *, void *p)
        {
            auto *s = static_cast<Search *>(p);
            s->status->copy_label(
                s->tab->editor->find(s->needle->value(), false, s->match->value(), s->whole->value())
                    ? "Match selected"
                    : "No match");
        },
        &state);
    previous.callback(
        [](Fl_Widget *, void *p)
        {
            auto *s = static_cast<Search *>(p);
            s->status->copy_label(
                s->tab->editor->find(s->needle->value(), true, s->match->value(), s->whole->value())
                    ? "Match selected"
                    : "No match");
        },
        &state);
    one.callback(
        [](Fl_Widget *, void *p)
        {
            auto *s = static_cast<Search *>(p);
            auto count = s->tab->editor->replace(s->needle->value(), s->replacement->value(), false,
                                                 s->match->value(), s->whole->value());
            s->status->copy_label((std::to_string(count) + " replacements").c_str());
        },
        &state);
    all.callback(
        [](Fl_Widget *, void *p)
        {
            auto *s = static_cast<Search *>(p);
            auto count = s->tab->editor->replace(s->needle->value(), s->replacement->value(), true,
                                                 s->match->value(), s->whole->value());
            s->status->copy_label((std::to_string(count) + " replacements").c_str());
        },
        &state);
    close.callback([](Fl_Widget *, void *p) { static_cast<Fl_Window *>(p)->hide(); }, &dialog);
    dialog.end();
    dialog.set_modal();
    localize(dialog);
    dialog.show();
    while (dialog.shown())
        Fl::wait();
}
} // namespace pgjr
