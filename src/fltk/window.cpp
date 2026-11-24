#include "window.h"
#include "assets.h"
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_SVG_Image.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <sstream>
#include <iomanip>

namespace pgjr
{
namespace
{
std::string selected(Fl_Text_Buffer &buffer)
{
    char *raw = buffer.selection_text();
    std::string out = raw ? raw : "";
    std::free(raw);
    return out;
}
std::string contents(Fl_Text_Buffer &buffer)
{
    char *raw = buffer.text();
    std::string out = raw ? raw : "";
    std::free(raw);
    return out;
}
std::string chooseFile(bool save, const char *title, const char *filter)
{
    Fl_Native_File_Chooser chooser;
    chooser.title(title);
    chooser.type(save ? Fl_Native_File_Chooser::BROWSE_SAVE_FILE : Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.filter(filter);
    if (save)
        chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    return chooser.show() == 0 && chooser.filename() ? chooser.filename() : "";
}
void error(const std::exception &ex)
{
    fl_alert("%s", ex.what());
}
Fl_Text_Display::Style_Table_Entry styleTable[] = {{FL_BLACK, FL_COURIER, 14},
                                                   {fl_rgb_color(25, 75, 160), FL_COURIER_BOLD, 14},
                                                   {fl_rgb_color(30, 130, 70), FL_COURIER, 14},
                                                   {fl_rgb_color(150, 70, 30), FL_COURIER, 14}};
} // namespace

QueryTab::QueryTab(Window &owner, FLTK_Css &css, int x, int y, int w, int h)
    : Fl_Group(x, y, w, h), app(owner)
{
    target = new Fl_Box(x + 6, y + 2, w - 12, 26);
    target->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    editor = new SqlEditor(x, y + 30, w, h / 2 - 30);
    editor->buffer(&sql);
    editor->highlight_data(&styles, styleTable, 4, 'A', nullptr, nullptr);
    output = new fltk_dock::Tabs(x, y + h / 2, w, h / 2);
    result_panel = new Fl_Group(x, y + h / 2 + 25, w, h / 2 - 25, "Results");
    all = new Fl_Check_Button(x + 5, y + h / 2 + 30, 180, 25, "Intermediate results");
    all->value(1);
    result = new ResultTable(x, y + h / 2 + 60, w, h / 2 - 60, css);
    result->inspect = [this](const Cell &cell) { app.detail(cell); };
    result_panel->end();
    message_panel = new Fl_Group(x, y + h / 2 + 25, w, h / 2 - 25, "Messages");
    message_view = new Fl_Text_Display(x, y + h / 2 + 25, w, h / 2 - 25);
    message_view->buffer(&messages);
    message_view->textfont(FL_COURIER);
    message_panel->end();
    plan_panel = new Fl_Group(x, y + h / 2 + 25, w, h / 2 - 25, "Execution plan");
    analyze = new Fl_Check_Button(x, y + h / 2 + 28, 105, 26, "ANALYZE");
    analyze->tooltip("Actually executes SQL. Plan-mode changes are rolled back; sequence and external "
                     "effects may persist.");
    buffers = new Fl_Check_Button(x + 105, y + h / 2 + 28, 100, 26, "BUFFERS");
    verbose = new Fl_Check_Button(x + 205, y + h / 2 + 28, 100, 26, "VERBOSE");
    costs = new Fl_Check_Button(x + 305, y + h / 2 + 28, 85, 26, "COSTS");
    costs->value(1);
    timing = new Fl_Check_Button(x + 390, y + h / 2 + 28, 95, 26, "TIMING");
    timing->value(1);
    format = new Fl_Choice(x + 490, y + h / 2 + 28, 110, 26);
    format->add("TEXT|JSON|XML|YAML");
    format->value(0);
    plan_view = new Fl_Text_Display(x, y + h / 2 + 60, w, h / 2 - 60);
    plan_view->buffer(&plan);
    plan_view->textfont(FL_COURIER);
    plan_panel->end();
    output->end();
    end();
    sql.add_modify_callback(
        [](int, int inserted, int deleted, int, const char *, void *owner)
        {
            auto *tab = static_cast<QueryTab *>(owner);
            if (inserted || deleted)
            {
                tab->dirty = true;
                tab->rename(tab->name);
            }
            tab->highlight();
        },
        this);
}
QueryTab::~QueryTab()
{
    session.reset();
    editor->buffer(nullptr);
    message_view->buffer(nullptr);
    plan_view->buffer(nullptr);
    clear(); // Child widgets must die before their member text buffers.
}
void QueryTab::rename(std::string value)
{
    name = std::move(value);
    copy_label((name + (dirty ? " *" : "") + " [" + database + "]").c_str());
}
bool QueryTab::planSelected() const
{
    return output->value() == plan_panel;
}
void QueryTab::log(const std::string &value)
{
    if (messages.length() > 1024 * 1024)
        messages.text("[Earlier messages omitted]\n");
    auto end = std::min<std::size_t>(value.size(), 65536);
    while (end && end < value.size() && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80)
        --end;
    messages.append((value.substr(0, end) + (end < value.size() ? " ..." : "") + '\n').c_str());
    message_view->insert_position(messages.length());
    message_view->show_insert_position();
}
void QueryTab::resize(int x, int y, int w, int h)
{
    Fl_Group::resize(x, y, w, h);
    int middle = y + std::max(120, h * 45 / 100);
    target->resize(x + 6, y + 2, w - 12, 26);
    editor->resize(x, y + 30, w, middle - y - 34);
    output->resize(x, middle, w, y + h - middle);
    for (auto *group : {result_panel, message_panel, plan_panel})
        group->resize(x, middle + 30, w, y + h - middle - 30);
    all->resize(x + 5, middle + 33, 190, 26);
    result->resize(x, middle + 62, w, y + h - middle - 62);
    message_view->resize(x, middle + 31, w, y + h - middle - 31);
    analyze->resize(x, middle + 28, 105, 26);
    buffers->resize(x + 105, middle + 28, 100, 26);
    verbose->resize(x + 205, middle + 28, 100, 26);
    costs->resize(x + 305, middle + 28, 85, 26);
    timing->resize(x + 390, middle + 28, 95, 26);
    if (w < 610)
    {
        analyze->resize(x, middle + 28, 95, 26);
        buffers->resize(x + 95, middle + 28, 95, 26);
        verbose->resize(x + 190, middle + 28, 95, 26);
        costs->resize(x + 285, middle + 28, 85, 26);
        timing->resize(x + 370, middle + 28, 95, 26);
        format->resize(x, middle + 56, 110, 26);
        plan_view->resize(x, middle + 86, w, y + h - middle - 86);
    }
    else
    {
        format->resize(x + 490, middle + 28, 110, 26);
        plan_view->resize(x, middle + 58, w, y + h - middle - 58);
    }
}
// Style SQL byte ranges without changing the original UTF-8 buffer or undo history.
void QueryTab::highlight()
{
    auto text = contents(sql);
    std::string marks(text.size(), 'A');
    static const std::set<std::string> keywords = {
        "SELECT",   "FROM",   "WHERE",    "JOIN",   "LEFT", "RIGHT",  "INNER",   "OUTER",
        "ON",       "AS",     "WITH",     "INSERT", "INTO", "UPDATE", "DELETE",  "VALUES",
        "CREATE",   "DROP",   "ALTER",    "TABLE",  "VIEW", "INDEX",  "EXPLAIN", "ANALYZE",
        "BEGIN",    "COMMIT", "ROLLBACK", "ORDER",  "BY",   "GROUP",  "HAVING",  "LIMIT",
        "OFFSET",   "NULL",   "NOT",      "AND",    "OR",   "TRUE",   "FALSE",   "RETURNING",
        "DISTINCT", "UNION",  "ALL",      "CASE",   "WHEN", "THEN",   "ELSE",    "END"};
    for (std::size_t i = 0; i < text.size();)
    {
        auto start = i;
        if (text.compare(i, 2, "--") == 0)
        {
            i = text.find('\n', i);
            if (i == std::string::npos)
                i = text.size();
            std::fill(marks.begin() + start, marks.begin() + i, 'C');
        }
        else if (text.compare(i, 2, "/*") == 0)
        {
            int depth = 1;
            i += 2;
            while (i < text.size() && depth)
            {
                if (text.compare(i, 2, "/*") == 0)
                {
                    ++depth;
                    i += 2;
                }
                else if (text.compare(i, 2, "*/") == 0)
                {
                    --depth;
                    i += 2;
                }
                else
                    ++i;
            }
            std::fill(marks.begin() + start, marks.begin() + i, 'C');
        }
        else if (text[i] == '\'' || text[i] == '"')
        {
            char quote = text[i++];
            while (i < text.size())
            {
                if (text[i++] == quote)
                {
                    if (i < text.size() && text[i] == quote)
                    {
                        ++i;
                        continue;
                    }
                    break;
                }
            }
            std::fill(marks.begin() + start, marks.begin() + i, 'D');
        }
        else if (std::isalpha(static_cast<unsigned char>(text[i])) || text[i] == '_')
        {
            std::string word;
            while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_'))
                word += static_cast<char>(std::toupper(static_cast<unsigned char>(text[i++])));
            if (keywords.count(word))
                std::fill(marks.begin() + start, marks.begin() + i, 'B');
        }
        else
            ++i;
    }
    styles.text(marks.c_str());
    editor->redisplay_range(0, sql.length());
}

Window::Window(FLTK_Css &stylesheet, std::filesystem::path settings, bool smoke)
    : Fl_Css_Window(stylesheet, 100, 100, 1440, 820, "pgadmin-jr"), css(stylesheet),
      settings_(std::move(settings)), smoke_(smoke)
{
    menu_ = new Fl_Menu_Bar(0, 0, w(), 28);
    auto action = [this](const char *name, int shortcut, Fl_Callback *callback)
    { menu_->add(name, shortcut, callback, this); };
    action("File/New query", FL_CTRL + 'n',
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->newQuery(); });
    action("File/Open SQL...", FL_CTRL + 'o',
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->openSql(); });
    action("File/Import Qt workspace...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->importQtWorkspace(); });
    action("File/Save SQL...", FL_CTRL + 's',
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->saveSql(); });
    action("File/Save named query...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->saveNamed(); });
    action("File/Open named query...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->openNamed(); });
    action("File/Refresh saved queries", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->refreshLibrary(); });
    action("File/Rename tab...", FL_F + 2,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->renameQuery(); });
    action("File/Close tab", FL_CTRL + 'w',
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->closeQuery(); });
    action("File/Exit", FL_ALT + FL_F + 4, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->exit(); });
    action("Connection/New profile...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->editProfile(); });
    action("Connection/Connect", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->connectProfile(); });
    action("Connection/Edit profile...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->editSelectedProfile(); });
    action("Connection/Disconnect", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->disconnect(); });
    action("Connection/Remove profile...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->deleteProfile(); });
    action("Connection/Use selected database for this tab", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->useSelectedDatabase(); });
    action("Database/Backup...", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->databaseTask(0); });
    action("Database/Restore...", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->databaseTask(1); });
    action("Database/Create...", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->databaseTask(2); });
    action("Database/Drop...", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->databaseTask(3); });
    action("Database/VACUUM...", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->databaseTask(4); });
    action("Database/ANALYZE...", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->databaseTask(5); });
    action("Database/REINDEX...", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->databaseTask(6); });
    action("Database/Server roles and privileges...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->manageRoles(); });
    action("Database/Inspect object", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->inspectObject(); });
    action("Database/Refresh browser", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->refreshCatalog(); });
    action("Query/Run selection or script", FL_F + 5,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->execute(); });
    action("Query/Cancel", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->cancelQuery(); });
    action("Query/Find...", FL_CTRL + 'f', [](Fl_Widget *, void *p) { static_cast<Window *>(p)->find(); });
    action("Query/Find and replace...", FL_CTRL + 'h',
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->searchReplace(); });
    action("Query/Complete", FL_CTRL + ' ',
           [](Fl_Widget *, void *p)
           {
               auto *t = static_cast<Window *>(p)->current();
               if (t)
                   t->editor->complete();
           });
    action("Query/Toggle line comment", FL_CTRL + '/',
           [](Fl_Widget *, void *p)
           {
               auto *t = static_cast<Window *>(p)->current();
               if (t)
                   t->editor->toggleComment();
           });
    action("File/Manage saved query...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->libraryAction(); });
    action("Results/Copy all", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->copyAll(); });
    action("Results/Save preview as CSV...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->exportCsv(); });
    action("Results/Save preview as XLSX...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->exportExcel(); });
    action("Results/Execute to full CSV...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->execute(true); });
    action("Results/Execute to full XLSX...", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->execute(true, true); });
    action("Help/About", 0, [](Fl_Widget *, void *p) { static_cast<Window *>(p)->about(); });
    action("Language/Nederlands", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->changeLanguage("nl"); });
    action("Language/English", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->changeLanguage("en"); });
    action("Language/Deutsch", 0,
           [](Fl_Widget *, void *p) { static_cast<Window *>(p)->changeLanguage("de"); });
    connect_ = new Fl_Css_Button(css, 8, 35, 100, 30, "Connect");
    connect_->callback([](Fl_Widget *, void *p) { static_cast<Window *>(p)->connectProfile(); }, this);
    auto *profileButton = new Fl_Css_Button(css, 115, 35, 100, 30, "New profile");
    profileButton->callback([](Fl_Widget *, void *p) { static_cast<Window *>(p)->editProfile(); }, this);
    auto *newButton = new Fl_Css_Button(css, 222, 35, 100, 30, "New query");
    newButton->callback([](Fl_Widget *, void *p) { static_cast<Window *>(p)->newQuery(); }, this);
    run_ = new Fl_Css_Button(css, 329, 35, 110, 30, "Run (F5)");
    run_->callback([](Fl_Widget *, void *p) { static_cast<Window *>(p)->execute(); }, this);
    cancel_ = new Fl_Css_Button(css, 446, 35, 90, 30, "Cancel");
    cancel_->callback([](Fl_Widget *, void *p) { static_cast<Window *>(p)->cancelQuery(); }, this);
    auto icon = [this](Fl_Button *button, const char *source)
    {
        Fl_SVG_Image svg(nullptr, source);
        icons_.emplace_back(svg.copy(18, 18));
        button->image(icons_.back().get());
        button->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_IMAGE_NEXT_TO_TEXT);
    };
    icon(connect_, connectSvg);
    icon(profileButton, profileSvg);
    icon(newButton, newSvg);
    icon(run_, runSvg);
    icon(cancel_, cancelSvg);
    dock_ = new fltk_dock::Host(8, 74, w() - 16, h() - 104);
    tree_ = new Fl_Tree(8, 74, 260, h() - 104);
    tree_->showroot(0);
    tree_->callback([](Fl_Widget *, void *p) { static_cast<Window *>(p)->treeClick(); }, this);
    tabs_ = new fltk_dock::Tabs(278, 74, w() - 526, h() - 104);
    tabs_->end();
    auto *library = new Fl_Group(w() - 236, 101, 226, h() - 132, "Queries");
    library_filter_ = new Fl_Input(w() - 230, 108, 214, 27);
    library_filter_->tooltip("Filter saved queries");
    library_filter_->when(FL_WHEN_CHANGED);
    library_filter_->callback([](Fl_Widget *, void *p) { static_cast<Window *>(p)->refreshLibrary(false); },
                              this);
    library_tree_ = new Fl_Tree(w() - 234, 141, 222, h() - 175);
    library_tree_->showroot(0);
    library_tree_->callback(
        [](Fl_Widget *, void *p)
        {
            auto *app = static_cast<Window *>(p);
            if (Fl::event_button() == FL_RIGHT_MOUSE)
                app->libraryAction();
            else if (Fl::event_clicks())
                app->openNamed();
        },
        this);
    library->end();
    library->resizable(library_tree_);
    auto *history = new Fl_Group(w() - 236, 101, 226, h() - 132, "History");
    history_tree_ = new Fl_Tree(w() - 234, 108, 222, h() - 142);
    history_tree_->showroot(0);
    history_tree_->callback(
        [](Fl_Widget *, void *p)
        {
            auto *app = static_cast<Window *>(p);
            if (!Fl::event_clicks())
                return;
            auto item = app->history_items_.find(app->history_tree_->callback_item());
            if (item != app->history_items_.end())
            {
                app->newQuery(item->second.value("profile", ""), item->second.value("database", "postgres"),
                              item->second.value("sql", ""), "History");
                if (item->second.value("plan", false))
                    app->current()->output->value(app->current()->plan_panel);
            }
        },
        this);
    history->end();
    history->resizable(history_tree_);
    dock_->center(*tabs_);
    browser_panel_ = &dock_->add_panel("browser", "Databases", *tree_, fltk_dock::Side::Left, 260);
    library_panel_ = &dock_->add_panel("library", "Queries", *library, fltk_dock::Side::Right, 260);
    history_panel_ = &dock_->add_panel("history", "History", *history, fltk_dock::Side::Right, 260);
    dock_->tabify(*history_panel_, *library_panel_);
    static_cast<fltk_dock::Tabs *>(library_panel_->parent())->value(library_panel_);
    action("View/Database browser", 0, [](Fl_Widget *, void *p)
    {
        auto &app = *static_cast<Window *>(p);
        app.dock_->open(*app.browser_panel_);
    });
    action("View/Query library", 0, [](Fl_Widget *, void *p)
    {
        auto &app = *static_cast<Window *>(p);
        app.dock_->open(*app.library_panel_);
    });
    action("View/History", 0, [](Fl_Widget *, void *p)
    {
        auto &app = *static_cast<Window *>(p);
        app.dock_->open(*app.history_panel_);
    });
    action("View/Reset layout", 0, [](Fl_Widget *, void *p)
    {
        auto &app = *static_cast<Window *>(p);
        app.dock_->dock(*app.browser_panel_, fltk_dock::Side::Left);
        app.dock_->dock(*app.library_panel_, fltk_dock::Side::Right);
        app.dock_->tabify(*app.history_panel_, *app.library_panel_);
    });
    status_ = new Fl_Box(8, h() - 26, w() - 16, 22, "Ready - no network connections at startup");
    status_->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    end();
    resizable(nullptr);
    size_range(1000, 560);
    callback([](Fl_Widget *, void *p) { static_cast<Window *>(p)->exit(); }, this);
    load();
    for (const auto &p : profiles_)
    {
        auto *item = tree_->add(tree_->root(), p.name.c_str());
        nodes_[item] = {0, p.id, p.database, "", "", false};
        tree_->close(item, 0);
    }
    if (queries_.empty())
        newQuery();
    refreshLibrary();
    localize(*this);
    Fl::add_timeout(0.05, tick, this);
}
Window::~Window()
{
    Fl::remove_timeout(tick, this);
    catalog_.reset();
    queries_.clear();
}
void Window::resize(int x, int y, int w, int h)
{
    Fl_Css_Window::resize(x, y, w, h);
    menu_->resize(0, 0, w, 28);
    dock_->resize(8, 74, w - 16, h - 104);
    status_->resize(8, h - 26, w - 16, 22);
}
QueryTab *Window::current()
{
    return dynamic_cast<QueryTab *>(tabs_->value());
}
Profile *Window::profile(const std::string &id)
{
    for (auto &p : profiles_)
        if (p.id == id)
            return &p;
    return nullptr;
}
void Window::newQuery(std::string id, std::string database, std::string text, std::string name)
{
    // An explicit target wins; otherwise use the selected browser node before the active tab.
    if (id.empty() && database.empty())
    {
        auto selected = nodes_.find(tree_->first_selected_item());
        if (selected != nodes_.end())
        {
            id = selected->second.profile;
            database = selected->second.database;
        }
    }
    if (id.empty() && database.empty())
    {
        if (auto *tab = current())
        {
            id = tab->profile_id;
            database = tab->database;
        }
        else if (!profiles_.empty())
        {
            id = profiles_.front().id;
            database = profiles_.front().database;
        }
    }
    auto *previous = Fl_Group::current();
    Fl_Group::current(nullptr);
    auto tab = std::make_unique<QueryTab>(*this, css, tabs_->x() + 1, tabs_->y() + 30,
                                          tabs_->w() - 2, tabs_->h() - 31);
    Fl_Group::current(previous);
    tab->profile_id = id;
    tab->database = database.empty() ? "postgres" : database;
    tab->rename(name.empty() ? "Query " + std::to_string(sequence_++) : name);
    tab->sql.text(text.c_str());
    localize(*tab);
    updateTarget(*tab);
    tabs_->add(*tab);
    tab->when(FL_WHEN_CLOSED);
    tab->callback([](Fl_Widget *widget, void *p)
    {
        auto &app = *static_cast<Window *>(p);
        app.tabs_->value(widget);
        app.closeQuery();
    }, this);
    tabs_->value(tab.get());
    tabs_->resize(tabs_->x(), tabs_->y(), tabs_->w(), tabs_->h());
    queries_.push_back(std::move(tab));
    tabs_->redraw();
}
void Window::load()
{
    if (!std::filesystem::exists(settings_))
        return;
    try
    {
        auto state = Json::parse(readFile(settings_));
        profiles_ = state.value("profiles", std::vector<Profile>{});
        history_ = state.value("history", std::vector<Json>{});
        if (history_.size() > 100)
            history_.resize(100);
        for (const auto &entry : state.value("tabs", Json::array()))
        {
            const auto text = entry.value("sql", "");
            if (text.size() > 8 * 1024 * 1024 || text.find('\0') != std::string::npos)
                throw std::runtime_error("Invalid or oversized SQL snapshot in workspace");
            newQuery(entry.value("profile", ""), entry.value("database", "postgres"), entry.value("sql", ""),
                     entry.value("name", ""));
            current()->path = entry.value("path", "");
            current()->saved_name = entry.value("library", "");
            current()->bom = entry.value("bom", false);
            current()->dirty = entry.value("dirty", false);
            if (!current()->path.empty())
            {
                try
                {
                    bool bom = false;
                    auto disk = readSqlFile(std::filesystem::u8path(current()->path), &bom);
                    if (!entry.contains("bom"))
                        current()->bom = bom;
                    if (disk != contents(current()->sql) || bom != current()->bom)
                        current()->dirty = true;
                }
                catch (...)
                {
                    current()->dirty = true;
                }
            }
            current()->rename(current()->name);
            current()->editor->insert_position(
                std::clamp(entry.value("cursor", 0), 0, current()->sql.length()));
            current()->result->widths(entry.value("widths", std::vector<int>{}));
        }
        auto active = state.value("active", 0);
        if (active >= 0 && active < static_cast<int>(queries_.size()))
            tabs_->value(queries_[active].get());
        if (state.contains("docking"))
        {
            std::vector<fltk_dock::PanelState> layout;
            for (const auto &entry : state.at("docking"))
            {
                fltk_dock::PanelState panel;
                panel.id = entry.at("id").get<std::string>();
                panel.side = static_cast<fltk_dock::Side>(entry.at("side").get<int>());
                panel.extent = entry.at("extent").get<int>();
                panel.open = entry.value("open", true);
                panel.floating = entry.value("floating", false);
                panel.x = entry.value("x", 100);
                panel.y = entry.value("y", 100);
                panel.w = entry.value("w", 400);
                panel.h = entry.value("h", 400);
                panel.tab_group = entry.value("tab_group", "");
                panel.active = entry.value("active", false);
                layout.push_back(std::move(panel));
            }
            dock_->restore_layout(layout);
        }
    }
    catch (const std::exception &ex)
    {
        throw std::runtime_error("Workspace was not loaded; the original file was preserved: " +
                                 std::string(ex.what()));
    }
}
void Window::save()
{
    Json state;
    state["profiles"] = profiles_;
    state["tabs"] = Json::array();
    state["history"] = history_;
    state["language"] = language();
    state["docking"] = Json::array();
    for (const auto &panel : dock_->save_layout())
        state["docking"].push_back({{"id", panel.id}, {"side", static_cast<int>(panel.side)},
                                    {"extent", panel.extent}, {"open", panel.open},
                                    {"floating", panel.floating}, {"x", panel.x}, {"y", panel.y},
                                    {"w", panel.w}, {"h", panel.h}, {"tab_group", panel.tab_group},
                                    {"active", panel.active}});
    for (std::size_t i = 0; i < queries_.size(); ++i)
    {
        const auto &tab = queries_[i];
        if (current() == tab.get())
            state["active"] = i;
        state["tabs"].push_back({{"profile", tab->profile_id},
                                 {"database", tab->database},
                                 {"name", tab->name},
                                 {"sql", contents(tab->sql)},
                                 {"path", tab->path},
                                 {"widths", tab->result->widths()},
                                 {"library", tab->saved_name},
                                 {"dirty", tab->dirty},
                                 {"bom", tab->bom},
                                 {"cursor", tab->editor->insert_position()}});
        if (!tab->saved_name.empty())
            QueryLibrary(settings_.parent_path() / "queries").widths(tab->saved_name, tab->result->widths());
    }
    if (!settings_.parent_path().empty())
        std::filesystem::create_directories(settings_.parent_path());
    auto serialized = state.dump(2);
    if (serialized.size() > 64 * 1024 * 1024)
        throw std::runtime_error(
            "Workspace exceeds 64 MiB. Save large scripts to files and close some tabs before exiting.");
    writeFile(settings_, serialized);
}
void Window::editProfile()
{
    Fl_Double_Window dialog(470, 430, "New PostgreSQL profile");
    const char *labels[] = {"Name",     "Host",     "Port",     "User",
                            "Database", "SSL mode", "Password", "Timeout (seconds)"};
    std::vector<Fl_Input *> fields;
    const char *defaults[] = {"Local PostgreSQL", "localhost", "5432", "postgres",
                              "postgres",         "prefer",    "",     "10"};
    for (int i = 0; i < 8; ++i)
    {
        auto *input = new Fl_Input(125, 20 + i * 39, 320, 29, labels[i]);
        input->value(defaults[i]);
        if (i == 6)
            input->type(FL_SECRET_INPUT);
        fields.push_back(input);
    }
    auto *remember = new Fl_Check_Button(125, 337, 320, 26, "Remember in system credential store");
    bool accepted = false;
    Fl_Button ok(265, 384, 85, 30, "Save"), cancel(360, 384, 85, 30, "Cancel");
    struct Response
    {
        Fl_Window *window;
        bool *accepted;
    };
    Response response{&dialog, &accepted};
    ok.callback(
        [](Fl_Widget *, void *p)
        {
            auto *r = static_cast<Response *>(p);
            *r->accepted = true;
            r->window->hide();
        },
        &response);
    cancel.callback([](Fl_Widget *, void *p) { static_cast<Fl_Window *>(p)->hide(); }, &dialog);
    dialog.end();
    dialog.set_modal();
    localize(dialog);
    dialog.show();
    while (dialog.shown())
        Fl::wait();
    if (!accepted)
        return;
    Profile p;
    p.id = newId();
    p.name = fields[0]->value();
    p.host = fields[1]->value();
    p.port = fields[2]->value();
    p.user = fields[3]->value();
    p.database = fields[4]->value();
    p.ssl = fields[5]->value();
    p.remember = remember->value() != 0;
    try
    {
        std::size_t at = 0;
        std::string text = fields[7]->value();
        p.timeout = std::stoi(text, &at);
        if (at != text.size() || p.timeout < 2 || p.timeout > 120)
            throw std::runtime_error("Invalid timeout");
    }
    catch (...)
    {
        fl_alert("Connection timeout must be between 2 and 120 seconds.");
        return;
    }

    if (p.name.empty() || p.host.empty() || p.user.empty() || p.database.empty())
    {
        fl_alert("Name, host, user and database are required.");
        return;
    }
    secrets_[p.id] = fields[6]->value();
    profiles_.push_back(p);
    auto *item = tree_->add(tree_->root(), p.name.c_str());
    nodes_[item] = {0, p.id, p.database, "", "", false};
    tree_->select_only(item);
    tree_->redraw();
    if (auto *tab = current())
        if (tab->profile_id.empty())
        {
            tab->profile_id = p.id;
            tab->database = p.database;
            tab->rename(tab->name);
        }
    try
    {
        save();
    }
    catch (const std::exception &ex)
    {
        error(ex);
    }
}
void Window::connectProfile()
{
    auto *selected = tree_->first_selected_item();
    auto found = nodes_.find(selected);
    if (found == nodes_.end())
    {
        fl_alert("Select a server profile first.");
        return;
    }
    auto *p = profile(found->second.profile);
    if (!p)
        return;
    if (catalog_wait_)
    {
        fl_alert("Wait for the current catalog request.");
        return;
    }
    catalog_profile_ = p->id;
    Fl_Tree_Item *root = nullptr;
    for (const auto &entry : nodes_)
        if (entry.second.kind == 0 && entry.second.profile == p->id)
        {
            root = entry.first;
            break;
        }
    if (root && nodes_[root].loaded)
    {
        tree_->open(root, 0);
        for (auto &tab : queries_)
            if (tab->profile_id == p->id)
                ensureSession(*tab, false);
        return;
    }
    catalog(p->database,
            "SELECT datname FROM pg_database WHERE NOT datistemplate AND datallowconn ORDER BY datname", root,
            1);
}
void Window::catalog(std::string database, std::string sql, Fl_Tree_Item *parent, int next_kind)
{
    if (catalog_wait_ || !parent)
        return;
    auto *p = profile(catalog_profile_);
    if (!p)
        return;
    catalog_wait_ = true;
    catalog_parent_ = parent;
    catalog_kind_ = next_kind;
    catalog_sql_ = std::move(sql);
    catalog_database_ = std::move(database);
    catalog_results_.clear();
    if (!catalog_)
        catalog_ = std::make_unique<Session>([] { Fl::awake(); });
    catalog_->connect(*p, catalog_database_, secrets_[p->id]);
    status_->copy_label("Connecting / reading catalog...");
}
void Window::browse(Fl_Tree_Item *item)
{
    auto found = nodes_.find(item);
    if (found == nodes_.end() || catalog_wait_)
        return;
    auto &node = found->second;
    catalog_profile_ = node.profile;
    if (node.kind == 0)
    {
        connectProfile();
        return;
    }
    if (node.kind == 1)
        catalog(node.database,
                "SELECT nspname FROM pg_namespace WHERE nspname NOT LIKE 'pg_%' AND nspname <> "
                "'information_schema' ORDER BY nspname",
                item, 2);
    else if (node.kind == 2)
        catalog(node.database,
                "SELECT c.relname, c.relkind::text FROM pg_class c JOIN pg_namespace n ON "
                "n.oid=c.relnamespace WHERE n.nspname=" +
                    quoteLiteral(node.schema) +
                    " AND c.relkind IN ('r','p','v','m','S','f') ORDER BY c.relname",
                item, 3);
    else if (node.kind == 3)
        catalog(node.database,
                "SELECT a.attname, format_type(a.atttypid,a.atttypmod) FROM pg_attribute a JOIN pg_class c "
                "ON c.oid=a.attrelid JOIN pg_namespace n ON n.oid=c.relnamespace WHERE n.nspname=" +
                    quoteLiteral(node.schema) + " AND c.relname=" + quoteLiteral(node.name) +
                    " AND a.attnum>0 AND NOT a.attisdropped ORDER BY a.attnum",
                item, 4);
}
void Window::treeClick()
{
    auto *item = tree_->callback_item();
    auto found = nodes_.find(item);
    if (found == nodes_.end())
        return;
    if (Fl::event_button() == FL_RIGHT_MOUSE)
    {
        tree_->select_only(item, 0);
        const char *labels[] = {"New query",
                                "Inspect object",
                                "Connect",
                                "Disconnect",
                                "Edit profile",
                                "Create database...",
                                "Backup...",
                                "Restore...",
                                "Drop database...",
                                "VACUUM...",
                                "ANALYZE...",
                                "REINDEX...",
                                "Server roles / database privileges...",
                                "Refresh"};
        Fl_Menu_Item menu[15]{};
        for (int i = 0; i < 14; ++i)
            menu[i].label(labels[i]);
        auto *selected = menu[0].popup(Fl::event_x_root(), Fl::event_y_root());
        if (!selected)
            return;
        auto action = selected - menu;
        if (action == 0)
        {
            auto node = found->second;
            newQuery(node.profile, node.database);
            if (node.kind == 3)
                current()->sql.text(("SELECT * FROM " + quoteIdentifier(node.schema) + "." +
                                     quoteIdentifier(node.name) + " LIMIT 1000;")
                                        .c_str());
        }
        else if (action == 1)
            inspectObject();
        else if (action == 2)
            connectProfile();
        else if (action == 3)
            disconnect();
        else if (action == 4)
            editSelectedProfile();
        else if (action == 5)
            databaseTask(2);
        else if (action == 6)
            databaseTask(0);
        else if (action == 7)
            databaseTask(1);
        else if (action == 8)
            databaseTask(3);
        else if (action >= 9 && action <= 11)
            databaseTask(static_cast<int>(action) - 5);
        else if (action == 12)
            manageRoles();
        else
            refreshCatalog();
        return;
    }
    if (Fl::event_clicks() && found->second.kind == 1)
    {
        const auto node = found->second;
        newQuery(node.profile, node.database);
        if (node.kind == 3)
            current()->sql.text(("SELECT * FROM " + quoteIdentifier(node.schema) + "." +
                                 quoteIdentifier(node.name) + " LIMIT 1000;")
                                    .c_str());
        return;
    }
    if (tree_->callback_reason() == FL_TREE_REASON_SELECTED && !found->second.loaded)
        browse(item);
    else if (tree_->callback_reason() == FL_TREE_REASON_OPENED && !found->second.loaded)
        browse(item);
}
bool Window::ensureSession(QueryTab &tab, bool interactive)
{
    if (tab.session && tab.session->connected())
        return true;
    auto *p = profile(tab.profile_id);
    if (!p)
    {
        fl_alert("Choose a database in the tree, or create a connection profile.");
        return false;
    }
    std::size_t active = 0;
    for (const auto &q : queries_)
        if (q->session)
            ++active;
    if (!tab.session && active >= 8)
    {
        tab.log("Close a query tab to release one of the eight sessions.");
        if (interactive)
            fl_alert("Close a query tab to release one of the eight sessions.");
        return false;
    }
    if (!tab.session)
        tab.session = std::make_unique<Session>([] { Fl::awake(); });
    if (!tab.session->busy())
        tab.session->connect(*p, tab.database, secrets_[p->id]);
    return false;
}
void Window::connectedQuery(QueryTab &tab)
{
    if (tab.pending_sql.empty())
        return;
    tab.result->clear(tab.all->value() != 0);
    tab.messages.text("");
    tab.plan.text("");
    tab.log("Independent tab session. Statements stop on the first error. Use BEGIN/COMMIT explicitly for "
            "transactions.");
    if (tab.pending_plan)
        tab.log("Plan mode: changes are rolled back after collecting the plans. Sequences/external effects "
                "may persist.");
    tab.output->value(tab.pending_plan ? tab.plan_panel : tab.result_panel);
    if (!tab.pending_export.empty())
        tab.log("Full export executes the SQL again. A file error does not undo completed SQL changes; use "
                "an explicit transaction when needed.");
    tab.session->execute(std::move(tab.pending_sql), tab.pending_plan, std::move(tab.pending_export),
                         tab.result->widths());
    tab.pending_sql.clear();
    tab.pending_export.clear();
}
void Window::execute(bool full_csv, bool excel)
{
    auto *tab = current();
    if (!tab)
        return;
    if (tab->session && tab->session->busy())
    {
        fl_alert("This tab is busy.");
        return;
    }
    tab->pending_export.clear();
    auto sql = selected(tab->sql);
    if (sql.empty())
        sql = contents(tab->sql);
    if (sql.empty())
        return;
    if (sql.size() > 8 * 1024 * 1024)
    {
        fl_alert("At most 8 MiB SQL per execution.");
        return;
    }
    if (full_csv)
    {
        auto path = chooseFile(true, excel ? "Execute to full XLSX" : "Execute to full CSV",
                               excel ? "Excel\t*.xlsx" : "CSV\t*.csv");
        if (path.empty())
            return;
        if (std::filesystem::u8path(path).extension().empty())
            path += excel ? ".xlsx" : ".csv";
        tab->pending_export = std::filesystem::u8path(path);
    }
    tab->pending_plan = tab->planSelected();
    if (tab->pending_plan)
    {
        std::string error;
        auto statements = splitSql(sql, error);
        if (!error.empty())
        {
            fl_alert("%s", error.c_str());
            return;
        }
        std::string output;
        for (const auto &statement : statements)
        {
            if (statement.keyword == "EXPLAIN")
                output += statement.sql + '\n';
            else
            {
                if (!statement.title.empty())
                    output += "-- @resultaat: " + statement.title + '\n';
                output += "EXPLAIN (ANALYZE " + std::string(tab->analyze->value() ? "TRUE" : "FALSE") +
                          ", BUFFERS " + (tab->buffers->value() ? "TRUE" : "FALSE") + ", VERBOSE " +
                          (tab->verbose->value() ? "TRUE" : "FALSE") + ", COSTS " +
                          (tab->costs->value() ? "TRUE" : "FALSE") +
                          (tab->analyze->value()
                               ? std::string(", TIMING ") + (tab->timing->value() ? "TRUE" : "FALSE")
                               : "") +
                          ", FORMAT " + tab->format->text() + ")\n" + statement.sql + '\n';
            }
        }
        sql = std::move(output);
    }
    tab->pending_sql = std::move(sql);
    if (tab->pending_sql.size() <= 64000)
    {
        history_.insert(history_.begin(), Json{{"sql", tab->pending_sql},
                                               {"profile", tab->profile_id},
                                               {"database", tab->database},
                                               {"plan", tab->pending_plan}});
        if (history_.size() > 100)
            history_.resize(100);
        refreshLibrary(false);
    }
    if (ensureSession(*tab))
        connectedQuery(*tab);
}
void Window::closeQuery()
{
    auto *tab = current();
    if (!tab)
        return;
    if (tab->session && tab->session->busy())
    {
        fl_alert("Cancel the running query and wait before closing this tab.");
        return;
    }
    if (tab->session && tab->session->inTransaction() &&
        chooseAction("Closing this tab rolls back its open transaction.", "Cancel", "Close", nullptr) != 1)
        return;
    if (tab->dirty)
    {
        int answer = chooseAction("Save changes before closing this tab?", "Cancel", "Save", "Discard");
        if (answer <= 0)
            return;
        if (answer == 1)
        {
            saveSql();
            if (tab->dirty)
                return;
        }
    }
    tabs_->remove(*tab);
    queries_.erase(
        std::remove_if(queries_.begin(), queries_.end(), [tab](const auto &q) { return q.get() == tab; }),
        queries_.end());
    if (queries_.empty())
        newQuery();
    tabs_->redraw();
}
void Window::renameQuery()
{
    if (auto *tab = current())
        if (auto *name = safeInput("Tab name:", tab->name.c_str()))
            tab->rename(*name ? name : "Query");
    tabs_->redraw();
}
void Window::openSql()
{
    auto path = chooseFile(false, "Open SQL", "SQL\t*.sql");
    if (path.empty())
        return;
    try
    {
        bool bom = false;
        auto text = readSqlFile(std::filesystem::u8path(path), &bom);
        newQuery("", "", text, std::filesystem::u8path(path).filename().u8string());
        current()->path = path;
        current()->bom = bom;
        current()->dirty = false;
        current()->rename(current()->name);
    }
    catch (const std::exception &ex)
    {
        error(ex);
    }
}
void Window::saveSql()
{
    auto *tab = current();
    if (!tab)
        return;
    auto path = tab->path;
    if (path.empty() && !tab->saved_name.empty())
    {
        try
        {
            QueryLibrary library(settings_.parent_path() / "queries");
            std::string category;
            for (const auto &entry : library.entries())
                if (entry.name == tab->saved_name)
                    category = entry.category;
            library.save(tab->saved_name, contents(tab->sql), category, tab->result->widths());
            tab->dirty = false;
            tab->rename(tab->name);
            refreshLibrary();
        }
        catch (const std::exception &ex)
        {
            error(ex);
        }
        return;
    }
    if (path.empty())
        path = chooseFile(true, "Save SQL", "SQL\t*.sql");
    if (path.empty())
        return;
    try
    {
        writeFile(std::filesystem::u8path(path),
                  (tab->bom ? std::string("\xef\xbb\xbf") : std::string()) + contents(tab->sql));
        tab->path = path;
        tab->dirty = false;
        tab->rename(tab->name);
        status_->copy_label("SQL saved");
    }
    catch (const std::exception &ex)
    {
        error(ex);
    }
}
void Window::saveNamed()
{
    auto *tab = current();
    if (!tab)
        return;
    auto *entered =
        safeInput("Query name:", tab->saved_name.empty() ? tab->name.c_str() : tab->saved_name.c_str());
    if (!entered || !*entered)
        return;
    std::string name = entered;
    try
    {
        QueryLibrary library(settings_.parent_path() / "queries");
        library.validate(name);
        auto existing = library.existingName(name);
        if (!existing.empty())
            name = existing;
        std::string category;
        for (const auto &entry : library.entries())
            if (entry.name == name)
            {
                category = entry.category;
                if (chooseAction("Replace this saved query?", "Cancel", "Replace", nullptr) != 1)
                    return;
            }
        auto *entered_category = safeInput("Category (blank for uncategorized):", category.c_str());
        if (!entered_category)
            return;
        library.save(name, contents(tab->sql), entered_category, tab->result->widths());
        tab->saved_name = name;
        tab->path.clear();
        tab->bom = false;
        tab->dirty = false;
        tab->rename(name);
        refreshLibrary();
    }
    catch (const std::exception &ex)
    {
        error(ex);
    }
}
void Window::openNamed()
{
    try
    {
        auto found = library_items_.find(library_tree_->first_selected_item());
        if (found == library_items_.end())
        {
            fl_message("Select a saved query in the right panel.");
            return;
        }
        auto entry = found->second;
        QueryLibrary library(settings_.parent_path() / "queries");
        newQuery("", "", library.read(entry.name), entry.name);
        current()->saved_name = entry.name;
        current()->dirty = false;
        current()->rename(current()->name);
        current()->result->widths(entry.widths);
    }
    catch (const std::exception &ex)
    {
        error(ex);
    }
}
void Window::copyAll()
{
    if (auto *tab = current())
    {
        auto text = tab->result->copyText();
        Fl::copy(text.c_str(), static_cast<int>(text.size()), 1);
    }
}
void Window::find()
{
    auto *tab = current();
    if (!tab)
        return;
    auto *needle = safeInput("Find:", "");
    if (needle && *needle && !tab->editor->find(needle))
        fl_message("No match found.");
}
void Window::changeLanguage(const char *value)
{
    try
    {
        language(value);
        localize(*this);
        localize(*browser_panel_);
        localize(*library_panel_);
        localize(*history_panel_);
        save();
    }
    catch (const std::exception &ex)
    {
        error(ex);
    }
}
void Window::explainOptions()
{
    if (auto *tab = current())
        tab->output->value(tab->plan_panel);
}
void Window::about()
{
    Fl_SVG_Image svg(nullptr, appSvg);
    std::unique_ptr<Fl_Image> icon(svg.copy(112, 112));
    Fl_Double_Window dialog(620, 270, "About pgadmin-jr");
    Fl_Box image(24, 40, 112, 112);
    image.image(icon.get());
    Fl_Box text(160, 24, 435, 190);
    text.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    text.copy_label((std::string("pgadmin-jr ") + PGADM_APP_VERSION +
                     "\n\nNative PostgreSQL administration\nFLTK, fltk-css, fltk-cell, libpq and "
                     "libxlsxwriter\n\nC++17 / STL - no Qt runtime\nhttps://github.com/hdijkema/pgadmin-jr")
                        .c_str());
    Fl_Button close(490, 225, 105, 30, "Close");
    close.callback([](Fl_Widget *, void *p) { static_cast<Fl_Window *>(p)->hide(); }, &dialog);
    dialog.end();
    dialog.set_modal();
    localize(dialog);
    dialog.show();
    while (dialog.shown())
        Fl::wait();
    image.image(nullptr);
}
void Window::detail(const Cell &cell)
{
    Cell stored = cell;
    bool binary = stored.type == 17 && !stored.null && !stored.clipped;
    auto value = cellText(stored);
    std::vector<std::size_t> offsets{0};
    auto size = binary ? stored.binary.size() : value.size();
    auto page_size = binary ? 4096u : 50000u;
    for (std::size_t end = page_size; end < size;)
    {
        if (!binary)
            while (end < size && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80)
                --end;
        offsets.push_back(end);
        end += page_size;
    }
    Fl_Text_Buffer buffer;
    Fl_Double_Window dialog(850, 590, "Cell details");
    Fl_Box description(12, 8, 826, 28);
    description.copy_label((std::to_string(size) + " retained bytes" +
                            (stored.clipped ? " | PREVIEW CLIPPED - full value is not retained" : ""))
                               .c_str());
    Fl_Text_Display display(12, 42, 826, 480);
    display.buffer(&buffer);
    display.textfont(FL_COURIER);
    display.wrap_mode(binary ? Fl_Text_Display::WRAP_NONE : Fl_Text_Display::WRAP_AT_BOUNDS, 0);
    Fl_Button previous(12, 540, 90, 30, "Previous"), next(110, 540, 90, 30, "Next"),
        copy(400, 540, 120, 30, "Copy value"), save(530, 540, 145, 30, "Save value..."),
        close(730, 540, 108, 30, "Close");
    Fl_Box page(205, 540, 185, 30);
    struct Details
    {
        Cell *cell;
        std::string *value;
        std::vector<std::size_t> *offsets;
        bool binary;
        std::size_t current = 0;
        Fl_Text_Buffer *buffer;
        Fl_Box *page;
        Fl_Button *previous, *next;
    };
    Details state{&stored, &value, &offsets, binary, 0, &buffer, &page, &previous, &next};
    auto render = [](Details &s)
    {
        auto start = (*s.offsets)[s.current],
             end = s.current + 1 < s.offsets->size() ? (*s.offsets)[s.current + 1]
                                                     : (s.binary ? s.cell->binary.size() : s.value->size());
        std::string text;
        if (s.binary)
        {
            std::ostringstream out;
            for (auto i = start; i < end; i += 16)
            {
                out << std::hex << std::setw(8) << std::setfill('0') << i << "  ";
                for (auto j = i; j < end && j < i + 16; ++j)
                    out << std::setw(2) << static_cast<unsigned>(s.cell->binary[j]) << ' ';
                out << '\n';
            }
            text = out.str();
        }
        else
            text = s.value->substr(start, end - start);
        s.buffer->text(text.c_str());
        s.page->copy_label(
            (std::to_string(s.current + 1) + " / " + std::to_string(s.offsets->size())).c_str());
        if (s.current)
            s.previous->activate();
        else
            s.previous->deactivate();
        if (s.current + 1 < s.offsets->size())
            s.next->activate();
        else
            s.next->deactivate();
    };
    struct Navigation
    {
        Details *state;
        decltype(render) *render;
    };
    Navigation navigation{&state, &render};
    previous.callback(
        [](Fl_Widget *, void *p)
        {
            auto *n = static_cast<Navigation *>(p);
            if (n->state->current)
                --n->state->current;
            (*n->render)(*n->state);
        },
        &navigation);
    next.callback(
        [](Fl_Widget *, void *p)
        {
            auto *n = static_cast<Navigation *>(p);
            if (n->state->current + 1 < n->state->offsets->size())
                ++n->state->current;
            (*n->render)(*n->state);
        },
        &navigation);
    copy.callback(
        [](Fl_Widget *, void *p)
        {
            auto *s = static_cast<Details *>(p);
            Fl::copy(s->value->c_str(), static_cast<int>(s->value->size()), 1);
        },
        &state);
    save.callback(
        [](Fl_Widget *, void *p)
        {
            auto *s = static_cast<Details *>(p);
            if (s->cell->clipped && chooseAction("Only the retained part is available. Save that part?",
                                                 "Cancel", "Save", nullptr) != 1)
                return;
            auto path = chooseFile(true, "Save cell value", s->binary ? "Binary\t*.bin" : "Text\t*.txt");
            if (path.empty())
                return;
            try
            {
                auto bytes =
                    s->binary ? std::string(s->cell->binary.begin(), s->cell->binary.end()) : *s->value;
                writeFile(std::filesystem::u8path(path), bytes);
            }
            catch (const std::exception &ex)
            {
                error(ex);
            }
        },
        &state);
    close.callback([](Fl_Widget *, void *p) { static_cast<Fl_Window *>(p)->hide(); }, &dialog);
    render(state);
    dialog.resizable(display);
    dialog.end();
    dialog.set_modal();
    localize(dialog);
    dialog.show();
    while (dialog.shown())
        Fl::wait();
    display.buffer(nullptr);
}
void Window::exit()
{
    for (const auto &tab : queries_)
    {
        if (tab->session && tab->session->busy())
        {
            fl_alert("Cancel running work before exiting.");
            return;
        }
        if (tab->session && tab->session->inTransaction() &&
            chooseAction("Exit and roll back open transactions?", "Cancel", "Exit", nullptr) != 1)
            return;
    }
    if (catalog_ && catalog_->busy())
    {
        fl_alert("Wait for the catalog connection to finish.");
        return;
    }
    try
    {
        if (!smoke_)
            save();
        hide();
    }
    catch (const std::exception &ex)
    {
        error(ex);
    }
}
void Window::tick(void *owner)
{
    auto *app = static_cast<Window *>(owner);
    app->pump();
    Fl::repeat_timeout(0.05, tick, owner);
}
void Window::pump()
{
    for (auto &tab : queries_)
        updateTarget(*tab);
    for (auto &tab : queries_)
        if (tab->session)
            for (auto &event : tab->session->poll())
            {
                if (event.request != tab->session->request())
                    continue;
                if (event.kind == Event::Connected)
                {
                    tab->log("Connected to " + event.database + " | PID " + std::to_string(event.backend));
                    connectedQuery(*tab);
                }
                else if (event.kind == Event::Batch)
                {
                    if (!event.section.status.empty())
                        tab->log(event.section.status);
                    bool is_plan =
                        event.section.columns.size() == 1 && event.section.columns.front() == "QUERY PLAN";
                    if (is_plan)
                        for (const auto &row : event.section.rows)
                            if (!row.empty())
                                tab->plan.append((row.front().text + '\n').c_str());
                    tab->result->append(event.index, std::move(event.section));
                }
                else if (event.kind == Event::ConnectionError)
                {
                    tab->output->value(tab->message_panel);
                    tab->log(event.text);
                    status_->copy_label(event.text.c_str());
                    if (event.needs_password)
                        if (auto *value = safePassword("PostgreSQL password:", ""))
                        {
                            secrets_[tab->profile_id] = value;
                            auto *p = profile(tab->profile_id);
                            if (p)
                                tab->session->connect(*p, tab->database, value);
                        }
                }
                else
                {
                    tab->log(event.text);
                    if (event.kind == Event::Finished)
                    {
                        if (!event.success)
                            tab->output->value(tab->message_panel);
                        status_->copy_label(event.text.c_str());
                    }
                }
            }
    if (catalog_)
        for (auto &event : catalog_->poll())
        {
            if (event.request != catalog_->request())
                continue;
            if (event.kind == Event::Connected)
                catalog_->execute(catalog_sql_);
            else if (event.kind == Event::Batch)
                catalog_results_.push_back(std::move(event.section));
            else if (event.kind == Event::ConnectionError || event.kind == Event::Finished)
            {
                if (event.kind == Event::ConnectionError || !event.success)
                {
                    status_->copy_label(event.text.c_str());
                    catalog_wait_ = false;
                    if (event.needs_password)
                        if (auto *value = safePassword("PostgreSQL password:", ""))
                        {
                            secrets_[catalog_profile_] = value;
                            auto *p = profile(catalog_profile_);
                            if (p)
                            {
                                catalog_wait_ = true;
                                catalog_->connect(*p, catalog_database_, value);
                            }
                        }
                    continue;
                }
                auto parent = nodes_.find(catalog_parent_);
                if (parent != nodes_.end())
                {
                    auto base = parent->second;
                    parent->second.loaded = true;
                    for (const auto &section : catalog_results_)
                        for (const auto &row : section.rows)
                            if (!row.empty())
                            {
                                auto value = row[0].text;
                                auto *item = tree_->add(catalog_parent_, value.c_str());
                                Node node = base;
                                node.kind = catalog_kind_;
                                node.loaded = false;
                                if (node.kind == 1)
                                    node.database = value;
                                else if (node.kind == 2)
                                    node.schema = value;
                                else if (node.kind == 4)
                                    node.column = value;
                                else
                                    node.name = value;
                                nodes_[item] = std::move(node);
                            }
                    tree_->open(catalog_parent_, 0);
                    tree_->redraw();
                    std::vector<std::string> words;
                    for (const auto &node : nodes_)
                        if (node.second.kind >= 2)
                            words.push_back(node.first->label());
                    for (auto &tab : queries_)
                        tab->editor->completionWords(words);
                }
                catalog_wait_ = false;
                catalog_results_.clear();
                status_->copy_label("Catalog loaded");
                if (catalog_kind_ == 1)
                    for (auto &tab : queries_)
                        if (tab->profile_id == catalog_profile_ && !tab->session)
                            ensureSession(*tab, false);
            }
        }
    auto *tab = current();
    if (tab && tab->session && tab->session->busy())
    {
        run_->deactivate();
        cancel_->activate();
    }
    else
    {
        run_->activate();
        if (catalog_wait_)
            cancel_->activate();
        else
            cancel_->deactivate();
    }
}
} // namespace pgjr
