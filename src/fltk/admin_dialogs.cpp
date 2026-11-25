#include "window.h"
#include "tool_job.h"
#include "xlsx_writer.h"
#include <FL/Fl.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/fl_ask.H>
#include <chrono>
#include <cstdlib>
#include <sstream>
namespace pgjr
{
namespace
{
class Form
{
    Fl_Double_Window window_;
    int next_ = 16;
    bool accepted_ = false;
    Json &dialogs_;
    const Fl_Window &parent_;
    std::string id_;

  public:
    Form(const char *title, Json &dialogs, const Fl_Window &parent)
        : window_(680, 500, title), dialogs_(dialogs), parent_(parent), id_(title)
    {
    }
    Fl_Input *input(const std::string &label, const std::string &value = {}, bool secret = false)
    {
        auto *field = new Fl_Input(215, next_, 440, 28);
        field->copy_label(label.c_str());
        field->value(value.c_str());
        if (secret)
            field->type(FL_SECRET_INPUT);
        next_ += 35;
        return field;
    }
    Fl_Check_Button *check(const std::string &label, bool value = false)
    {
        auto *field = new Fl_Check_Button(215, next_, 440, 25);
        field->copy_label(label.c_str());
        field->value(value);
        next_ += 29;
        return field;
    }
    void note(const std::string &text)
    {
        auto *box = new Fl_Box(15, next_, 650, 55);
        box->copy_label(text.c_str());
        box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
        next_ += 61;
    }
    bool run()
    {
        auto *ok = new Fl_Button(455, next_ + 8, 90, 30, "Apply");
        ok->callback(
            [](Fl_Widget *, void *owner)
            {
                auto *f = static_cast<Form *>(owner);
                f->accepted_ = true;
                f->window_.hide();
            },
            this);
        auto *cancel = new Fl_Button(555, next_ + 8, 100, 30, "Cancel");
        cancel->callback([](Fl_Widget *, void *p) { static_cast<Form *>(p)->window_.hide(); }, this);
        window_.end();
        window_.size(680, next_ + 52);
        window_.set_modal();
        localize(window_);
        DialogPlacement placement(window_, dialogs_, id_, parent_);
        window_.show();
        while (window_.shown())
            Fl::wait();
        return accepted_;
    }
};
struct Answer
{
    bool success = false, clipped = false;
    std::vector<Section> sections;
};
Answer sessionDialog(Json &dialogs, const Fl_Window &parent, const Profile &profile,
                     const std::string &database, const std::string &secret, const std::string &sql,
                     const char *title)
{
    Fl_Text_Buffer buffer;
    Fl_Double_Window dialog(840, 530, title);
    Fl_Text_Display view(12, 12, 816, 450);
    view.buffer(&buffer);
    view.textfont(FL_COURIER);
    Fl_Button cancel(710, 480, 115, 30, "Cancel");
    dialog.end();
    dialog.set_modal();
    Session session;
    bool closed = false, started = false;
    Answer answer;
    struct Control
    {
        Session *session;
        Fl_Window *window;
        bool *closed;
    };
    Control control{&session, &dialog, &closed};
    auto callback = [](Fl_Widget *, void *p)
    {
        auto *c = static_cast<Control *>(p);
        if (c->session->busy())
            c->session->cancel();
        else
        {
            *c->closed = true;
            c->window->hide();
        }
    };
    cancel.callback(callback, &control);
    dialog.callback(callback, &control);
    dialog.resizable(view);
    localize(dialog);
    DialogPlacement placement(dialog, dialogs, std::string("session-") + title, parent);
    dialog.show();
    session.connect(profile, database, secret);
    auto start = std::chrono::steady_clock::now();
    while (dialog.shown())
    {
        Fl::wait(0.05);
        for (auto &event : session.poll())
        {
            if (event.kind == Event::Connected)
            {
                started = true;
                session.execute(sql);
            }
            else if (event.kind == Event::Batch)
            {
                buffer.append((event.section.status + '\n').c_str());
                answer.clipped = answer.clipped || event.section.clipped;
                answer.sections.push_back(std::move(event.section));
            }
            else if (event.kind == Event::Finished || event.kind == Event::ConnectionError)
            {
                answer.success = event.kind == Event::Finished && event.success && !answer.clipped;
                if (answer.clipped)
                    buffer.append("Metadata preview clipped; use SQL to refine the selection.\n");
                buffer.append((event.text + '\n').c_str());
                cancel.copy_label(tr("Close"));
                if (answer.success)
                    dialog.hide();
            }
            else
                buffer.append((event.text + '\n').c_str());
        }
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start)
                .count();
        dialog.copy_label((std::string(title) + " | " + std::to_string(seconds) + " s").c_str());
        if (!started && !session.busy() && closed)
            break;
    }
    view.buffer(nullptr);
    return answer;
}
std::string choose(bool save)
{
    Fl_Native_File_Chooser chooser;
    chooser.title(save ? "Backup destination" : "Restore backup");
    chooser.type(save ? Fl_Native_File_Chooser::BROWSE_SAVE_FILE : Fl_Native_File_Chooser::BROWSE_FILE);
    if (save)
        chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    return chooser.show() == 0 ? chooser.filename() : "";
}
std::vector<std::vector<Cell>> data(const Answer &answer)
{
    std::vector<std::vector<Cell>> rows;
    for (const auto &section : answer.sections)
        rows.insert(rows.end(), section.rows.begin(), section.rows.end());
    return rows;
}
bool forgetPassword(const Profile &profile)
{
    std::string error;
    std::atomic_bool done{false};
    Fl_Double_Window progress(440, 105, "System credential store");
    Fl_Box label(12, 12, 416, 75, "Updating the stored credential...");
    progress.end();
    progress.callback([](Fl_Widget *, void *) {});
    progress.set_modal();
    localize(progress);
    progress.show();
    std::thread worker(
        [&]
        {
            try
            {
                erasePassword(profile);
            }
            catch (const std::exception &e)
            {
                error = e.what();
            }
            done = true;
        });
    while (!done)
        Fl::wait(0.05);
    worker.join();
    progress.hide();
    if (!error.empty())
    {
        fl_alert("%s", error.c_str());
        return false;
    }
    return true;
}
void exportDialog(const std::function<void(const std::atomic_bool &)> &write)
{
    Fl_Double_Window dialog(480, 125, "Export results");
    Fl_Box status(12, 12, 456, 45);
    Fl_Button cancel(350, 78, 115, 30, "Cancel");
    dialog.end();
    dialog.set_modal();
    std::atomic_bool cancelled{false}, done{false};
    std::string error;
    std::thread worker(
        [&]
        {
            try
            {
                write(cancelled);
            }
            catch (const std::exception &e)
            {
                error = e.what();
            }
            done = true;
        });
    auto callback = [](Fl_Widget *, void *p) { static_cast<std::atomic_bool *>(p)->store(true); };
    cancel.callback(callback, &cancelled);
    dialog.callback(callback, &cancelled);
    localize(dialog);
    dialog.show();
    auto start = std::chrono::steady_clock::now();
    while (!done)
    {
        Fl::wait(0.05);
        auto seconds =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start)
                .count();
        status.copy_label((std::string(cancelled ? "Cancelling" : "Writing export") + " | " +
                           std::to_string(seconds) + " s")
                              .c_str());
    }
    worker.join();
    dialog.hide();
    if (!error.empty())
        fl_alert("%s", error.c_str());
}
} // namespace
void Window::exportExcel()
{
    auto *tab = current();
    if (!tab)
        return;
    Fl_Native_File_Chooser chooser;
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    chooser.filter("Excel\t*.xlsx");
    if (chooser.show())
        return;
    auto path = std::filesystem::u8path(chooser.filename());
    auto sections = tab->result->sections();
    auto widths = tab->result->widths();
    exportDialog([&](const std::atomic_bool &cancelled) { exportXlsx(path, sections, widths, &cancelled); });
}
void Window::exportCsv()
{
    auto *tab = current();
    if (!tab)
        return;
    Fl_Native_File_Chooser chooser;
    chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    chooser.filter("CSV\t*.csv");
    if (chooser.show())
        return;
    auto path = std::filesystem::u8path(chooser.filename());
    auto sections = tab->result->sections();
    exportDialog(
        [&](const std::atomic_bool &cancelled)
        {
            auto temporary = path;
            temporary += "." + newId() + ".tmp";
            try
            {
                auto bytes = pgjr::exportCsv(sections);
                if (cancelled)
                    throw std::runtime_error("Export cancelled");
                writeFile(temporary, bytes);
                if (cancelled)
                    throw std::runtime_error("Export cancelled");
                publishFile(temporary, path);
            }
            catch (...)
            {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
                throw;
            }
        });
}
void Window::cancelQuery()
{
    if (auto *tab = current())
    {
        tab->pending_sql.clear();
        tab->pending_export.clear();
        if (tab->session && tab->session->busy())
        {
            tab->session->cancel();
            status_->copy_label("Cancelling...");
            return;
        }
    }
    if (catalog_ && catalog_wait_)
        catalog_->cancel();
}
void Window::disconnect()
{
    auto it = nodes_.find(tree_->first_selected_item());
    auto id = it == nodes_.end() ? (current() ? current()->profile_id : "") : it->second.profile;
    if (catalog_wait_)
    {
        if (catalog_ && catalog_profile_ == id)
            catalog_->cancel();
        else
            fl_alert("Wait for the catalog request to finish.");
        return;
    }
    bool transaction = false;
    for (const auto &tab : queries_)
        if (tab->profile_id == id && tab->session)
        {
            if (tab->session->busy())
            {
                fl_alert("Cancel running queries first.");
                return;
            }
            transaction = transaction || tab->session->inTransaction();
        }
    if (transaction &&
        chooseAction("Disconnect and roll back open transactions?", "Cancel", "Disconnect", nullptr) != 1)
        return;
    for (auto &tab : queries_)
        if (tab->profile_id == id)
            tab->session.reset();
    if (catalog_profile_ == id)
        catalog_.reset();
    secrets_.erase(id);
    status_->copy_label("Disconnected");
}
void Window::updateTarget(QueryTab &tab)
{
    auto *p = profile(tab.profile_id);
    std::string state = "Disconnected";
    if (tab.session)
    {
        if (tab.session->busy())
            state = "Working";
        else if (tab.session->connected())
            state = tab.session->inTransaction() ? "Transaction open" : "Autocommit";
    }
    auto label = (p ? p->name + " | " + p->host + ":" + p->port + " | " + p->user : "No connection profile") +
                 " / " + tab.database + " | " + tr(state.c_str());
    if (!tab.target->label() || label != tab.target->label())
        tab.target->copy_label(label.c_str());
}
void Window::useSelectedDatabase()
{
    auto *tab = current();
    auto selected = nodes_.find(tree_->first_selected_item());
    if (!tab || selected == nodes_.end())
        return;
    auto node = selected->second;
    if (tab->session && tab->session->busy())
    {
        fl_alert("Cancel the running query first.");
        return;
    }
    if (tab->session && tab->session->inTransaction() &&
        chooseAction("Changing database rolls back this tab's open transaction.", "Cancel", "Change database",
                     nullptr) != 1)
        return;
    tab->session.reset();
    tab->pending_sql.clear();
    tab->pending_export.clear();
    tab->profile_id = node.profile;
    tab->database = node.database;
    tab->rename(tab->name);
    updateTarget(*tab);
}
void Window::editSelectedProfile()
{
    auto selected = nodes_.find(tree_->first_selected_item());
    if (selected == nodes_.end())
        return;
    auto *p = profile(selected->second.profile);
    if (!p)
        return;
    Form form("Edit connection profile", dialogs_, *this);
    auto *name = form.input("Name", p->name);
    auto *host = form.input("Host", p->host);
    auto *port = form.input("Port", p->port);
    auto *user = form.input("User", p->user);
    auto *database = form.input("Maintenance database", p->database);
    auto *ssl = form.input("SSL mode", p->ssl);
    auto *timeout = form.input("Timeout (seconds)", std::to_string(p->timeout));
    auto *secret = form.input("Password (blank keeps)", "", true);
    auto *remember = form.check("Remember in system credential store", p->remember);
    form.note("Existing sessions must be disconnected before changing this profile.");
    if (!form.run())
        return;
    if (catalog_wait_)
    {
        fl_alert("Wait for the catalog request to finish.");
        return;
    }
    for (const auto &tab : queries_)
        if (tab->profile_id == p->id && tab->session)
        {
            fl_alert("Disconnect this profile first.");
            return;
        }
    auto old = *p;
    Profile next = old;
    next.name = name->value();
    next.host = host->value();
    next.port = port->value();
    next.user = user->value();
    next.database = database->value();
    next.ssl = ssl->value();
    next.remember = remember->value() != 0;
    try
    {
        std::size_t at = 0;
        std::string text = timeout->value();
        next.timeout = std::stoi(text, &at);
        if (at != text.size() || next.timeout < 2 || next.timeout > 120)
            throw std::runtime_error("Invalid timeout");
    }
    catch (...)
    {
        fl_alert("Connection timeout must be between 2 and 120 seconds.");
        return;
    }

    if (next.name.empty() || next.host.empty() || next.user.empty() || next.database.empty())
    {
        fl_alert("Name, host, user and database are required.");
        return;
    }
    if (old.remember &&
        (!next.remember || next.user != old.user || next.host != old.host || next.port != old.port) &&
        !forgetPassword(old))
        return;
    if (next.user != old.user || next.host != old.host || next.port != old.port)
        secrets_.erase(old.id);
    *p = next;
    if (*secret->value())
        secrets_[p->id] = secret->value();
    for (auto &node : nodes_)
        if (node.second.profile == p->id && node.second.kind == 0)
        {
            node.first->label(p->name.c_str());
            node.second.database = p->database;
        }
    refreshCatalog();
    try
    {
        save();
    }
    catch (const std::exception &e)
    {
        fl_alert("%s", e.what());
    }
}
void Window::deleteProfile()
{
    auto selected = nodes_.find(tree_->first_selected_item());
    if (selected == nodes_.end())
        return;
    auto *p = profile(selected->second.profile);
    if (!p)
        return;
    auto removing = *p;
    if (chooseAction(
            "Remove this connection profile and its stored password? Databases and query tabs are retained.",
            "Cancel", "Remove", nullptr) != 1)
        return;
    if (catalog_wait_)
    {
        fl_alert("Wait for the catalog request to finish.");
        return;
    }
    disconnect();
    for (const auto &tab : queries_)
        if (tab->profile_id == removing.id && tab->session)
            return;
    if (removing.remember && !forgetPassword(removing))
        return;
    profiles_.erase(std::remove_if(profiles_.begin(), profiles_.end(),
                                   [&](const auto &value) { return value.id == removing.id; }),
                    profiles_.end());
    secrets_.erase(removing.id);
    nodes_.clear();
    tree_->clear();
    tree_->root(new Fl_Tree_Item(tree_));
    catalog_parent_ = nullptr;
    for (const auto &value : profiles_)
    {
        auto *item = tree_->add(tree_->root(), value.name.c_str());
        nodes_[item] = {0, value.id, value.database, "", "", false};
    }
    try
    {
        save();
    }
    catch (const std::exception &e)
    {
        fl_alert("%s", e.what());
    }
    tree_->redraw();
}
void Window::refreshCatalog()
{
    if (catalog_wait_)
        return;
    for (auto it = nodes_.begin(); it != nodes_.end();)
        if (it->second.kind != 0)
            it = nodes_.erase(it);
        else
        {
            it->second.loaded = false;
            it->first->clear_children();
            ++it;
        }
    tree_->redraw();
}
// PostgreSQL cannot rename the connected database. Use another database and leave
// foreign clients alone; PostgreSQL reports any remaining connections or privilege errors.
void Window::renameDatabase()
{
    auto selected = nodes_.find(tree_->first_selected_item());
    if (selected == nodes_.end() || selected->second.kind != 1)
    {
        fl_alert("%s", tr("Select a database first."));
        return;
    }
    const auto node = selected->second;
    auto *p = profile(node.profile);
    if (!p)
        return;
    const auto connection = *p;
    auto maintenance = connection.database;
    if (maintenance == node.database)
        maintenance = node.database == "postgres" ? "template1" : "postgres";
    Form form("Rename database", dialogs_, *this);
    form.note("Server: " + connection.host + ":" + connection.port + " | Database: " + node.database);
    auto *name = form.input("New database name", node.database);
    auto *other = form.input("Maintenance database", maintenance);
    form.note("Own connections to this database will close and open transactions will roll back. "
              "Other clients are not disconnected.");
    if (!form.run())
        return;
    const std::string next = name->value();
    maintenance = other->value();
    if (next == node.database)
        return;
    try
    {
        const auto sql = renameDatabaseSql(node.database, next);
        auto error = nameError(maintenance);
        if (!error.empty())
            throw std::runtime_error(error);
        if (maintenance == node.database)
        {
            fl_alert("%s", tr("Choose a different maintenance database for renaming."));
            return;
        }
        if (catalog_wait_)
        {
            fl_alert("%s", tr("Wait for the catalog request to finish."));
            return;
        }
        for (const auto &tab : queries_)
            if (tab->profile_id == node.profile && tab->database == node.database &&
                tab->session && tab->session->busy())
            {
                fl_alert("%s", tr("Cancel running queries for this database first."));
                return;
            }
        for (auto &tab : queries_)
            if (tab->profile_id == node.profile && tab->database == node.database)
            {
                tab->session.reset();
                updateTarget(*tab);
            }
        catalog_.reset();
        auto result = sessionDialog(dialogs_, *this, connection, maintenance,
                                    secrets_[node.profile], sql, tr("Rename database"));
        if (!result.success)
            return;
        databaseRenamed(node.profile, node.database, next);
        refreshCatalog();
        for (const auto &entry : nodes_)
            if (entry.second.kind == 0 && entry.second.profile == node.profile)
            {
                tree_->select_only(entry.first, 0);
                browse(entry.first);
                break;
            }
    }
    catch (const std::exception &ex)
    {
        fl_alert("%s", ex.what());
    }
}
// Keep scripts, custom tab names and result data; only future connection targets change.
void Window::databaseRenamed(const std::string &id, const std::string &from, const std::string &to)
{
    if (auto *p = profile(id))
        if (p->database == from)
            p->database = to;
    for (auto &entry : nodes_)
        if (entry.second.profile == id && entry.second.database == from)
            entry.second.database = to;
    for (auto &tab : queries_)
        if (tab->profile_id == id && tab->database == from)
        {
            tab->database = to;
            tab->rename(tab->name);
            updateTarget(*tab);
        }
    save();
}
void Window::databaseTask(int operation)
{
    auto it = nodes_.find(tree_->first_selected_item());
    if (it == nodes_.end())
    {
        fl_alert("Select a server or database first.");
        return;
    }
    auto node = it->second;
    auto *p = profile(node.profile);
    if (!p)
        return;
    auto task = static_cast<Task>(operation);
    auto database = node.database.empty() ? p->database : node.database;
    const char *titles[] = {"Backup database", "Restore database", "Create database", "Drop database",
                            "VACUUM",          "ANALYZE",          "REINDEX"};
    if (operation < 0 || operation > 6)
        return;
    if (operation >= 4 && node.kind != 0 && node.kind != 1 &&
        !(node.kind == 3 && (node.object_type == "r" || node.object_type == "p" ||
                            node.object_type == "m")))
    {
        fl_alert("Select a server, database, table or materialized view for maintenance.");
        return;
    }
    Form form(titles[operation], dialogs_, *this);
    form.note("Server: " + p->host + ":" + p->port + " | User: " + p->user + " | Database: " + database);
    Fl_Input *name = nullptr, *confirmation = nullptr;
    Fl_Check_Button *plain = nullptr, *clean = nullptr, *skip = nullptr, *extra = nullptr;
    std::filesystem::path file;
    if (task == Task::Create)
        name = form.input("Database name");
    if (task == Task::Backup)
        plain = form.check("Plain SQL (otherwise custom archive)");
    if (task == Task::Restore)
    {
        clean = form.check("Clean objects first (archives only)");
        skip = form.check("Skip owners and privileges (archives only)", true);
        form.note("Restore into an empty database. SQL scripts can contain commands that override "
                  "transaction settings. Use a trusted backup.");
    }
    if (task == Task::Drop)
    {
        confirmation = form.input("Type database name");
        form.note("Permanently deletes this database. Own sessions will close and open transactions will "
                  "roll back. Other clients are not forced off.");
    }
    if (task == Task::Vacuum)
        extra = form.check("Also ANALYZE", true);
    if (task == Task::Reindex)
        extra = form.check("CONCURRENTLY", true);
    if (operation >= 4)
        form.note("Uses a separate session. Completed maintenance is not undone by cancellation. A selected "
                  "relation limits the operation to that relation.");
    if (!form.run())
        return;
    if (name)
        database = name->value();
    if (confirmation && database != confirmation->value())
    {
        fl_alert("Database confirmation does not match.");
        return;
    }
    if (task == Task::Backup || task == Task::Restore)
    {
        auto path = choose(task == Task::Backup);
        if (path.empty())
            return;
        file = std::filesystem::u8path(path);
    }
    if (task == Task::Drop || task == Task::Restore)
    {
        if (catalog_wait_)
        {
            fl_alert("Wait for the catalog request to finish.");
            return;
        }
        for (const auto &tab : queries_)
            if (tab->profile_id == p->id && tab->database == database && tab->session && tab->session->busy())
            {
                fl_alert("Cancel running queries for this database first.");
                return;
            }
        for (auto &tab : queries_)
            if (tab->profile_id == p->id && tab->database == database)
                tab->session.reset();
        catalog_.reset();
    }
    try
    {
        if (operation >= 4)
        {
            auto schema = node.kind == 3 ? node.schema : "", relation = node.kind == 3 ? node.name : "";
            auto sql = maintenanceSql(task, database, schema, relation, extra ? extra->value() != 0 : true,
                                      extra ? extra->value() != 0 : true);
            sessionDialog(dialogs_, *this, *p, database, secrets_[p->id], sql, titles[operation]);
            return;
        }
        ToolRequest request;
        request.task = task;
        request.profile = *p;
        request.database = database;
        request.file = file;
        request.secret = secrets_[p->id];
        request.bin = executableDirectory();
        request.plain = plain && plain->value();
        request.clean = clean && clean->value();
        request.skip_owners = skip && skip->value();
        Fl_Text_Buffer buffer;
        Fl_Double_Window dialog(850, 570, titles[operation]);
        Fl_Text_Display display(12, 45, 826, 465);
        display.buffer(&buffer);
        display.textfont(FL_COURIER);
        Fl_Box elapsed(12, 10, 826, 26);
        Fl_Button cancel(720, 526, 115, 30, "Cancel");
        dialog.end();
        dialog.resizable(display);
        dialog.set_modal();
        ToolJob job(std::move(request));
        struct Control
        {
            ToolJob *job;
            Fl_Window *window;
        };
        Control control{&job, &dialog};
        auto callback = [](Fl_Widget *, void *p)
        {
            auto *c = static_cast<Control *>(p);
            if (c->job->busy())
                c->job->cancel();
            else
                c->window->hide();
        };
        cancel.callback(callback, &control);
        dialog.callback(callback, &control);
        localize(dialog);
        DialogPlacement placement(dialog, dialogs_, std::string("tool-") + titles[operation], *this);
        dialog.show();
        auto started = std::chrono::steady_clock::now();
        auto completed = std::chrono::steady_clock::time_point{};
        while (dialog.shown())
        {
            Fl::wait(0.05);
            auto text = job.output();
            if (!text.empty())
            {
                buffer.append(text.c_str());
                if (buffer.length() > 1024 * 1024)
                    buffer.text("[Earlier log output omitted]\n");
                display.insert_position(buffer.length());
                display.show_insert_position();
            }
            if (!job.busy() && completed == std::chrono::steady_clock::time_point{})
                completed = std::chrono::steady_clock::now();
            auto seconds =
                std::chrono::duration_cast<std::chrono::seconds>(
                    (completed == std::chrono::steady_clock::time_point{} ? std::chrono::steady_clock::now()
                                                                          : completed) -
                    started)
                    .count();
            elapsed.copy_label((std::string(job.busy() ? "Running" : "Finished") +
                                " | Elapsed: " + std::to_string(seconds) + " s")
                                   .c_str());
            if (!job.busy())
                cancel.copy_label(tr("Close"));
        }
        display.buffer(nullptr);
        refreshCatalog();
    }
    catch (const std::exception &ex)
    {
        fl_alert("%s", ex.what());
    }
}
void Window::manageRoles()
{
    auto selected = nodes_.find(tree_->first_selected_item());
    Profile *p = selected == nodes_.end() ? (current() ? profile(current()->profile_id) : nullptr)
                                          : profile(selected->second.profile);
    if (!p)
    {
        fl_alert("Select a server first.");
        return;
    }
    auto database = selected == nodes_.end() ? p->database : selected->second.database;
    auto loaded = sessionDialog(dialogs_, *this, *p, p->database, secrets_[p->id], rolesQuery(),
                                "Loading server roles");
    if (!loaded.success)
        return;
    auto rows = data(loaded);
    std::string names;
    for (const auto &row : rows)
        if (!row.empty())
            names += row[0].text + "\n";
    int action = chooseAction("Server roles (shared by all databases)", "Cancel", "Create role", "Edit role");
    if (action <= 0)
        return;
    Json original = Json::object();
    std::string selected_name;
    if (action == 2)
    {
        auto *value = safeInput(("Role name:\n" + names).c_str(), "");
        if (!value)
            return;
        selected_name = value;
        for (const auto &row : rows)
            if (row.size() >= 12 && row[0].text == selected_name)
            {
                original["name"] = row[0].text;
                int i = 1;
                for (auto key :
                     {"login", "inherit", "createdb", "createrole", "superuser", "replication", "bypassrls"})
                    original[key] = row[i++].text == "t";
                original["connection_limit"] = std::stoi(row[8].text);
                original["valid_until"] = row[9].text;
                original["comment"] = row[10].text;
                original["memberships"] = Json::parse(row[11].text);
                break;
            }
        if (original.empty())
        {
            fl_alert("Role not found.");
            return;
        }
    }
    if (action == 2)
    {
        int option = chooseAction("Edit role attributes or database privileges?", "Cancel", "Attributes",
                                  "Database privileges");
        if (option <= 0)
            return;
        if (option == 2)
        {
            auto rights = sessionDialog(dialogs_, *this, *p, p->database, secrets_[p->id],
                                        rightsQuery(selected_name, database), "Loading database privileges");
            if (!rights.success)
                return;
            Json before = Json::object();
            Form form("Database privileges", dialogs_, *this);
            form.note(
                selected_name + " on " + database +
                ". Effective rights may also come from PUBLIC, membership, ownership or superuser status.");
            std::map<std::string, Fl_Check_Button *> fields;
            for (const auto &row : data(rights))
                if (row.size() == 5)
                {
                    auto privilege = row[0].text;
                    before[privilege] = row[1].text == "t";
                    before[privilege + "_grant"] = row[2].text == "t";
                    before["owner"] = row[4].text;
                    fields[privilege] = form.check(privilege + " (direct; effective: " + row[3].text + ")",
                                                   before[privilege].get<bool>());
                    fields[privilege + "_grant"] = form.check(privilege + " with grant option",
                                                              before[privilege + "_grant"].get<bool>());
                }
            auto *owner = form.check("Make this role database owner");
            if (!form.run())
                return;
            Json desired;
            for (const auto &field : fields)
                desired[field.first] = field.second->value() != 0;
            try
            {
                auto sql = rightsSql(selected_name, database, desired, before, owner->value() != 0);
                if (!sql.empty())
                    sessionDialog(dialogs_, *this, *p, p->database, secrets_[p->id],
                                  "BEGIN;\n" + sql + "COMMIT;", "Apply database privileges");
            }
            catch (const std::exception &e)
            {
                fl_alert("%s", e.what());
            }
            return;
        }
    }
    Form form(action == 1 ? "Create server role" : "Edit server role", dialogs_, *this);
    auto *name = form.input("Role name", selected_name);
    if (action == 2)
        name->readonly(1);
    std::map<std::string, Fl_Check_Button *> flags;
    for (auto key : {"login", "inherit", "createdb", "createrole", "superuser", "replication", "bypassrls"})
        flags[key] = form.check(key, original.value(key, std::string(key) == "inherit"));
    auto *limit = form.input("Connection limit", std::to_string(original.value("connection_limit", -1)));
    auto *until = form.input("Valid until", original.value("valid_until", ""));
    auto *comment = form.input("Comment", original.value("comment", ""));
    std::string memberships;
    for (const auto &role : original.value("memberships", std::vector<std::string>{}))
        memberships += role + "\n";
    auto *parents = form.input("Memberships (one per line)", memberships);
    parents->type(FL_MULTILINE_INPUT);
    auto *secret = form.input("New password (blank keeps)", "", true);
    auto *clear = form.check("Remove password");
    if (!form.run())
        return;
    Json desired;
    desired["name"] = name->value();
    for (const auto &flag : flags)
        desired[flag.first] = flag.second->value() != 0;
    desired["valid_until"] = until->value();
    desired["comment"] = comment->value();
    desired["password"] = secret->value();
    desired["clear_password"] = clear->value() != 0;
    try
    {
        std::string text = limit->value();
        std::size_t parsed = 0;
        desired["connection_limit"] = std::stoi(text, &parsed);
        if (parsed != text.size())
            throw std::runtime_error("Invalid connection limit");
        std::vector<std::string> values;
        std::istringstream lines(parents->value());
        std::string line;
        while (std::getline(lines, line))
            if (!line.empty())
                values.push_back(line);
        desired["memberships"] = values;
        auto sql = roleSql(desired, original);
        if (!sql.empty())
            sessionDialog(dialogs_, *this, *p, p->database, secrets_[p->id], "BEGIN;\n" + sql + "COMMIT;",
                          "Apply server role");
        std::fill(sql.begin(), sql.end(), '\0');
    }
    catch (const std::exception &e)
    {
        fl_alert("%s", e.what());
    }
}
void Window::inspectObject()
{
    auto it = nodes_.find(tree_->first_selected_item());
    if (it == nodes_.end() || it->second.kind < 1 || it->second.kind == 5)
        return;
    auto node = it->second;
    auto *p = profile(node.profile);
    if (!p)
        return;
    std::string sql;
    if (node.kind == 1)
        sql = "SELECT datname AS name, pg_get_userbyid(datdba) AS owner, pg_encoding_to_char(encoding) AS "
              "encoding, datcollate AS collation, datconnlimit AS connection_limit, datallowconn AS "
              "connections_allowed, shobj_description(oid,'pg_database') AS comment FROM pg_database WHERE "
              "datname=current_database();";
    else if (node.kind == 2)
        sql = "SELECT nspname AS name, pg_get_userbyid(nspowner) AS owner, "
              "obj_description(oid,'pg_namespace') AS comment FROM pg_namespace WHERE nspname=" +
              quoteLiteral(node.schema) + ";";
    else if (node.kind == 6)
        sql = "SELECT p.proname AS name, p.oid, pg_get_userbyid(p.proowner) AS owner, "
              "l.lanname AS language, pg_get_function_arguments(p.oid) AS arguments, "
              "pg_get_function_result(p.oid) AS result_type, p.prokind AS kind, "
              "p.provolatile AS volatility, p.proisstrict AS strict, p.prosecdef AS security_definer, "
              "p.proparallel AS parallel, obj_description(p.oid,'pg_proc') AS comment, "
              "pg_get_functiondef(p.oid) AS definition FROM pg_proc p "
              "JOIN pg_language l ON l.oid=p.prolang WHERE p.oid=" + quoteLiteral(node.oid) + "::oid;";
    else if (node.kind == 4)
        sql = "SELECT a.attname AS name, format_type(a.atttypid,a.atttypmod) AS type, a.attnotnull AS "
              "not_null, a.attidentity AS identity, a.attgenerated AS generated, "
              "pg_get_expr(d.adbin,d.adrelid) AS default_value, col_description(c.oid,a.attnum) AS comment "
              "FROM pg_attribute a JOIN pg_class c ON c.oid=a.attrelid JOIN pg_namespace n ON "
              "n.oid=c.relnamespace LEFT JOIN pg_attrdef d ON d.adrelid=a.attrelid AND d.adnum=a.attnum "
              "WHERE n.nspname=" +
              quoteLiteral(node.schema) + " AND c.relname=" + quoteLiteral(node.name) +
              " AND a.attname=" + quoteLiteral(node.column) + " AND a.attnum>0 AND NOT a.attisdropped;";
    else
        sql = "SELECT c.relname AS name, c.relkind AS kind, c.oid, pg_get_userbyid(c.relowner) AS owner, "
              "c.relpersistence AS persistence, c.reltuples AS estimated_rows, c.relrowsecurity AS "
              "row_security, c.relispartition AS partition, obj_description(c.oid,'pg_class') AS comment "
              "FROM pg_class c JOIN pg_namespace n ON n.oid=c.relnamespace WHERE n.nspname=" +
              quoteLiteral(node.schema) + " AND c.relname=" + quoteLiteral(node.name) + ";";
    auto answer =
        sessionDialog(dialogs_, *this, *p, node.database, secrets_[p->id], sql, "Object properties");
    if (!answer.success)
        return;
    Section properties;
    properties.title = node.database + " / " + node.schema + " / " +
                       (node.kind == 6 ? node.signature : node.name) +
                       (node.column.empty() ? "" : " / " + node.column);
    properties.columns = {"Property", "Value"};
    std::string routine_definition;
    for (const auto &section : answer.sections)
        for (const auto &row : section.rows)
            for (std::size_t col = 0; col < row.size() && col < section.columns.size(); ++col)
            {
                if (node.kind == 6 && section.columns[col] == "definition")
                {
                    if (!row[col].clipped && !row[col].null)
                        routine_definition = row[col].text;
                    continue;
                }
                Cell key;
                key.text = section.columns[col];
                key.type = 25;
                properties.rows.push_back({key, row[col]});
            }
    properties.count = properties.rows.size();
    Fl_Text_Buffer definition;
    Fl_Double_Window dialog(980, 650, "Object properties");
    Fl_Tabs tabs(12, 12, 956, 580);
    Fl_Group property_panel(14, 40, 952, 550, "Properties");
    ResultTable table(16, 43, 948, 544, css);
    table.widths({260, 620});
    table.clear(true);
    table.append(0, properties);
    table.inspect = [this](const Cell &cell) { detail(cell); };
    property_panel.resizable(table);
    property_panel.end();
    Fl_Group sql_panel(14, 40, 952, 550, "Definition SQL");
    Fl_Text_Display editor(16, 43, 948, 490);
    editor.buffer(&definition);
    editor.textfont(FL_COURIER);
    Fl_Button copy(690, 545, 120, 30, "Copy SQL"), save_sql(825, 545, 130, 30, "Save SQL...");
    sql_panel.resizable(editor);
    sql_panel.end();
    tabs.end();
    Fl_Button close(842, 605, 125, 30, "Close");
    Fl_Box status(14, 605, 815, 30);
    dialog.end();
    dialog.resizable(tabs);
    dialog.set_modal();
    std::unique_ptr<ToolJob> job;
    auto directory =
        std::filesystem::temp_directory_path() / std::filesystem::u8path("pgjr-definition-" + newId());
    auto file = directory / "definition.sql";
    bool requested = false;
    struct Control
    {
        Fl_Window *window;
        std::unique_ptr<ToolJob> *job;
    };
    Control control{&dialog, &job};
    auto close_action = [](Fl_Widget *, void *p)
    {
        auto *c = static_cast<Control *>(p);
        if (*c->job && (*c->job)->busy())
            (*c->job)->cancel();
        else
            c->window->hide();
    };
    close.callback(close_action, &control);
    dialog.callback(close_action, &control);
    copy.callback(
        [](Fl_Widget *, void *p)
        {
            auto *buffer = static_cast<Fl_Text_Buffer *>(p);
            char *value = buffer->text();
            Fl::copy(value, buffer->length(), 1);
            std::free(value);
        },
        &definition);
    save_sql.callback(
        [](Fl_Widget *, void *p)
        {
            Fl_Native_File_Chooser chooser;
            chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
            chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
            chooser.filter("SQL\t*.sql");
            if (chooser.show())
                return;
            auto *buffer = static_cast<Fl_Text_Buffer *>(p);
            char *value = buffer->text();
            try
            {
                writeFile(std::filesystem::u8path(chooser.filename()), value);
            }
            catch (const std::exception &e)
            {
                fl_alert("%s", e.what());
            }
            std::free(value);
        },
        &definition);
    copy.deactivate();
    save_sql.deactivate();
    localize(dialog);
    DialogPlacement placement(dialog, dialogs_, "object-properties", *this);
    auto &saved = dialogs_["object-properties"];
    if (saved.is_object())
    {
        if (saved.contains("widths") && saved["widths"].is_array())
            table.widths(saved["widths"].get<std::vector<int>>());
        if (saved.value("sql_page", false))
            tabs.value(&sql_panel);
    }
    dialog.show();
    std::string logs;
    while (dialog.shown())
    {
        Fl::wait(0.05);
        if (tabs.value() == &sql_panel && !requested)
        {
            requested = true;
            if (node.kind == 6)
            {
                if (routine_definition.empty())
                    definition.text("-- Definition unavailable or exceeds the preview limit.\n");
                else
                {
                    definition.text(routine_definition.c_str());
                    copy.activate();
                    save_sql.activate();
                }
                continue;
            }
            if (node.kind < 3)
            {
                definition.text("Select a relation or column to retrieve its definition SQL.");
                continue;
            }
            try
            {
                std::filesystem::create_directory(directory);
                ToolRequest request;
                request.profile = *p;
                request.database = node.database;
                request.secret = secrets_[p->id];
                request.file = file;
                request.bin = executableDirectory();
                request.plain = true;
                request.schema_only = true;
                request.schema = node.schema;
                request.relation = node.name;
                job = std::make_unique<ToolJob>(std::move(request));
                status.copy_label("Reading definition with pg_dump...");
            }
            catch (const std::exception &e)
            {
                status.copy_label(e.what());
            }
        }
        if (job)
        {
            logs += job->output();
            if (!job->busy())
            {
                if (job->success())
                {
                    try
                    {
                        auto text = readFile(file);
                        definition.text(text.c_str());
                        copy.activate();
                        save_sql.activate();
                        status.copy_label("Structure only; dependent objects may be required separately.");
                    }
                    catch (const std::exception &e)
                    {
                        status.copy_label(e.what());
                    }
                }
                else
                    status.copy_label(logs.c_str());
                job.reset();
            }
        }
    }
    dialogs_["object-properties"]["widths"] = table.widths();
    dialogs_["object-properties"]["sql_page"] = tabs.value() == &sql_panel;
    job.reset();
    editor.buffer(nullptr);
    std::error_code ignored;
    std::filesystem::remove(file, ignored);
    std::filesystem::remove(directory, ignored);
}
} // namespace pgjr
