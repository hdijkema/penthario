#include "tool_job.h"
#include "xlsx_writer.h"
#include <chrono>
#include <fstream>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
extern char **environ;
#endif
namespace pgjr
{
ToolJob::ToolJob(ToolRequest request)
    : worker_([this, request = std::move(request)]() mutable { run(std::move(request)); })
{
}
ToolJob::~ToolJob()
{
    cancel();
    if (worker_.joinable())
        worker_.join();
}
void ToolJob::log(const std::string &text)
{
    std::lock_guard<std::mutex> guard(mutex_);
    pending_ += text;
    auto end = pending_.size(), at = end;
    while (at && (static_cast<unsigned char>(pending_[at - 1]) & 0xc0) == 0x80)
        --at;
    if (at && static_cast<unsigned char>(pending_[at - 1]) >= 0xc0)
    {
        auto lead = static_cast<unsigned char>(pending_[at - 1]);
        unsigned expected = lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : 2;
        if (end - (at - 1) < expected)
            end = at - 1;
    }
    if (log_.size() + end > 2 * 1024 * 1024)
        log_ = "[Earlier log output omitted]\n";
    log_.append(pending_, 0, end);
    pending_.erase(0, end);
}
std::string ToolJob::output()
{
    std::lock_guard<std::mutex> guard(mutex_);
    auto text = std::move(log_);
    log_.clear();
    return text;
}
namespace
{
std::string databaseArgument(const std::string &value)
{
    std::string out = "dbname='";
    for (char c : value)
    {
        if (c == '\\' || c == '\'')
            out += '\\';
        out += c;
    }
    return out + "'";
}
#ifdef _WIN32
std::wstring wide(const std::string &text, unsigned codepage = CP_UTF8)
{
    int n = MultiByteToWideChar(codepage, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                nullptr, 0);
    if (!n && !text.empty())
        throw std::runtime_error("Invalid UTF-8 process argument");
    std::wstring out(n, L'\0');
    MultiByteToWideChar(codepage, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}
std::wstring argument(const std::wstring &value)
{
    std::wstring out = L"\"";
    int slashes = 0;
    for (auto c : value)
    {
        if (c == L'\\')
        {
            ++slashes;
            continue;
        }
        if (c == L'"')
            out.append(slashes * 2 + 1, L'\\');
        else
            out.append(slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    return out + L'"';
}
struct Handle
{
    HANDLE value = nullptr;
    ~Handle()
    {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }
};
#endif
} // namespace
int ToolJob::process(const std::filesystem::path &executable, const std::vector<std::string> &arguments,
                     const Profile &profile, const std::string &secret,
                     const std::filesystem::path &input_file, const std::filesystem::path &output_file)
{
    std::map<std::string, std::string> pg = {
        {"PGHOST", profile.host},
        {"PGPORT", profile.port},
        {"PGUSER", profile.user},
        {"PGDATABASE", profile.database},
        {"PGSSLMODE", profile.ssl},
        {"PGCONNECT_TIMEOUT", std::to_string(std::clamp(profile.timeout, 2, 120))},
        {"PGCLIENTENCODING", "UTF8"},
        {"PGAPPNAME", "penthario tools"}};
    if (!secret.empty())
        pg["PGPASSWORD"] = secret;
    auto ignored = [&](const std::string &key)
    { return pg.count(key) || key == "PGHOSTADDR" || key == "PGSERVICE" || key == "PGPASSWORD"; };
#ifdef _WIN32
    Handle read, write, child, thread, input, output;
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    input.value = CreateFileW(input_file.empty() ? L"NUL" : input_file.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    if (input.value == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Cannot prepare child process input");
    if (!output_file.empty())
    {
        output.value = CreateFileW(output_file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &security, CREATE_NEW,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output.value == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create backup staging file");
    }
    if (!CreatePipe(&read.value, &write.value, &security, 0) ||
        !SetHandleInformation(read.value, HANDLE_FLAG_INHERIT, 0))
        throw std::runtime_error("Cannot create process output pipe");
    std::wstring environment;
    auto *inherited = GetEnvironmentStringsW();
    if (!inherited)
        throw std::runtime_error("Cannot read process environment");
    for (auto *entry = inherited; *entry; entry += wcslen(entry) + 1)
    {
        std::wstring line = entry;
        auto at = line.find(L'=', 1);
        auto key = line.substr(0, at);
        std::string ascii;
        for (auto c : key)
            ascii += c < 128 ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : '?';
        if (!ignored(ascii))
        {
            environment += line;
            environment += L'\0';
        }
    }
    FreeEnvironmentStringsW(inherited);
    for (const auto &pair : pg)
    {
        // PostgreSQL's Windows tools use narrow argv/getenv bytes with UTF8
        // client encoding. Preserve those bytes through the child's ANSI CRT.
        environment += wide(pair.first + "=" + pair.second, CP_ACP);
        environment += L'\0';
    }
    environment += L'\0';
    std::vector<std::wstring> lines;
    for (const auto *entry = environment.c_str(); *entry; entry += wcslen(entry) + 1)
        lines.emplace_back(entry);
    std::sort(lines.begin(), lines.end(),
              [](const auto &a, const auto &b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; });
    environment.clear();
    for (auto &line : lines)
    {
        environment += line;
        environment += L'\0';
        std::fill(line.begin(), line.end(), L'\0');
    }
    environment += L'\0';
    std::wstring command = argument(executable.wstring());
    for (const auto &a : arguments)
        command += L" " + argument(wide(a, CP_ACP));
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = output_file.empty() ? write.value : output.value;
    startup.hStdError = write.value;
    startup.hStdInput = input.value;
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, environment.data(), nullptr, &startup,
                        &info))
        throw std::runtime_error("Cannot start PostgreSQL tool (Windows error " +
                                 std::to_string(GetLastError()) + ")");
    child.value = info.hProcess;
    thread.value = info.hThread;
    CloseHandle(write.value);
    write.value = nullptr;
    std::fill(environment.begin(), environment.end(), L'\0');
    bool killed = false;
    for (;;)
    {
        if (cancelled_ && !killed)
        {
            TerminateProcess(child.value, 1);
            killed = true;
        }
        DWORD available = 0;
        while (PeekNamedPipe(read.value, nullptr, 0, nullptr, &available, nullptr) && available)
        {
            char buffer[16384];
            DWORD count = 0;
            if (!ReadFile(read.value, buffer,
                          static_cast<DWORD>(std::min<std::size_t>(sizeof(buffer), available)), &count,
                          nullptr) ||
                !count)
                break;
            log(std::string(buffer, count));
        }
        if (WaitForSingleObject(child.value, 20) == WAIT_OBJECT_0)
        {
            while (PeekNamedPipe(read.value, nullptr, 0, nullptr, &available, nullptr) && available)
            {
                char buffer[16384];
                DWORD count = 0;
                if (!ReadFile(read.value, buffer, sizeof(buffer), &count, nullptr) || !count)
                    break;
                log(std::string(buffer, count));
            }
            break;
        }
    }
    DWORD code = 1;
    GetExitCodeProcess(child.value, &code);
    return static_cast<int>(code);
#else
    std::vector<std::string> environment;
    for (char **entry = environ; *entry; ++entry)
    {
        std::string line = *entry;
        if (!ignored(line.substr(0, line.find('='))))
            environment.push_back(std::move(line));
    }
    for (const auto &pair : pg)
        environment.push_back(pair.first + "=" + pair.second);
    std::vector<char *> envp;
    for (auto &line : environment)
        envp.push_back(line.data());
    envp.push_back(nullptr);
    std::vector<std::string> args{executable.string()};
    args.insert(args.end(), arguments.begin(), arguments.end());
    std::vector<char *> argv;
    for (auto &a : args)
        argv.push_back(a.data());
    argv.push_back(nullptr);
    int pipes[2];
    if (pipe(pipes))
        throw std::runtime_error("Cannot create process pipe");
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO,
                                     input_file.empty() ? "/dev/null" : input_file.c_str(), O_RDONLY, 0);
    if (output_file.empty())
        posix_spawn_file_actions_adddup2(&actions, pipes[1], STDOUT_FILENO);
    else
        posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output_file.c_str(),
                                         O_WRONLY | O_CREAT | O_EXCL, 0600);
    posix_spawn_file_actions_adddup2(&actions, pipes[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipes[0]);
    posix_spawn_file_actions_addclose(&actions, pipes[1]);
    pid_t pid = 0;
    int error = posix_spawn(&pid, executable.c_str(), &actions, nullptr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    close(pipes[1]);
    for (auto &e : environment)
        std::fill(e.begin(), e.end(), '\0');
    if (error)
    {
        close(pipes[0]);
        throw std::runtime_error("Cannot start PostgreSQL tool (errno " + std::to_string(error) + ")");
    }
    fcntl(pipes[0], F_SETFL, O_NONBLOCK);
    int status = 0;
    bool killed = false;
    auto cancel_time = std::chrono::steady_clock::time_point{};
    for (;;)
    {
        if (cancelled_ && !killed)
        {
            kill(pid, SIGTERM);
            cancel_time = std::chrono::steady_clock::now();
            killed = true;
        }
        if (killed && std::chrono::steady_clock::now() - cancel_time > std::chrono::seconds(2))
            kill(pid, SIGKILL);
        char buffer[16384];
        ssize_t count;
        while ((count = read(pipes[0], buffer, sizeof(buffer))) > 0)
            log(std::string(buffer, count));
        auto result = waitpid(pid, &status, WNOHANG);
        if (result == pid)
        {
            while ((count = read(pipes[0], buffer, sizeof(buffer))) > 0)
                log(std::string(buffer, count));
            break;
        }
        if (result < 0)
        {
            close(pipes[0]);
            throw std::runtime_error("Cannot wait for PostgreSQL tool");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    close(pipes[0]);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}
void ToolJob::run(ToolRequest request)
{
    std::filesystem::path staging;
    try
    {
        auto error = nameError(request.database);
        if (!error.empty())
            throw std::runtime_error(error);
        if (request.task == Task::Drop &&
            (request.database == "postgres" || request.database == "template0" ||
             request.database == "template1" || request.database == request.profile.database))
            throw std::runtime_error("System and maintenance databases cannot be dropped here");
        std::vector<std::string> args{"--no-password"};
        std::filesystem::path input_file, output_file;
        std::string tool;
        if (request.task == Task::Backup)
        {
            if (request.file.empty())
                throw std::runtime_error("Select a backup file");
            staging = request.file;
            staging += "." + newId() + ".tmp";
            tool = "pg_dump";
            args.insert(args.end(), {"--verbose", request.plain ? "--format=p" : "--format=c",
                                     "--encoding=UTF8", "--dbname=" + databaseArgument(request.database)});
            output_file = staging;
            if (request.schema_only)
            {
                args.insert(args.end(), {"--schema-only", "--no-owner", "--no-privileges"});
                if (!request.relation.empty())
                    args.insert(args.end(), {"--strict-names", "--table=" + quoteIdentifier(request.schema) +
                                                                   "." + quoteIdentifier(request.relation)});
            }
        }
        else if (request.task == Task::Restore)
        {
            std::ifstream file(request.file, std::ios::binary);
            if (!file)
                throw std::runtime_error("Cannot read backup file");
            input_file = std::filesystem::absolute(request.file);
            char bytes[512] = {};
            file.read(bytes, sizeof(bytes));
            bool archive = std::string(bytes, 5) == "PGDMP" || std::string(bytes + 257, 5) == "ustar";
            if (archive)
            {
                tool = "pg_restore";
                args.insert(args.end(), {"--verbose", "--single-transaction", "--exit-on-error",
                                         "--dbname=" + databaseArgument(request.database)});
                if (request.clean)
                    args.insert(args.end(), {"--clean", "--if-exists"});
                if (request.skip_owners)
                    args.insert(args.end(), {"--no-owner", "--no-privileges"});
                // pg_restore reads the archive from its native stdin handle.
            }
            else
            {
                if (request.clean)
                    throw std::runtime_error("Clean restore is only supported for PostgreSQL archives");
                tool = "psql";
                args.insert(args.end(), {"--no-psqlrc", "--single-transaction", "--set=ON_ERROR_STOP=on",
                                         "--dbname=" + databaseArgument(request.database), "--file=-"});
            }
        }
        else if (request.task == Task::Create)
        {
            tool = "createdb";
            args.insert(args.end(), {"--encoding=UTF8", "--template=template0",
                                     "--maintenance-db=" + databaseArgument(request.profile.database), "--",
                                     request.database});
        }
        else if (request.task == Task::Drop)
        {
            tool = "dropdb";
            args.insert(args.end(), {"--maintenance-db=" + databaseArgument(request.profile.database), "--",
                                     request.database});
        }
        else
            throw std::runtime_error("Use a database session for maintenance");
#ifdef _WIN32
        auto executable = request.bin / (tool + ".exe");
#else
        auto executable = request.bin / tool;
        if (!std::filesystem::exists(executable))
        {
            auto path = std::getenv("PATH");
            std::string paths = path ? path : "";
            std::size_t begin = 0;
            while (begin <= paths.size())
            {
                auto end = paths.find(':', begin);
                auto candidate = std::filesystem::path(paths.substr(begin, end - begin)) / tool;
                if (access(candidate.c_str(), X_OK) == 0)
                {
                    executable = candidate;
                    break;
                }
                if (end == std::string::npos)
                    break;
                begin = end + 1;
            }
        }
#endif
        if (!std::filesystem::exists(executable))
            throw std::runtime_error("PostgreSQL tool is missing: " + tool);
        if (request.secret.empty() && request.profile.remember)
            request.secret = password(request.profile);
        if (cancelled_)
            throw std::runtime_error("Cancelled before starting PostgreSQL tool");
        log("Started " + tool + " for " + request.database + "\n");
        int code = process(executable, args, request.profile, request.secret, input_file, output_file);
        std::fill(request.secret.begin(), request.secret.end(), '\0');
        if (cancelled_)
            throw std::runtime_error(
                "Cancelled. Completed database operations may remain; backup destination was preserved.");
        if (code)
            throw std::runtime_error("PostgreSQL tool failed (exit code " + std::to_string(code) +
                                     "); see the log");
        if (!staging.empty())
            publishFile(staging, request.file);
        success_ = true;
        log("Completed successfully\n");
    }
    catch (const std::exception &e)
    {
        log(std::string(e.what()) + '\n');
        if (!staging.empty())
        {
            std::error_code ignored;
            std::filesystem::remove(staging, ignored);
        }
    }
    std::fill(request.secret.begin(), request.secret.end(), '\0');
    busy_ = false;
}
} // namespace pgjr
