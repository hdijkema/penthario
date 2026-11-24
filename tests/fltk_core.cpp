#include "core.h"
#include "session.h"
#include "tool_job.h"
#include "query_library.h"
#include "xlsx_writer.h"
#include "legacy_workspace.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

void require(bool value, const char *message)
{
    if (!value)
        throw std::runtime_error(message);
}
std::vector<pgjr::Event> wait(pgjr::Session &session)
{
    std::vector<pgjr::Event> events;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    bool complete = false;
    while (std::chrono::steady_clock::now() < deadline)
    {
        for (auto &e : session.poll())
        {
            if (e.kind == pgjr::Event::Finished || e.kind == pgjr::Event::Connected ||
                e.kind == pgjr::Event::ConnectionError)
                complete = true;
            events.push_back(std::move(e));
        }
        if (complete)
            return events;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("Session timeout");
}
int main(int argc, char **argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--xlsx-fixture")
        {
            pgjr::Section section;
            section.title = "Exact values";
            section.columns = {"numeric", "formula text", "Unicode", "empty", "NULL"};
            pgjr::Cell number;
            number.text = "9007199254740993.125";
            pgjr::Cell formula;
            formula.text = "=1+2";
            pgjr::Cell unicode;
            unicode.text = "café ☃";
            pgjr::Cell empty;
            pgjr::Cell null;
            null.null = true;
            section.rows = {{number, formula, unicode, empty, null}};
            section.status = "SELECT 1";
            pgjr::exportXlsx(std::filesystem::u8path(argv[2]), {section}, {220, 180, 150, 80, 80});
            return 0;
        }
        std::string error;
        pgjr::Profile first;
        first.id = "credential-contract";
        auto changed = first;
        changed.database = "other";
        require(pgjr::credentialKey(first) == pgjr::credentialKey(changed),
                "Credential sharing across databases");
        changed.host = "other-server";
        require(pgjr::credentialKey(first) != pgjr::credentialKey(changed),
                "Credentials bound to server host");
        require(!pgjr::Json(first).contains("password"), "Passwords excluded from JSON");
        auto legacy = pgjr::readLegacyWorkspace(std::filesystem::path(PGADM_SOURCE_DIR) /
                                            "tests/fixtures/legacy-workspace.ini");
        require(legacy["profiles"][0]["name"] == "Legacy café" && legacy["profiles"][0]["port"] == "65432",
                "Legacy profile import");
        require(legacy["tabs"][0]["name"] == "Imported tab" &&
                    legacy["tabs"][0]["sql"] == "-- résumé\nSELECT '☃';" &&
                    legacy["tabs"][0]["widths"][1] == 300,
                "Legacy workspace UTF-16/tab import");
        auto statements =
            pgjr::splitSql("-- @resultaat: First\nSELECT ';'; /* /* nested */ ok */ SELECT $$a;b$$;", error);
        require(error.empty() && statements.size() == 2 && statements.front().title == "First",
                "SQL splitter/annotation");
        pgjr::splitSql("SELECT 'open", error);
        require(!error.empty(), "Unterminated SQL must fail");
        require(pgjr::quoteIdentifier("a\"b") == "\"a\"\"b\"", "Identifier escaping");
        require(pgjr::csvField("a\"b") == "\"a\"\"b\"", "CSV escaping");
        auto temp =
            std::filesystem::temp_directory_path() / std::filesystem::u8path("pgjr-test-" + pgjr::newId());
        std::filesystem::create_directory(temp);
        pgjr::writeFile(temp / "data.json", "first");
        pgjr::writeFile(temp / "data.json", "second");
        require(pgjr::readFile(temp / "data.json") == "second", "Atomic file replacement");
        pgjr::QueryLibrary library(temp / "queries");
        library.save("Exact numeric", "SELECT 1;", "Maintenance", {120, 300});
        require(library.entries().size() == 1 && library.entries()[0].category == "Maintenance",
                "Query categories");
        library.rename("Exact numeric", "Renamed");
        require(library.read("Renamed") == "SELECT 1;", "Named query rename");
        library.categorize("Renamed", "Analysis");
        library.widths("Renamed", {240});
        require(library.entries()[0].widths == std::vector<int>{240}, "Named-query widths");
        library.remove("Renamed");
        bool rejected = false;
        try
        {
            library.save("../escape", "SELECT 1;", "");
        }
        catch (...)
        {
            rejected = true;
        }
        require(rejected, "Query path traversal rejected");
        require(pgjr::maintenanceSql(pgjr::Task::Vacuum, "db", "a\"b", "table") ==
                    "VACUUM (VERBOSE, ANALYZE) \"a\"\"b\".\"table\";",
                "Maintenance identifier quoting");
        require(pgjr::roleSql(pgjr::Json{{"name", "odd\"role"}, {"login", true}})
                        .find("CREATE ROLE \"odd\"\"role\" WITH LOGIN") != std::string::npos,
                "Role quoting");
        rejected = false;
        try
        {
            pgjr::roleSql(pgjr::Json{{"name", "pg_reserved"}});
        }
        catch (...)
        {
            rejected = true;
        }
        require(rejected, "Reserved role guard");
        pgjr::writeFile(temp / "limit.xlsx", "previous");
        pgjr::Section oversized;
        pgjr::Cell huge;
        huge.text = std::string(33000, 'x');
        oversized.rows = {{huge}};
        rejected = false;
        try
        {
            pgjr::exportXlsx(temp / "limit.xlsx", {oversized}, {});
        }
        catch (...)
        {
            rejected = true;
        }
        require(rejected && pgjr::readFile(temp / "limit.xlsx") == "previous",
                "Failed XLSX retains destination");
        std::filesystem::remove(temp / "limit.xlsx");
        std::filesystem::remove(temp / "queries/queries.json");
        std::filesystem::remove(temp / "queries");
        std::filesystem::remove(temp / "data.json");
        std::filesystem::remove(temp);
        if (auto port = std::getenv("PGADM_TEST_PORT"))
        {
            auto owned_data = std::getenv("PGADM_TEST_DATA");
            require(owned_data && std::filesystem::exists(std::filesystem::u8path(owned_data) / "PG_VERSION"),
                    "Use the isolated integration runner; an owned test cluster is required");
            pgjr::Profile profile;
            profile.host = "127.0.0.1";
            profile.port = port;
            profile.ssl = "disable";
            pgjr::Session session;
            session.connect(profile, "postgres", "");
            auto events = wait(session);
            require(session.connected(), "Disposable PostgreSQL connection");
            session.execute("SHOW data_directory;");
            events = wait(session);
            bool owned = false;
            for (const auto &event : events)
                for (const auto &row : event.section.rows)
                    if (!row.empty())
                        owned = std::filesystem::equivalent(std::filesystem::u8path(row[0].text),
                                                            std::filesystem::u8path(owned_data));
            require(owned, "Refusing to mutate a server outside the owned test cluster");
            if (auto stall = std::getenv("PGADM_TEST_STALL_PORT"))
            {
                auto stalled = profile;
                stalled.port = stall;
                pgjr::Session connecting;
                auto start = std::chrono::steady_clock::now();
                connecting.connect(stalled, "postgres", "");
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                connecting.cancel();
                auto cancelled = wait(connecting);
                require(!connecting.connected() && !cancelled.back().success &&
                            std::chrono::steady_clock::now() - start < std::chrono::seconds(2),
                        "Cancel a stalled connection handshake");
            }
            session.execute("CREATE TEMP TABLE fltk_test (id int); INSERT INTO fltk_test VALUES (1);");
            events = wait(session);
            require(events.back().success, "Temporary fixture");
            session.execute("EXPLAIN ANALYZE UPDATE fltk_test SET id=2;", true);
            events = wait(session);
            require(events.back().success && !session.inTransaction(), "Explain rollback");
            session.execute("SELECT id FROM fltk_test;");
            events = wait(session);
            bool original = false;
            for (const auto &e : events)
                for (const auto &row : e.section.rows)
                    if (!row.empty() && row[0].text == "1")
                        original = true;
            require(original, "Explain must not commit changes");
            session.execute("BEGIN; UPDATE fltk_test SET id=3;");
            wait(session);
            session.execute("EXPLAIN ANALYZE UPDATE fltk_test SET id=4; EXPLAIN SELECT 1/0;", true);
            events = wait(session);
            require(!events.back().success && session.inTransaction(), "Savepoint cleanup on SQL error");
            session.execute("SELECT id FROM fltk_test;");
            events = wait(session);
            original = false;
            for (const auto &e : events)
                for (const auto &row : e.section.rows)
                    if (!row.empty() && row[0].text == "3")
                        original = true;
            require(original, "Earlier transaction work retained");
            session.execute("ROLLBACK;");
            wait(session);
            session.execute("SELECT pg_sleep(10);");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            session.cancel();
            events = wait(session);
            require(!events.back().success, "Cancellation");
            pgjr::Session other;
            other.connect(profile, "postgres", "");
            wait(other);
            other.execute("SELECT * FROM fltk_test;");
            events = wait(other);
            require(!events.back().success, "Temporary tables stay in their own tab session");
            auto output = std::filesystem::temp_directory_path() /
                          std::filesystem::u8path("pgjr-export-" + pgjr::newId() + ".csv");
            session.execute(
                "SELECT n, 9007199254740993.125::numeric AS exact_value FROM generate_series(1,51000) n;",
                false, output);
            events = wait(session);
            require(events.back().success, "Full CSV export");
            auto csv = pgjr::readFile(output);
            require(csv.find("\"51000\",\"9007199254740993.125\"") != std::string::npos,
                    "Export includes rows beyond preview without numeric loss");
            std::size_t retained = 0;
            bool clipped = false;
            for (const auto &event : events)
            {
                retained += event.section.rows.size();
                clipped = clipped || event.section.clipped;
            }
            require(retained == 50000 && clipped, "Preview is bounded and marked");
            session.execute("SELECT 1; SELECT 1/0;", false, output);
            events = wait(session);
            require(!events.back().success && pgjr::readFile(output) == csv,
                    "Failed export preserves existing destination");
            std::filesystem::remove(output);
            session.execute(
                "SELECT NULL::text, ''::text, '\\x00ff'::bytea, true, repeat(U&'\\2603',700000);");
            events = wait(session);
            bool types = false;
            for (const auto &event : events)
                for (const auto &row : event.section.rows)
                    if (row.size() == 5)
                        types = row[0].null && !row[1].null && row[1].text.empty() &&
                                row[2].binary == std::vector<std::uint8_t>({0, 255}) && row[3].text == "t" &&
                                row[4].clipped && row[4].text.size() % 3 == 0;
            require(types, "NULL, empty string, bytea, bool and large-cell clipping remain distinct");
            auto directory = std::filesystem::temp_directory_path() /
                             std::filesystem::u8path("pgjr-tools-" + pgjr::newId());
            std::filesystem::create_directory(directory);
            auto xlsx = directory / "full.xlsx";
            session.execute("SELECT 9007199254740993.125::numeric, '=1+2'::text;", false, xlsx);
            events = wait(session);
            require(events.back().success && pgjr::readFile(xlsx).substr(0, 2) == "PK",
                    "Full XLSX generation");
            auto runTool = [&](pgjr::ToolRequest request)
            {
                request.profile = profile;
                request.bin = pgjr::executableDirectory();
                pgjr::ToolJob job(std::move(request));
                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
                std::string log;
                while (job.busy() && std::chrono::steady_clock::now() < deadline)
                {
                    log += job.output();
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                log += job.output();
                if (!job.success())
                    std::cerr << log;
                return !job.busy() && job.success();
            };
            auto database = u8"pgjr caf\u00e9 ' " + pgjr::newId();
            pgjr::ToolRequest request;
            request.task = pgjr::Task::Create;
            request.database = database;
            require(runTool(request), "Create database with quoted/spaced name");
            pgjr::Session fixture;
            fixture.connect(profile, database, "");
            wait(fixture);
            fixture.execute(
                "CREATE TABLE fixture (id int, data bytea); INSERT INTO fixture VALUES(1, '\\x00ff');");
            require(wait(fixture).back().success, "Backup fixture");
            auto archive = directory / std::filesystem::u8path(u8"caf\u00e9 \u03bb quoted ' backup.dump");
            request.task = pgjr::Task::Backup;
            request.file = archive;
            require(runTool(request), "Custom backup");
            auto plain = directory / "plain.sql";
            request.file = plain;
            request.plain = true;
            require(runTool(request), "Plain SQL backup");
            fixture.execute("DROP TABLE fixture;");
            wait(fixture);
            request.task = pgjr::Task::Restore;
            request.file = archive;
            request.skip_owners = true;
            require(runTool(request), "Archive restore");
            fixture.execute("SELECT encode(data,'hex') FROM fixture;");
            events = wait(fixture);
            bool restored = false;
            for (const auto &event : events)
                for (const auto &row : event.section.rows)
                    if (!row.empty() && row[0].text == "00ff")
                        restored = true;
            require(restored, "Binary backup data preserved");
            fixture.execute("DROP TABLE fixture;");
            wait(fixture);
            request.file = plain;
            require(runTool(request), "SQL restore through psql");
            for (auto task : {pgjr::Task::Vacuum, pgjr::Task::Analyze, pgjr::Task::Reindex})
            {
                fixture.execute(pgjr::maintenanceSql(task, database, "public", "fixture"));
                require(wait(fixture).back().success, "Independent maintenance execution");
            }
            auto role = "pgjr ' role " + pgjr::newId();
            pgjr::Json desired = {
                {"name", role}, {"login", true}, {"password", "temporary ' test \\ secret"}};
            fixture.execute(pgjr::roleSql(desired));
            require(wait(fixture).back().success, "Create role with password escaping");
            auto updated = desired;
            updated["createdb"] = true;
            fixture.execute(pgjr::roleSql(updated, desired));
            require(wait(fixture).back().success, "Edit role");
            fixture.execute(pgjr::rightsSql(role, database,
                                            pgjr::Json{{"CONNECT", true}, {"CONNECT_grant", true}},
                                            pgjr::Json::object(), false));
            require(wait(fixture).back().success, "Database privilege grant");
            fixture.execute("DROP OWNED BY " + pgjr::quoteIdentifier(role) + "; DROP ROLE " +
                            pgjr::quoteIdentifier(role) + ";");
            require(wait(fixture).back().success, "Role fixture cleanup");
            fixture.execute("SELECT pg_terminate_backend(pg_backend_pid());");
            wait(fixture);
            request.task = pgjr::Task::Drop;
            request.file.clear();
            require(runTool(request), "Drop disposable database");
            for (const auto &file : std::filesystem::directory_iterator(directory))
                std::filesystem::remove(file.path());
            std::filesystem::remove(directory);
        }
        std::cout << "FLTK/STL core contracts passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
