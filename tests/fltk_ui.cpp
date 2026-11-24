#include "window.h"
#include <fltk_cell_editor.h>
#include <FL/Fl.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_PNG_Image.H>
#include <fstream>
#include <iostream>
#include <stdexcept>

void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
pgjr::QueryTab *findTab(Fl_Group &group, const std::string &name = {})
{
    for (int i = 0; i < group.children(); ++i)
    {
        if (auto *tab = dynamic_cast<pgjr::QueryTab *>(group.child(i)))
            if (name.empty() || tab->name == name)
                return tab;
        if (auto *child = dynamic_cast<Fl_Group *>(group.child(i)))
            if (auto *tab = findTab(*child, name))
                return tab;
    }
    return nullptr;
}
// Locate the browser by its isolated profile, independently of the widget layout.
Fl_Tree *findProfileTree(Fl_Group &group, const char *profile)
{
    for (int i = 0; i < group.children(); ++i)
    {
        if (auto *tree = dynamic_cast<Fl_Tree *>(group.child(i)))
            if (tree->find_item(profile))
                return tree;
        if (auto *child = dynamic_cast<Fl_Group *>(group.child(i)))
            if (auto *tree = findProfileTree(*child, profile))
                return tree;
    }
    return nullptr;
}
int main(int argc, char **argv)
{
    try
    {
        Fl::lock();
        auto directory = argc > 1 ? std::filesystem::u8path(argv[1])
                                  : std::filesystem::temp_directory_path() / pgjr::newId();
        std::filesystem::create_directories(directory);
        auto settings = directory / "ui-workspace.json";
        pgjr::Json state = {
            {"profiles", pgjr::Json::array()},
            {"tabs",
             pgjr::Json::array(
                 {{{"profile", ""},
                   {"database", "postgres"},
                   {"name", "Cell renderer check"},
                   {"sql", "-- @resultaat: Rendering check\nSELECT 9007199254740993.125, true, 'hello';"},
                   {"widths", {280, 210, 160, 100}}}})}};
        state["history"] = pgjr::Json::array({{{"sql", "select * from t_object limit 10"},
                                               {"profile", ""},
                                               {"database", "postgres"},
                                               {"plan", false}}});
        pgjr::Profile profile;
        profile.id = "regression-profile";
        profile.name = "Regression server";
        profile.database = "selected_database";
        state["profiles"] = std::vector<pgjr::Profile>{profile};
        pgjr::writeFile(settings, state.dump());
        pgjr::QueryLibrary(settings.parent_path() / "queries").save(
            "Regression query", "select 1;", "Regression/category", {});
        FLTK_Css css;
        require(css.parse("window { background: #edf1f5; color: #203040; } button { background: #e0e8f0; "
                          "color: #203040; border: 1px solid #a0b0c0; border-radius: 4px; } cell { "
                          "background: #ffffff; color: #203040; border: 1px solid #c0ccd8; font-size: 14px; "
                          "} cell.section { background: #c5d7e9; } cell.header { background: #dce6f0; }"),
                "CSS parse");
        pgjr::Window window(css, settings);
        window.refreshLibrary();
        window.refreshLibrary(false);
        auto broken = directory / "broken-workspace.json";
        pgjr::writeFile(broken, "{broken");
        bool protected_file = false;
        try
        {
            pgjr::Window invalid(css, broken);
        }
        catch (...)
        {
            protected_file = true;
        }
        require(protected_file && pgjr::readFile(broken) == "{broken",
                "Invalid workspace is never overwritten on startup");
        auto *tab = findTab(window);
        require(tab && tab->name == "Cell renderer check", "Restored named tab");
        auto *browser = findProfileTree(window, profile.name.c_str());
        require(browser != nullptr, "Isolated profile appears in browser");
        browser->select_only(browser->find_item(profile.name.c_str()), 0);
        window.newQuery({}, {}, {}, "Selected database");
        auto *selected_tab = findTab(window, "Selected database");
        require(selected_tab && selected_tab->profile_id == profile.id &&
                    selected_tab->database == "selected_database" && !selected_tab->session,
                "New query uses browser selection instead of the active tab without connecting");
        window.newQuery(profile.id, "explicit_database", {}, "Explicit database");
        auto *explicit_tab = findTab(window, "Explicit database");
        require(explicit_tab && explicit_tab->database == "explicit_database" && !explicit_tab->session,
                "Explicit query target takes precedence over browser selection");
        window.closeQuery();
        window.closeQuery();
        pgjr::language("nl");
        pgjr::localize(window);
        require(std::string(tab->result_panel->label()) == "Resultaten", "Dutch UI without Qt translations");
        pgjr::language("de");
        pgjr::localize(window);
        require(std::string(tab->result_panel->label()) == "Ergebnisse", "German UI without Qt translations");
        pgjr::language("en");
        pgjr::localize(window);
        auto original =
            std::string("-- @resultaat: Rendering check\nSELECT 9007199254740993.125, true, 'hello';");
        tab->sql.text("alpha alphabet ALPHA");
        tab->editor->insert_position(0);
        require(tab->editor->replace("alpha", "x", true, false, true) == 2,
                "Whole-word replace without changing partial identifiers");
        char *replaced = tab->sql.text();
        require(std::string(replaced) == "x alphabet x", "Replacement contents");
        std::free(replaced);
        tab->sql.text("SELECT 1;\nSELECT 2;");
        tab->sql.select(0, tab->sql.length());
        tab->editor->toggleComment();
        char *commented = tab->sql.text();
        require(std::string(commented) == "-- SELECT 1;\n-- SELECT 2;", "Line comment toggle");
        std::free(commented);
        tab->editor->toggleComment();
        tab->sql.unselect();
        tab->editor->insert_position(6);
        tab->editor->indent();
        char *indented = tab->sql.text();
        require(std::string(indented) == "SELECT     1;\nSELECT 2;", "Tab inserts spaces at the caret");
        std::free(indented);
        tab->sql.text(original.c_str());
        require(!tab->session, "Startup must not connect or execute SQL");
        pgjr::Section section;
        section.title = "Typed values";
        section.columns = {"Text", "Exact numeric", "Binary", "Boolean"};
        section.count = 2;
        section.status = "SELECT 2";
        pgjr::Cell text;
        text.null = false;
        text.type = 25;
        text.text = "Double-click for wrapped cell details";
        pgjr::Cell number = text;
        number.type = 1700;
        number.text = "9007199254740993.125";
        pgjr::Cell binary = text;
        binary.type = 17;
        binary.text.clear();
        binary.binary = {0, 31, 163, 255};
        pgjr::Cell boolean = text;
        boolean.type = 16;
        boolean.text = "t";
        auto null = text;
        null.null = true;
        section.rows = {{text, number, binary, boolean}, {null, number, binary, boolean}};
        tab->result->clear(true);
        tab->result->append(0, section);
        require(tab->result->widths() == std::vector<int>({280, 210, 160, 100}),
                "Column widths survive first result");
        require(tab->result->copyText().find("9007199254740993.125") != std::string::npos,
                "Copy retains exact numeric source");
        section.title = "Second statement";
        tab->result->append(1, section);
        require(tab->result->sections().size() == 2, "Continuous intermediate results");
        tab->result->clear(false);
        tab->result->append(0, section);
        tab->result->append(1, section);
        require(tab->result->sections().size() == 1, "Last result only");
        tab->result->clear(true);
        tab->result->append(0, section);
        tab->result->append(1, section);
        window.show();
        window.resize(window.x(), window.y(), 1200, 820);
        Fl::check();
        Fl_Image_Surface surface(window.w(), window.h());
        Fl_Surface_Device::push_current(&surface);
        surface.draw(&window);
        std::unique_ptr<Fl_RGB_Image> image(surface.image());
        Fl_Surface_Device::pop_current();
        require(image && image->w() == 1200, "Window rendered to image surface");
        require(fl_write_png((directory / "fltk-results.png").u8string().c_str(), image.get()) == 0,
                "Render artifact");
        // One input overlay, independent of the drawer/data. Cancellation is delivered once.
        int before = tab->result->children(), cancelled = 0;
        fltk_cell::TableCellTextEditor editor;
        editor.begin(*tab->result, {300, 500, 160, 28}, std::string("test"),
                     [&](auto value)
                     {
                         if (!value)
                             ++cancelled;
                     });
        require(editor.active() && tab->result->children() == before + 1, "Single editor overlay");
        editor.cancel();
        require(cancelled == 1 && !editor.active() && tab->result->children() == before,
                "Editor cancellation/lifetime");
        Fl::check();
        window.exit();
        require(pgjr::Json::parse(pgjr::readFile(settings))["tabs"][0]["widths"][0] == 280,
                "Workspace width persistence");
        std::cout << "FLTK UI contracts passed; rendered " << (directory / "fltk-results.png").u8string()
                  << '\n';
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
