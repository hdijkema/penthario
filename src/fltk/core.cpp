#include "core.h"
#include <keychain/keychain.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <fstream>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

namespace pgjr
{
std::filesystem::path executableDirectory()
{
#ifdef _WIN32
    std::wstring path(32768, L'\0');
    auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size())
        throw std::runtime_error("Cannot locate application");
    path.resize(length);
    return std::filesystem::path(path).parent_path();
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size))
        throw std::runtime_error("Cannot locate application");
    return std::filesystem::canonical(path.c_str()).parent_path();
#else
    return std::filesystem::canonical("/proc/self/exe").parent_path();
#endif
}
void to_json(Json &j, const Profile &p)
{
    j = {{"id", p.id},     {"name", p.name},         {"host", p.host},
         {"port", p.port}, {"user", p.user},         {"database", p.database},
         {"ssl", p.ssl},   {"remember", p.remember}, {"timeout", p.timeout}};
}
void from_json(const Json &j, Profile &p)
{
    p.id = j.value("id", "");
    p.name = j.value("name", "");
    p.host = j.value("host", "localhost");
    p.port = j.value("port", "5432");
    p.user = j.value("user", "postgres");
    p.database = j.value("database", "postgres");
    p.ssl = j.value("ssl", "prefer");
    p.remember = j.value("remember", false);
    p.timeout = std::clamp(j.value("timeout", 10), 2, 120);
}
namespace
{
bool word(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '$';
}
} // namespace
std::vector<Statement> splitSql(const std::string &sql, std::string &error)
{
    error.clear();
    std::vector<Statement> result;
    std::size_t start = 0, i = 0;
    bool content = false;
    std::string title, keyword;
    while (i < sql.size())
    {
        char c = sql[i], next = i + 1 < sql.size() ? sql[i + 1] : 0;
        if (std::isspace(static_cast<unsigned char>(c)))
        {
            ++i;
            continue;
        }
        if (c == '-' && next == '-')
        {
            auto end = sql.find('\n', i);
            if (end == std::string::npos)
                end = sql.size();
            if (!content)
            {
                static const std::regex annotation(R"(^--\s*@resultaat\s*:\s*(.+)$)", std::regex::icase);
                std::smatch match;
                auto comment = sql.substr(i, end - i);
                if (std::regex_match(comment, match, annotation))
                    title = match[1].str();
            }
            i = end;
            continue;
        }
        if (c == '/' && next == '*')
        {
            int depth = 1;
            i += 2;
            while (i < sql.size() && depth)
            {
                if (sql.compare(i, 2, "/*") == 0)
                {
                    ++depth;
                    i += 2;
                }
                else if (sql.compare(i, 2, "*/") == 0)
                {
                    --depth;
                    i += 2;
                }
                else
                    ++i;
            }
            if (depth)
            {
                error = "Unterminated block comment.";
                return {};
            }
            continue;
        }
        if (c == ';')
        {
            if (content)
                result.push_back({sql.substr(start, i - start + 1), title, keyword});
            start = ++i;
            content = false;
            title.clear();
            keyword.clear();
            continue;
        }
        if (c == '\'' || c == '"')
        {
            bool escapes =
                c == '\'' && i && (sql[i - 1] == 'e' || sql[i - 1] == 'E') && (i < 2 || !word(sql[i - 2]));
            content = true;
            ++i;
            bool closed = false;
            while (i < sql.size())
            {
                if (escapes && sql[i] == '\\')
                {
                    i = std::min(sql.size(), i + 2);
                    continue;
                }
                if (sql[i] == c)
                {
                    if (i + 1 < sql.size() && sql[i + 1] == c)
                    {
                        i += 2;
                        continue;
                    }
                    ++i;
                    closed = true;
                    break;
                }
                ++i;
            }
            if (!closed)
            {
                error = "Unterminated SQL string or identifier.";
                return {};
            }
            continue;
        }
        if (c == '$' && (!i || !word(sql[i - 1])))
        {
            auto end = sql.find('$', i + 1);
            if (end != std::string::npos)
            {
                auto tag = sql.substr(i, end - i + 1);
                static const std::regex delimiter(R"(\$(?:[A-Za-z_][A-Za-z_0-9]*)?\$)");
                if (std::regex_match(tag, delimiter))
                {
                    end = sql.find(tag, end + 1);
                    if (end == std::string::npos)
                    {
                        error = "Unterminated dollar-quoted string.";
                        return {};
                    }
                    i = end + tag.size();
                    content = true;
                    continue;
                }
            }
        }
        if (!content && std::isalpha(static_cast<unsigned char>(c)))
        {
            auto end = i + 1;
            while (end < sql.size() && word(sql[end]))
                ++end;
            keyword = sql.substr(i, end - i);
            std::transform(keyword.begin(), keyword.end(), keyword.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        }
        content = true;
        ++i;
    }
    if (content)
        result.push_back({sql.substr(start), title, keyword});
    return result;
}
std::string quoteIdentifier(const std::string &value)
{
    std::string out = "\"";
    for (char c : value)
    {
        if (!c)
            throw std::runtime_error("NUL in identifier");
        if (c == '"')
            out += '"';
        out += c;
    }
    return out + '"';
}
std::string quoteLiteral(const std::string &value)
{
    std::string out = "'";
    for (char c : value)
    {
        if (!c)
            throw std::runtime_error("NUL in literal");
        if (c == '\'')
            out += '\'';
        out += c;
    }
    return out + '\'';
}
std::string csvField(const std::string &value)
{
    std::string out = "\"";
    for (char c : value)
    {
        if (c == '"')
            out += '"';
        out += c;
    }
    return out + '"';
}
std::string readFile(const std::filesystem::path &path)
{
    if (std::filesystem::file_size(path) > 64 * 1024 * 1024)
        throw std::runtime_error("File exceeds the 64 MiB in-memory limit: " + path.u8string());
    std::ifstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot read " + path.u8string());
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}
std::string readSqlFile(const std::filesystem::path &path, bool *bom)
{
    if (std::filesystem::file_size(path) > 8 * 1024 * 1024)
        throw std::runtime_error("SQL editor files are limited to 8 MiB; use Restore for large backups");
    auto text = readFile(path);
    bool has_bom = text.compare(0, 3, "\xef\xbb\xbf") == 0;
    if (bom)
        *bom = has_bom;
    if (has_bom)
        text.erase(0, 3);
    if (text.find('\0') != std::string::npos)
        throw std::runtime_error("SQL files must be UTF-8 text without NUL characters");
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '\r')
        {
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
            normalized += '\n';
        }
        else
            normalized += text[i];
    }
    return normalized;
}
void writeFile(const std::filesystem::path &path, const std::string &text)
{
    auto temporary = path;
    temporary += "." + newId() + ".tmp";
    try
    {
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file.exceptions(std::ios::badbit | std::ios::failbit);
            file.write(text.data(), static_cast<std::streamsize>(text.size()));
            file.close();
        }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot publish " + path.u8string());
#else
        std::filesystem::rename(temporary, path);
#endif
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
std::filesystem::path configDirectory()
{
#ifdef _WIN32
    const wchar_t *base = _wgetenv(L"APPDATA");
    return std::filesystem::path(base ? base : L".") / L"Hans" / L"pgadmin-jr-fltk";
#elif defined(__APPLE__)
    const char *home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / "Library/Application Support/pgadmin-jr-fltk";
#else
    if (auto xdg = std::getenv("XDG_CONFIG_HOME"))
        return std::filesystem::path(xdg) / "pgadmin-jr-fltk";
    const char *home = std::getenv("HOME");
    return std::filesystem::path(home ? home : ".") / ".config/pgadmin-jr-fltk";
#endif
}
std::string newId()
{
    static std::atomic_uint64_t sequence{0};
    return std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
           std::to_string(++sequence);
}
std::string password(const Profile &p)
{
    keychain::Error error;
    auto value = keychain::getPassword("pgadmin-jr-fltk", credentialKey(p), p.user, error);
    if (error && error.type != keychain::ErrorType::NotFound)
        throw std::runtime_error(error.message);
    return value;
}
void rememberPassword(const Profile &p, const std::string &value)
{
    keychain::Error error;
    keychain::setPassword("pgadmin-jr-fltk", credentialKey(p), p.user, value, error);
    if (error)
        throw std::runtime_error(error.message);
}
std::string credentialKey(const Profile &p)
{
    return "connection/" + p.id + "/" + std::to_string(p.host.size()) + ":" + p.host + "/" + p.port;
}
void erasePassword(const Profile &p)
{
    keychain::Error error;
    keychain::deletePassword("pgadmin-jr-fltk", credentialKey(p), p.user, error);
    if (error && error.type != keychain::ErrorType::NotFound)
        throw std::runtime_error(error.message);
}
std::string cellText(const Cell &c)
{
    if (c.null)
        return "NULL";
    if (c.type != 17 || (c.clipped && c.binary.empty()))
        return c.text;
    std::string out = "\\x";
    constexpr char hex[] = "0123456789abcdef";
    for (auto b : c.binary)
    {
        out += hex[b >> 4];
        out += hex[b & 15];
    }
    return out;
}
std::string exportCsv(const std::vector<Section> &sections)
{
    std::string out = "\xef\xbb\xbf";
    for (const auto &section : sections)
    {
        if (sections.size() > 1)
            out += csvField(section.title) + "\r\n";
        for (std::size_t i = 0; i < section.columns.size(); ++i)
        {
            if (i)
                out += ',';
            out += csvField(section.columns[i]);
        }
        out += "\r\n";
        for (const auto &row : section.rows)
        {
            for (std::size_t i = 0; i < row.size(); ++i)
            {
                if (i)
                    out += ',';
                out += csvField(cellText(row[i]));
            }
            out += "\r\n";
        }
        if (section.clipped)
            out += csvField("[PREVIEW CLIPPED]") + "\r\n";
        if (sections.size() > 1)
            out += "\r\n";
    }
    return out;
}
} // namespace pgjr
