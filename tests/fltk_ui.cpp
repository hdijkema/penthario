#include "window.h"
#include "widgets.h"
#include <fltk_cell_editor.h>
#include <FL/Fl.H>
#include <FL/Fl_Image_Surface.H>
#include <FL/Fl_PNG_Image.H>
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

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
// Dispatch mouse events only inside our own test window, never to the desktop.
void mouse(Fl_Window &window, int event, int x, int y, int button = FL_LEFT_MOUSE, int clicks = 0)
{
    Fl::e_x = x;
    Fl::e_y = y;
    Fl::e_x_root = window.x_root() + x;
    Fl::e_y_root = window.y_root() + y;
    Fl::e_keysym = FL_Button + button;
    Fl::e_state = event == FL_RELEASE ? 0 : button == FL_RIGHT_MOUSE ? FL_BUTTON3 : FL_BUTTON1;
    Fl::e_clicks = clicks;
    Fl::e_is_click = event == FL_PUSH;
    Fl::handle(event, &window);
}
void snapshot(Fl_Window &window, const std::filesystem::path &file)
{
    Fl_Image_Surface surface(window.w(), window.h());
    Fl_Surface_Device::push_current(&surface);
    surface.draw(&window);
    std::unique_ptr<Fl_RGB_Image> image(surface.image());
    Fl_Surface_Device::pop_current();
    require(image && fl_write_png(file.u8string().c_str(), image.get()) == 0, "UI render artifact");
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
        pgjr::QueryLibrary(settings.parent_path() / "queries")
            .save("Regression query", "select 1;", "Regression/category", {});
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
        require(std::string(tab->result_panel->label()) == "Resultaten", "Dutch native translation catalog");
        pgjr::language("de");
        pgjr::localize(window);
        require(std::string(tab->result_panel->label()) == "Ergebnisse", "German native translation catalog");
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
        require(tab->result->columnLabel(0) == "Text" && tab->result->rows() == 3,
                "A single result starts with named column headers and data, without duplicate headings");
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
        // Verify the real widget hierarchy dispatches native table header drags.
        tab->result->clear(true);
        tab->result->append(0, section);
        const int header_x =
            tab->result->x() + Fl::box_dx(tab->result->box()) + tab->result->col_width(0) - 7;
        const int header_y = tab->result->y() + Fl::box_dy(tab->result->box()) + 14;
        const int old_width = tab->result->col_width(0);
        mouse(window, FL_MOVE, header_x, header_y);
        mouse(window, FL_MOVE, header_x + 1, header_y);
#ifdef _WIN32
        require(GetCursor() == LoadCursorW(nullptr, MAKEINTRESOURCEW(32644)),
                "Resize cursor survives consecutive moves through the parent hierarchy");
#endif
        mouse(window, FL_PUSH, header_x, header_y);
        mouse(window, FL_DRAG, header_x + 60, header_y);
        mouse(window, FL_RELEASE, header_x + 60, header_y);
        require(tab->result->col_width(0) > old_width + 40,
                "Column header border can be resized with the mouse");
        tab->result->widths({280, 210, 160, 100});
        const int right_grip = header_x + 14;
        mouse(window, FL_PUSH, right_grip, header_y);
        mouse(window, FL_DRAG, right_grip + 35, header_y);
        mouse(window, FL_RELEASE, right_grip + 35, header_y);
        require(tab->result->col_width(0) == 315, "Wide grip works on both sides of a column border");
        tab->result->widths({280, 210, 160, 100});
        int inspections = 0;
        auto inspect = tab->result->inspect;
        tab->result->inspect = [&](const auto &) { ++inspections; };
        const int cell_x = tab->result->x() + 20,
                  cell_y = tab->result->y() + tab->result->col_header_height() + 14;
        mouse(window, FL_PUSH, cell_x, cell_y, FL_LEFT_MOUSE, 1);
        require(inspections == 0, "Double-click press does not open a cell dialog");
        mouse(window, FL_RELEASE, cell_x, cell_y, FL_LEFT_MOUSE, 1);
        require(inspections == 1, "Double-click opens cell details exactly once");
        tab->result->inspect = inspect;
        const int old_editor = tab->editor->h();
        const int split_x = tab->splitter->x() + tab->splitter->w() / 2, split_y = tab->splitter->y() + 2;
        mouse(window, FL_PUSH, split_x, split_y);
        mouse(window, FL_DRAG, split_x, split_y - 65);
        mouse(window, FL_RELEASE, split_x, split_y - 65);
        require(tab->editor->h() < old_editor - 40 && tab->result->h() > 100,
                "Dragging the splitter gives results more height");
        const auto split = tab->splitRatio();
        window.resize(window.x(), window.y(), 1100, 760);
        require(tab->splitRatio() == split, "Query split ratio survives window resize");
        window.resize(window.x(), window.y(), 1200, 820);
        auto original_callback = browser->callback();
        auto *original_data = browser->user_data();
        int contexts = 0;
        browser->callback([](Fl_Widget *, void *data) { ++*static_cast<int *>(data); }, &contexts);
        auto *profile_item = browser->find_item(profile.name.c_str());
        const int tree_x = profile_item->x() + 80, tree_y = profile_item->y() + profile_item->h() / 2;
        mouse(window, FL_PUSH, tree_x, tree_y, FL_RIGHT_MOUSE);
        mouse(window, FL_RELEASE, tree_x, tree_y, FL_RIGHT_MOUSE);
        require(contexts == 1 && browser->callback_item() == profile_item,
                "Right-click forwards the clicked tree item once, even when already selected");
        browser->callback(original_callback, original_data);
        struct PopupProbe
        {
            bool seen = false;
            int x = 0, y = 0;
        } probe;
        auto observe_popup = +[](void *data)
        {
            auto &probe = *static_cast<PopupProbe *>(data);
            if (auto *popup = Fl::grab())
            {
                probe.seen = true;
                probe.x = popup->x_root();
                probe.y = popup->y_root();
                Fl::e_keysym = FL_Escape;
                Fl::e_state = 0;
                Fl::handle(FL_KEYBOARD, popup);
            }
        };
        Fl::add_timeout(0.05, observe_popup, &probe);
        mouse(window, FL_PUSH, tree_x, tree_y, FL_RIGHT_MOUSE);
        Fl::remove_timeout(observe_popup, &probe);
        mouse(window, FL_RELEASE, tree_x, tree_y, FL_RIGHT_MOUSE);
        require(probe.seen && std::abs(probe.x - (window.x_root() + tree_x)) <= 2 &&
                    std::abs(probe.y - (window.y_root() + tree_y)) <= 2,
                "Real context menu opens at the clicked screen position without doubling window offset");
        auto *menu = dynamic_cast<Fl_Menu_Bar *>(window.child(0));
        require(menu, "Main menu exists");
        std::vector<std::string> top_menus;
        for (auto *item = menu->menu(); item && item->text; item = item->next())
            top_menus.emplace_back(item->text);
        require(top_menus == std::vector<std::string>({"File", "Edit", "Query", "Results", "Connection",
                                                       "Database", "View", "Help"}),
                "Main menu order places editing near files and Help last");
        require(menu->find_item("Query/Saved queries/Manage...") && menu->find_item("View/Language/English"),
                "Saved query management and language options are grouped");
        int icon_buttons = 0;
        for (int n = 0; n < window.children(); ++n)
            if (auto *button = dynamic_cast<Fl_Button *>(window.child(n)))
                if (button->image())
                {
                    ++icon_buttons;
                    require((!button->label() || !*button->label()) && button->tooltip(),
                            "Toolbar buttons have an icon and tooltip without text");
                }
        require(icon_buttons == 5, "All five toolbar actions remain available");
        tab->output->value(tab->plan_panel);
        Fl::check();
        for (auto *option : {tab->analyze, tab->buffers, tab->verbose, tab->costs, tab->timing})
            require(option->y() >= tab->plan_panel->y() + 4 &&
                        option->x() + option->w() <= tab->plan_panel->x() + tab->plan_panel->w(),
                    "Explain options stay inside the content panel below its tab strip");
        require(tab->plan_view->y() > tab->format->y() + tab->format->h(),
                "Plan output does not overlap the options");
        snapshot(window, directory / "fltk-plan.png");
        tab->output->value(tab->result_panel);
        snapshot(window, directory / "fltk-single-result.png");
        tab->result->append(1, section);
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
        const auto split_before_theme = tab->splitRatio();
        auto *sql_before_theme = tab->sql.text();
        const std::string preserved_sql = sql_before_theme;
        std::free(sql_before_theme);
        fltk_dock::Host *dock = nullptr;
        for (int n = 0; n < window.children(); ++n)
            if (auto *host = dynamic_cast<fltk_dock::Host *>(window.child(n)))
                dock = host;
        require(dock, "Dock host exists for theme propagation");
        fltk_dock::Panel *floating = nullptr;
        std::function<void(Fl_Group &)> find_panel = [&](Fl_Group &group)
        {
            for (int n = 0; n < group.children(); ++n)
            {
                if (auto *panel = dynamic_cast<fltk_dock::Panel *>(group.child(n)))
                    if (panel->id() == "library")
                        floating = panel;
                if (auto *child = group.child(n)->as_group())
                    find_panel(*child);
            }
        };
        find_panel(*dock);
        require(floating, "Library dock panel exists");
        const auto layout = dock->save_layout();
        dock->float_panel(*floating, window.x_root() + 360, window.y_root() + 140);
        window.useTheme("dark");
        require(window.theme().name() == "dark" &&
                    tab->editor->color() == window.theme().style("editor").background,
                "Dark CSS updates the native SQL editor");
        require(browser->item_labelfgcolor() == window.theme().style("tree").color &&
                    floating->labelcolor() == window.theme().style("panel").color &&
                    floating->window()->color() == window.theme().style("window").background,
                "Trees and native floating panels receive the same palette");
        require(tab->splitRatio() == split_before_theme, "Changing theme preserves splitter geometry");
        char *sql_after_theme = tab->sql.text();
        require(std::string(sql_after_theme) == preserved_sql, "Changing theme preserves SQL contents");
        std::free(sql_after_theme);
        dock->restore_layout(layout);
        Fl::check();
        snapshot(window, directory / "theme-dark.png");
        const auto custom = directory / "custom-theme.css";
        pgjr::writeFile(custom, "editor { background: #142337; } cell.header { background: #445566; }");
        window.useTheme("dark", custom);
        require(tab->editor->color() == fl_rgb_color(20, 35, 55),
                "Local CSS overrides the selected base theme");
        const auto invalid_css = directory / "invalid-theme.css";
        pgjr::writeFile(invalid_css, "button { unsupported-property: yes; }");
        bool rejected_css = false;
        try
        {
            window.useTheme("light", invalid_css);
        }
        catch (const std::exception &)
        {
            rejected_css = true;
        }
        require(rejected_css && window.theme().name() == "dark" && window.theme().file() == custom &&
                    tab->editor->color() == fl_rgb_color(20, 35, 55),
                "Invalid CSS leaves the active theme, palette and preference intact");
        pgjr::writeFile(custom, "editor { background: #162839; }");
        window.reloadThemeCss();
        require(tab->editor->color() == fl_rgb_color(22, 40, 57), "Reload CSS applies edited local styles");
        window.useTheme("classic");
        require(window.theme().style("button").border_radius == 0, "Classic theme has square button borders");
        Fl::check();
        snapshot(window, directory / "theme-classic.png");
        window.useTheme("light");
        Fl::check();
        snapshot(window, directory / "theme-light.png");
        window.useTheme("dark", custom);
        {
            FLTK_Css restored_css;
            pgjr::Window restored(restored_css, settings, true);
            auto *restored_tab = findTab(restored);
            require(restored.theme().name() == "dark" && restored.theme().file() == custom && restored_tab &&
                        restored_tab->editor->color() == fl_rgb_color(22, 40, 57),
                    "Theme base and CSS path are restored from the isolated workspace");
        }
        window.exit();
        require(pgjr::Json::parse(pgjr::readFile(settings))["tabs"][0]["widths"][0] == 280,
                "Workspace width persistence");
        require(pgjr::Json::parse(pgjr::readFile(settings))["tabs"][0]["split"].get<double>() ==
                    tab->splitRatio(),
                "Query splitter position is persisted");
        std::cout << "FLTK UI contracts passed; rendered " << (directory / "fltk-results.png").u8string()
                  << '\n';
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
