#include "session.h"
#include "xlsx_writer.h"
#include <libpq-fe.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace pgjr
{
Session::Session(std::function<void()> wake) : wake_(std::move(wake)), thread_([this] { run(); }) {}
Session::~Session()
{
    stopping_ = true;
    cancelled_ = true;
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
}
bool Session::connect(Profile profile, std::string database, std::string password)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (busy_)
        return false;
    work_ = std::make_unique<Work>();
    work_->request = ++latest_;
    work_->connect = true;
    work_->profile = std::move(profile);
    work_->database = std::move(database);
    work_->password = std::move(password);
    busy_ = true;
    cancelled_ = false;
    cv_.notify_all();
    return true;
}
bool Session::execute(std::string sql, bool rollback, std::filesystem::path csv, std::vector<int> widths)
{
    std::lock_guard<std::mutex> guard(mutex_);
    if (busy_ || !connected_)
        return false;
    work_ = std::make_unique<Work>();
    work_->request = ++latest_;
    work_->sql = std::move(sql);
    work_->rollback = rollback;
    work_->csv = std::move(csv);
    work_->widths = std::move(widths);
    busy_ = true;
    cancelled_ = false;
    cv_.notify_all();
    return true;
}
void Session::cancel()
{
    cancelled_ = true;
    cv_.notify_all();
}
std::vector<Event> Session::poll()
{
    std::vector<Event> out;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        while (!events_.empty())
        {
            out.push_back(std::move(events_.front()));
            events_.pop_front();
        }
    }
    cv_.notify_all();
    return out;
}
void Session::post(Event value)
{
    value.request = active_;
    {
        std::unique_lock<std::mutex> guard(mutex_);
        cv_.wait(guard, [this] { return stopping_ || events_.size() < 8; });
        if (stopping_)
            return;
        events_.push_back(std::move(value));
    }
    if (wake_)
        wake_();
}
void Session::run()
{
    while (!stopping_)
    {
        std::unique_ptr<Work> current;
        {
            std::unique_lock<std::mutex> guard(mutex_);
            cv_.wait(guard, [this] { return stopping_ || work_; });
            if (stopping_)
                break;
            current = std::move(work_);
        }
        active_ = current->request;
        try
        {
            if (current->connect)
                open(*current);
            else
                script(*current);
        }
        catch (const std::exception &error)
        {
            Event event;
            event.kind = Event::Finished;
            event.success = false;
            event.text = error.what();
            busy_ = false;
            post(std::move(event));
        }
        std::fill(current->password.begin(), current->password.end(), '\0');
    }
    if (connection_)
        PQfinish(connection_);
}
// libpq performs DNS and authentication on the session worker.
void Session::open(const Work &work)
{
    connected_ = false;
    transaction_ = false;
    if (connection_)
    {
        PQfinish(connection_);
        connection_ = nullptr;
    }
    auto secret = work.password;
    if (secret.empty() && work.profile.remember && !cancelled_ && !stopping_)
        secret = password(work.profile);
    const char *keys[] = {"host",
                          "port",
                          "dbname",
                          "user",
                          "password",
                          "sslmode",
                          "connect_timeout",
                          "application_name",
                          "client_encoding",
                          nullptr};
    const auto timeout = std::to_string(std::clamp(work.profile.timeout, 2, 120));
    const char *values[] = {work.profile.host.c_str(),
                            work.profile.port.c_str(),
                            work.database.c_str(),
                            work.profile.user.c_str(),
                            secret.empty() ? nullptr : secret.c_str(),
                            work.profile.ssl.c_str(),
                            timeout.c_str(),
                            "penthario",
                            "UTF8",
                            nullptr};
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(std::clamp(work.profile.timeout, 2, 120));
    if (!cancelled_ && !stopping_)
        connection_ = PQconnectStartParams(keys, values, 0);
    while (connection_ && PQstatus(connection_) != CONNECTION_BAD && !cancelled_ && !stopping_ &&
           std::chrono::steady_clock::now() < deadline)
    {
        auto state = PQconnectPoll(connection_);
        if (state == PGRES_POLLING_OK || state == PGRES_POLLING_FAILED)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Event event;
    if (!connection_ || PQstatus(connection_) != CONNECTION_OK || cancelled_ || stopping_)
    {
        event.kind = Event::ConnectionError;
        event.success = false;
        event.text = cancelled_ || stopping_                        ? "Connection attempt cancelled"
                     : std::chrono::steady_clock::now() >= deadline ? "Connection timeout"
                     : connection_                                  ? PQerrorMessage(connection_)
                                                                    : "Cannot allocate PostgreSQL connection";
        event.needs_password =
            !cancelled_ && !stopping_ && connection_ && PQconnectionNeedsPassword(connection_);
        if (connection_)
        {
            PQfinish(connection_);
            connection_ = nullptr;
        }
    }
    else
    {
        connected_ = true;
        event.kind = Event::Connected;
        event.database = PQdb(connection_);
        event.backend = PQbackendPID(connection_);
        if (work.profile.remember && !work.password.empty())
        {
            try
            {
                rememberPassword(work.profile, secret);
            }
            catch (const std::exception &error)
            {
                Event message;
                message.text = std::string("Password was not saved: ") + error.what();
                post(std::move(message));
            }
        }
        PQsetnonblocking(connection_, 1);
        PQsetNoticeProcessor(
            connection_,
            [](void *owner, const char *message)
            {
                Event e;
                e.text = message;
                static_cast<Session *>(owner)->post(std::move(e));
            },
            this);
    }
    std::fill(secret.begin(), secret.end(), '\0');
    if (cancelled_ && event.kind == Event::Connected)
    {
        PQfinish(connection_);
        connection_ = nullptr;
        connected_ = false;
        event.kind = Event::ConnectionError;
        event.success = false;
        event.text = "Connection attempt cancelled";
    }
    busy_ = false;
    post(std::move(event));
}
// Drain one command in single-row mode. Cancellation is sent from this same worker using libpq's independent
// handle.
bool Session::command(const std::string &sql, const std::function<void(void *)> &result, bool cleanup)
{
    if (!PQsendQuery(connection_, sql.c_str()))
    {
        connected_ = connection_ && PQstatus(connection_) == CONNECTION_OK;
        return false;
    }
    PQsetSingleRowMode(connection_);
    bool success = true, sent_cancel = false;
    std::exception_ptr callback_error;
    auto stop_started = std::chrono::steady_clock::time_point{};
    for (;;)
    {
        if (!cleanup && (cancelled_ || callback_error) && !sent_cancel)
        {
            sent_cancel = true;
            if (auto *handle = PQgetCancel(connection_))
            {
                char error[256] = {};
                PQcancel(handle, error, sizeof(error));
                PQfreeCancel(handle);
            }
        }
        if (stopping_)
        {
            if (stop_started == std::chrono::steady_clock::time_point{})
                stop_started = std::chrono::steady_clock::now();
            if (std::chrono::steady_clock::now() - stop_started > std::chrono::seconds(3))
                return false;
        }
        int flush = PQflush(connection_);
        if (flush < 0 || !PQconsumeInput(connection_))
        {
            connected_ = false;
            return false;
        }
        if (flush || PQisBusy(connection_))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        auto *value = PQgetResult(connection_);
        if (!value)
            break;
        const auto status = PQresultStatus(value);
        if (status == PGRES_COPY_IN || status == PGRES_COPY_OUT || status == PGRES_COPY_BOTH)
        {
            PQclear(value);
            PQfinish(connection_);
            connection_ = nullptr;
            connected_ = false;
            throw std::runtime_error("COPY streaming is not supported in the editor; connection closed.");
        }
        if (status != PGRES_SINGLE_TUPLE && status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)
            success = false;
        if (!callback_error)
        {
            try
            {
                if (result)
                    result(value);
            }
            catch (...)
            {
                callback_error = std::current_exception();
            }
        }
        PQclear(value);
    }
    if (callback_error)
        std::rethrow_exception(callback_error);
    return success;
}

// Collect bounded previews while optionally streaming every row to CSV. Explain cleanup precedes
// completion/publication.
void Session::script(const Work &work)
{
    std::string error;
    auto statements = splitSql(work.sql, error);
    if (!error.empty())
        throw std::runtime_error(error);
    if (statements.empty())
        throw std::runtime_error("No SQL statements to execute");
    if (statements.size() > 2000)
        throw std::runtime_error("At most 2000 statements per execution.");
    if (auto setting = PQparameterStatus(connection_, "standard_conforming_strings");
        setting && std::string(setting) != "on")
        throw std::runtime_error("standard_conforming_strings must be on.");
    std::string cleanup;
    if (work.rollback && !statements.empty())
    {
        for (const auto &statement : statements)
            if (statement.keyword != "EXPLAIN")
                throw std::runtime_error("Plan mode accepts EXPLAIN statements only.");
        auto state = PQtransactionStatus(connection_);
        if (state != PQTRANS_IDLE && state != PQTRANS_INTRANS)
            throw std::runtime_error("Recover the open transaction with ROLLBACK first.");
        cleanup = state == PQTRANS_INTRANS
                      ? "ROLLBACK TO SAVEPOINT pgjr_explain; RELEASE SAVEPOINT pgjr_explain;"
                      : "ROLLBACK;";
        if (!command(state == PQTRANS_INTRANS ? "SAVEPOINT pgjr_explain;" : "BEGIN;", {}))
            throw std::runtime_error(PQerrorMessage(connection_));
    }
    auto start = std::chrono::steady_clock::now();
    std::ofstream csv;
    std::unique_ptr<XlsxWriter> xlsx;
    auto temporary = work.csv;
    bool success = true;
    std::size_t kept = 0, bytes = 0;
    try
    {
        if (!work.csv.empty())
        {
            temporary += "." + newId() + ".tmp";
            auto extension = work.csv.extension().u8string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension == ".xlsx")
            {
                xlsx = std::make_unique<XlsxWriter>(temporary);
                xlsx->widths(work.widths);
            }
            else
            {
                csv.open(temporary, std::ios::binary | std::ios::trunc);
                csv.exceptions(std::ios::badbit | std::ios::failbit);
                csv << "\xef\xbb\xbf";
            }
        }
        for (std::size_t index = 0; index < statements.size() && success && !cancelled_ && !stopping_;
             ++index)
        {
            Event event;
            event.kind = Event::Batch;
            event.index = index;
            const auto statement_start = std::chrono::steady_clock::now();
            event.section.title = statements[index].title.empty() ? "Query " + std::to_string(index + 1)
                                                                  : statements[index].title;
            bool headers = false;
            std::uint64_t count = 0;
            auto consume = [&](void *raw)
            {
                auto *result = static_cast<PGresult *>(raw);
                auto status = PQresultStatus(result);
                if (!headers)
                {
                    headers = true;
                    for (int col = 0; col < PQnfields(result); ++col)
                        event.section.columns.emplace_back(PQfname(result, col));
                    if (xlsx)
                    {
                        if (statements.size() > 1)
                            xlsx->row({event.section.title}, true);
                        if (!event.section.columns.empty())
                            xlsx->row(event.section.columns, true);
                    }
                    if (csv.is_open())
                    {
                        if (statements.size() > 1)
                            csv << csvField(event.section.title) << "\r\n";
                        for (std::size_t col = 0; col < event.section.columns.size(); ++col)
                        {
                            if (col)
                                csv << ',';
                            csv << csvField(event.section.columns[col]);
                        }
                        if (!event.section.columns.empty())
                            csv << "\r\n";
                    }
                }
                for (int row = 0; row < PQntuples(result); ++row)
                {
                    ++count;
                    if (xlsx)
                    {
                        std::vector<std::string> values;
                        for (int col = 0; col < PQnfields(result); ++col)
                            values.emplace_back(PQgetisnull(result, row, col) ? "NULL"
                                                                              : PQgetvalue(result, row, col));
                        xlsx->row(values);
                    }
                    if (csv.is_open())
                    {
                        for (int col = 0; col < PQnfields(result); ++col)
                        {
                            if (col)
                                csv << ',';
                            csv << csvField(PQgetisnull(result, row, col) ? "NULL"
                                                                          : PQgetvalue(result, row, col));
                        }
                        csv << "\r\n";
                    }
                    if (kept >= 50000 || bytes >= 32 * 1024 * 1024)
                    {
                        event.section.clipped = true;
                        continue;
                    }
                    std::vector<Cell> cells;
                    for (int col = 0; col < PQnfields(result); ++col)
                    {
                        Cell cell;
                        cell.type = PQftype(result, col);
                        cell.null = PQgetisnull(result, row, col) != 0;
                        const auto size = static_cast<std::size_t>(PQgetlength(result, row, col));
                        auto length = std::min(
                            {size, std::size_t(2 * 1024 * 1024),
                             std::size_t(32 * 1024 * 1024) - std::min(bytes, std::size_t(32 * 1024 * 1024))});
                        const auto *field = PQgetvalue(result, row, col);
                        while (length && length < size &&
                               (static_cast<unsigned char>(field[length]) & 0xc0) == 0x80)
                            --length;
                        cell.clipped = length < size;
                        event.section.clipped = event.section.clipped || cell.clipped;
                        if (!cell.null)
                        {
                            if (cell.type == 17 && !cell.clipped)
                            {
                                std::size_t decoded = 0;
                                auto *value = PQunescapeBytea(
                                    reinterpret_cast<const unsigned char *>(PQgetvalue(result, row, col)),
                                    &decoded);
                                if (value)
                                {
                                    cell.binary.assign(value, value + decoded);
                                    PQfreemem(value);
                                }
                            }
                            else
                                cell.text.assign(field, length);
                        }
                        bytes += cell.text.size() + cell.binary.size() + sizeof(Cell);
                        cells.push_back(std::move(cell));
                    }
                    event.section.rows.push_back(std::move(cells));
                    ++kept;
                    if (event.section.rows.size() >= 128)
                    {
                        event.section.count = count;
                        post(event);
                        event.section.rows.clear();
                    }
                }
                if (status == PGRES_FATAL_ERROR || status == PGRES_BAD_RESPONSE)
                {
                    event.section.error = true;
                    event.section.status = PQresultErrorMessage(result);
                }
                else if (status != PGRES_SINGLE_TUPLE)
                    event.section.status = PQcmdStatus(result);
            };
            success = command(statements[index].sql, consume);
            event.section.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - statement_start)
                                           .count();
            event.section.count = count;
            if (event.section.status.empty())
                event.section.status = success ? "Complete" : PQerrorMessage(connection_);
            if (statements.size() > 1 || event.section.columns.empty())
            {
                if (csv.is_open())
                {
                    if (!event.section.status.empty())
                        csv << csvField(event.section.status) << "\r\n";
                    if (statements.size() > 1)
                        csv << "\r\n";
                }
                if (xlsx)
                {
                    if (!event.section.status.empty())
                        xlsx->row({event.section.status});
                    if (statements.size() > 1)
                        xlsx->row({});
                }
            }
            post(std::move(event));
        }
    }
    catch (const std::exception &ex)
    {
        success = false;
        error = ex.what();
    }
    if (!cleanup.empty() && connection_ && PQstatus(connection_) == CONNECTION_OK)
    {
        if (!command(cleanup, {}, true))
        {
            success = false;
            error = "Explain rollback failed: " + std::string(PQerrorMessage(connection_));
        }
    }
    success = success && !cancelled_ && !stopping_;
    if (xlsx)
    {
        try
        {
            xlsx->close();
            xlsx.reset();
            if (success && !cancelled_)
                publishFile(temporary, work.csv);
            else
                success = false;
        }
        catch (const std::exception &ex)
        {
            success = false;
            error = ex.what();
        }
        if (!success)
        {
            xlsx.reset();
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
    }
    if (csv.is_open())
    {
        try
        {
            csv.close();
            if (success)
            {
#ifdef _WIN32
                if (!MoveFileExW(temporary.c_str(), work.csv.c_str(),
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                    throw std::runtime_error("Could not publish CSV.");
#else
                std::filesystem::rename(temporary, work.csv);
#endif
            }
        }
        catch (const std::exception &ex)
        {
            success = false;
            error = ex.what();
        }
        if (!success)
        {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
    }
    connected_ = connection_ && PQstatus(connection_) == CONNECTION_OK;
    transaction_ = connection_ && (PQtransactionStatus(connection_) == PQTRANS_INTRANS ||
                                   PQtransactionStatus(connection_) == PQTRANS_INERROR);
    Event done;
    done.kind = Event::Finished;
    done.success = success;
    done.transaction = transaction_;
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
            .count();
    done.text = (success      ? "Completed"
                 : cancelled_ ? "Cancelled"
                              : "Failed") +
                std::string(" | ") + std::to_string(duration) + " ms | " +
                (transaction_ ? "Transaction open" : "Autocommit") + (error.empty() ? "" : " | " + error);
    busy_ = false;
    post(std::move(done));
}
} // namespace pgjr
