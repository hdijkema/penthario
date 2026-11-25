#pragma once
#include "core.h"
#include "window_state.h"
#include "session.h"
#include "result_table.h"
#include "sql_editor.h"
#include "query_library.h"
#include "i18n.h"
#include "theme.h"
#include <fltk_css_widgets.h>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Tabs.H>
#include <fltk_dock.h>
#include <fltk_toolbar.h>
#include <fltk_toolbar_svg.h>
#include <FL/Fl_Text_Editor.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Box.H>
#include <map>

namespace pgjr
{
using ToolButton = Fl_Css_Widget<fltk_toolbar::SvgButton>;
class Window;
struct QueryTab : Fl_Group
{
    Window &app;
    std::string name, profile_id, database, path, pending_sql, saved_name;
    bool dirty = false, bom = false;
    std::filesystem::path pending_export;
    bool pending_plan = false;
    std::unique_ptr<Session> session;
    Fl_Text_Buffer sql, styles, messages, plan;
    SqlEditor *editor;
    Fl_Box *target, *splitter;
    Fl_Text_Display *message_view, *plan_view;
    fltk_dock::Tabs *output;
    ResultTable *result;
    Fl_Check_Button *all, *analyze, *buffers, *verbose, *costs, *timing;
    Fl_Choice *format;
    Fl_Group *result_panel, *message_panel, *plan_panel;
    QueryTab(Window &, FLTK_Css &, int, int, int, int);
    ~QueryTab() override;
    void resize(int, int, int, int) override;
    int handle(int) override;
    double splitRatio() const
    {
        return split_ratio_;
    }
    void splitRatio(double);
    void applyTheme(const Theme &);
    void highlight();
    void rename(std::string);
    bool planSelected() const;
    void log(const std::string &);

  private:
    std::array<Fl_Text_Display::Style_Table_Entry, 4> syntax_;
    double split_ratio_ = 0.45;
    bool splitting_ = false, splitter_hover_ = false;
};

// FLTK-only query frontend. Session workers own libpq; the GUI drains immutable result batches.
class Window : public Fl_Css_Window
{
  public:
    Window(FLTK_Css &, std::filesystem::path settings, bool smoke = false);
    ~Window() override;
    void show() override;
    void resize(int, int, int, int) override;
    void newQuery(std::string profile = {}, std::string database = {}, std::string text = {},
                  std::string name = {});
    void execute(bool full_csv = false, bool excel = false);
    void cancelQuery();
    void closeQuery();
    void renameQuery();
    void connectProfile();
    void editProfile();
    void openSql();
    void saveSql();
    void saveNamed();
    void openNamed();
    void refreshLibrary(bool reload = true);
    void libraryAction();
    void searchReplace();
    void changeLanguage(const char *);
    void changeTheme(const char *);
    void useTheme(const std::string &, const std::filesystem::path &css_file = {});
    void loadThemeCss();
    void reloadThemeCss();
    void saveThemeCss();
    const Theme &theme() const
    {
        return theme_;
    }
    void importWorkspace();
    void exportCsv();
    void exportExcel();
    void databaseTask(int);
    // Rename a selected database through a separate maintenance session; closes idle
    // own sessions after confirmation and updates workspace targets only on success.
    void renameDatabase();
    void manageRoles();
    void inspectObject();
    void editSelectedProfile();
    void deleteProfile();
    void useSelectedDatabase();
    void disconnect();
    void copyAll();
    void explainOptions();
    void find();
    void about();
    void detail(const Cell &);
    void exit();
    void pump();
    FLTK_Css &css;

  private:
    friend struct CatalogTest;
    Theme theme_;
    void applyTheme();
    std::array<int, 3> theme_actions_{};
    std::vector<ToolButton *> tool_buttons_;
    fltk_toolbar::Toolbar *toolbar_;
    QueryTab *current();
    Profile *profile(const std::string &);
    bool ensureSession(QueryTab &, bool interactive = true);
    void connectedQuery(QueryTab &);
    void load();
    void save();
    void browse(Fl_Tree_Item *);
    void catalog(std::string database, std::string sql, Fl_Tree_Item *parent, int next_kind);
    // Populate completed catalog rows beneath an existing node; group schema objects
    // by PostgreSQL kind while preserving database targets and routine identities.
    void populateCatalog(Fl_Tree_Item *, int next_kind, const std::vector<Section> &);
    void localizeCatalog();
    void treeClick();
    static void tick(void *);
    Fl_Menu_Bar *menu_;
    Fl_Tree *tree_;
    fltk_dock::Host *dock_;
    fltk_dock::Panel *browser_panel_, *library_panel_, *history_panel_;
    fltk_dock::Tabs *tabs_;
    Fl_Tree *library_tree_, *history_tree_;
    Fl_Input *library_filter_;
    std::map<Fl_Tree_Item *, SavedQuery> library_items_;
    std::map<Fl_Tree_Item *, Json> history_items_;
    std::vector<Json> history_;
    std::vector<SavedQuery> library_cache_;
    Fl_Box *status_;
    ToolButton *connect_, *run_, *cancel_;
    std::filesystem::path settings_;
    std::vector<Profile> profiles_;
    std::map<std::string, std::string> secrets_;
    std::vector<std::unique_ptr<QueryTab>> queries_;
    std::unique_ptr<Session> catalog_;
    std::vector<Section> catalog_results_;
    std::string catalog_profile_, catalog_database_, catalog_sql_;
    struct Node
    {
        int kind = 0;
        std::string profile, database, schema, name;
        bool loaded = false;
        std::string column;
        std::string object_type, oid, signature;
    };
    std::map<Fl_Tree_Item *, Node> nodes_;
    Fl_Tree_Item *catalog_parent_ = nullptr;
    int catalog_kind_ = 0;
    bool catalog_wait_ = false;
    unsigned sequence_ = 1;
    bool smoke_ = false, restore_maximized_ = false;
    Json layouts_ = Json::object(), dialogs_ = Json::object(), normal_rectangle_ = Json::object();
    void refreshCatalog();
    // Apply a successful server rename to this profile and its tabs, then save targets.
    void databaseRenamed(const std::string &profile, const std::string &from, const std::string &to);
    void updateTarget(QueryTab &);
};
} // namespace pgjr
