#include "query_library.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
namespace pgjr
{
void QueryLibrary::validate(const std::string &name)
{
    if (name.empty() || name.size() > 180 || name.front() == '.' || name.back() == '.' ||
        name.back() == ' ' || name.find_first_of("<>:\"/\\|?*\r\n\t") != std::string::npos)
        throw std::runtime_error("Use a query name without path separators or reserved filename characters");
    for (unsigned char c : name)
        if (c < 32)
            throw std::runtime_error("Control character in query name");
    std::string upper = name.substr(0, name.find('.'));
    for (char &c : upper)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
        (upper.size() == 4 && (upper.substr(0, 3) == "COM" || upper.substr(0, 3) == "LPT") &&
         upper[3] >= '0' && upper[3] <= '9'))
        throw std::runtime_error("Reserved filename");
}
std::filesystem::path QueryLibrary::file(const std::string &name) const
{
    validate(name);
    return directory_ / std::filesystem::u8path(name + ".sql");
}
Json QueryLibrary::metadata() const
{
    auto path = directory_ / "queries.json";
    if (!std::filesystem::exists(path))
        return Json::object();
    return Json::parse(readFile(path));
}
void QueryLibrary::metadata(const Json &value) const
{
    std::filesystem::create_directories(directory_);
    writeFile(directory_ / "queries.json", value.dump(2));
}
std::vector<SavedQuery> QueryLibrary::entries() const
{
    std::vector<SavedQuery> out;
    if (!std::filesystem::exists(directory_))
        return out;
    auto meta = metadata();
    for (const auto &entry : std::filesystem::directory_iterator(directory_))
        if (entry.is_regular_file() && entry.path().extension() == ".sql")
        {
            auto name = entry.path().stem().u8string();
            auto info = meta.value(name, Json::object());
            out.push_back({name, info.value("category", ""), info.value("widths", std::vector<int>{})});
        }
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b)
              { return std::tie(a.category, a.name) < std::tie(b.category, b.name); });
    return out;
}
std::string QueryLibrary::read(const std::string &name) const
{
    return readSqlFile(file(name));
}
std::string QueryLibrary::existingName(const std::string &name) const
{
    auto target = file(name);
    if (!std::filesystem::exists(directory_))
        return {};
    auto lower = [](std::string value)
    {
        for (char &c : value)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return value;
    };
    auto key = lower(name);
    std::string match;
    for (const auto &entry : std::filesystem::directory_iterator(directory_))
        if (entry.is_regular_file() && entry.path().extension() == ".sql")
        {
            auto current = entry.path().stem().u8string();
            if (current == name)
                return current;
            std::error_code error;
            bool same =
                std::filesystem::exists(target) && std::filesystem::equivalent(target, entry.path(), error);
            if (match.empty() && (lower(current) == key || same))
                match = current;
        }
    return match;
}
void QueryLibrary::save(const std::string &name, const std::string &sql, const std::string &category,
                        const std::vector<int> &widths)
{
    auto path = file(name);
    auto meta = metadata();
    std::filesystem::create_directories(directory_);
    writeFile(path, sql);
    meta[name] = {{"category", category}, {"widths", widths}};
    metadata(meta);
}
void QueryLibrary::rename(const std::string &from, const std::string &to)
{
    auto source = file(from), target = file(to);
    if (source == target)
        return;
    if (std::filesystem::exists(target))
        throw std::runtime_error("A query with that name already exists");
    auto meta = metadata();
    auto next = meta;
    next[to] = next.value(from, Json::object());
    next.erase(from);
    std::filesystem::rename(source, target);
    try
    {
        metadata(next);
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::rename(target, source, ignored);
        throw;
    }
}
void QueryLibrary::remove(const std::string &name)
{
    auto path = file(name);
    auto meta = metadata();
    meta.erase(name);
    if (!std::filesystem::remove(path))
        throw std::runtime_error("Query file does not exist");
    metadata(meta);
}
void QueryLibrary::categorize(const std::string &name, const std::string &category)
{
    if (!std::filesystem::exists(file(name)))
        throw std::runtime_error("Query file does not exist");
    auto meta = metadata();
    if (!meta.contains(name))
        meta[name] = Json::object();
    meta[name]["category"] = category;
    metadata(meta);
}
void QueryLibrary::widths(const std::string &name, const std::vector<int> &value)
{
    if (!std::filesystem::exists(file(name)))
        return;
    auto meta = metadata();
    if (!meta.contains(name))
        meta[name] = Json::object();
    meta[name]["widths"] = value;
    metadata(meta);
}
} // namespace pgjr
