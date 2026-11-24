#include "legacy_workspace.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>
namespace pgjr
{
namespace
{
std::string trim(std::string text)
{
    auto first = text.find_first_not_of(" \r\n\t"), last = text.find_last_not_of(" \r\n\t");
    return first == std::string::npos ? "" : text.substr(first, last - first + 1);
}
int hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
std::string unescape(std::string text)
{
    if (text.size() > 1 && text.front() == '"' && text.back() == '"')
        text = text.substr(1, text.size() - 2);
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] != '\\' || i + 1 == text.size())
        {
            out += text[i];
            continue;
        }
        char c = text[++i];
        if (c == 'x')
        {
            unsigned value = 0;
            int digits = 0;
            while (i + 1 < text.size() && hex(text[i + 1]) >= 0)
            {
                if (++digits > 2)
                    throw std::runtime_error("Invalid Qt byte escape");
                value = value * 16 + hex(text[++i]);
            }
            if (!digits)
                throw std::runtime_error("Invalid Qt escape");
            out += static_cast<char>(value);
        }
        else
        {
            switch (c)
            {
            case '0':
                out += '\0';
                break;
            case 'a':
                out += '\a';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'v':
                out += '\v';
                break;
            default:
                out += c;
                break;
            }
        }
    }
    return out;
}
std::string percent(std::string text)
{
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == '%' && i + 2 < text.size() && hex(text[i + 1]) >= 0 && hex(text[i + 2]) >= 0)
        {
            out += static_cast<char>(hex(text[i + 1]) * 16 + hex(text[i + 2]));
            i += 2;
        }
        else
            out += text[i];
    }
    return out;
}
std::map<std::string, std::string> ini(const std::filesystem::path &file)
{
    auto text = readFile(file);
    if (text.size() > 64 * 1024 * 1024)
        throw std::runtime_error("Qt workspace exceeds the 64 MiB import limit");
    std::istringstream lines(text);
    std::map<std::string, std::string> values;
    std::string line, group;
    while (std::getline(lines, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            group = line.substr(1, line.size() - 2);
            continue;
        }
        auto at = line.find('=');
        if (at == std::string::npos)
            continue;
        auto key = percent(trim(line.substr(0, at)));
        std::replace(key.begin(), key.end(), '\\', '/');
        values[(group == "General" || group.empty() ? "" : group + "/") + key] =
            unescape(trim(line.substr(at + 1)));
    }
    return values;
}
void utf8(std::string &out, std::uint32_t c)
{
    if (c < 0x80)
        out += static_cast<char>(c);
    else if (c < 0x800)
    {
        out += static_cast<char>(0xc0 | (c >> 6));
        out += static_cast<char>(0x80 | (c & 63));
    }
    else if (c < 0x10000)
    {
        out += static_cast<char>(0xe0 | (c >> 12));
        out += static_cast<char>(0x80 | ((c >> 6) & 63));
        out += static_cast<char>(0x80 | (c & 63));
    }
    else
    {
        out += static_cast<char>(0xf0 | (c >> 18));
        out += static_cast<char>(0x80 | ((c >> 12) & 63));
        out += static_cast<char>(0x80 | ((c >> 6) & 63));
        out += static_cast<char>(0x80 | (c & 63));
    }
}
class Reader
{
    const std::string &data_;
    std::size_t at_ = 0;

  public:
    explicit Reader(const std::string &data) : data_(data) {}
    unsigned byte()
    {
        if (at_ >= data_.size())
            throw std::runtime_error("Truncated Qt workspace value");
        return static_cast<unsigned char>(data_[at_++]);
    }
    std::uint32_t integer()
    {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i)
            value = (value << 8) | byte();
        return value;
    }
    std::string string()
    {
        auto size = integer();
        if (size == 0xffffffffu)
            return {};
        if (size % 2 || size > data_.size() - at_)
            throw std::runtime_error("Invalid Qt string length");
        std::string out;
        auto end = at_ + size;
        while (at_ < end)
        {
            auto high = byte();
            std::uint32_t c = (high << 8) | byte();
            if (c >= 0xd800 && c <= 0xdbff)
            {
                if (end - at_ < 2)
                    throw std::runtime_error("Truncated UTF-16 surrogate");
                auto next_high = byte();
                auto next = (next_high << 8) | byte();
                if (next < 0xdc00 || next > 0xdfff)
                    throw std::runtime_error("Invalid UTF-16 surrogate");
                c = 0x10000 + ((c - 0xd800) << 10) + (next - 0xdc00);
            }
            else if (c >= 0xdc00 && c <= 0xdfff)
                throw std::runtime_error("Invalid UTF-16 surrogate");
            utf8(out, c);
        }
        return out;
    }
    Json value(int depth = 0)
    {
        if (depth > 24)
            throw std::runtime_error("Qt workspace nesting limit exceeded");
        auto type = integer();
        if (type == 0)
        {
            string();
            return nullptr;
        }
        if (type == 1)
            return byte() != 0;
        if (type == 2)
            return static_cast<std::int32_t>(integer());
        if (type == 3)
            return integer();
        if (type == 4 || type == 5 || type == 6)
        {
            std::uint64_t bits = static_cast<std::uint64_t>(integer()) << 32;
            bits |= integer();
            if (type == 4)
                return static_cast<std::int64_t>(bits);
            if (type == 5)
                return bits;
            double number;
            std::memcpy(&number, &bits, sizeof(number));
            return number;
        }
        if (type == 10)
            return string();
        if (type == 8 || type == 9 || type == 11)
        {
            auto count = integer();
            if (count > 10000)
                throw std::runtime_error("Qt workspace collection limit exceeded");
            auto result = type == 8 ? Json::object() : Json::array();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                if (type == 8)
                {
                    auto key = string();
                    auto item = value(depth + 1);
                    result[key] = std::move(item);
                }
                else if (type == 11)
                    result.push_back(string());
                else
                    result.push_back(value(depth + 1));
            }
            return result;
        }
        throw std::runtime_error("Unsupported Qt workspace value type: " + std::to_string(type));
    }
    bool ended() const
    {
        return at_ == data_.size();
    }
};
Json variant(const std::map<std::string, std::string> &values, const std::string &key)
{
    auto found = values.find(key);
    if (found == values.end())
        return Json::array();
    const auto &text = found->second;
    if (text.compare(0, 9, "@Variant(") || text.back() != ')')
        throw std::runtime_error("Expected Qt variant list: " + key);
    auto bytes = text.substr(9, text.size() - 10);
    Reader reader(bytes);
    auto result = reader.value();
    if (!reader.ended() || !result.is_array())
        throw std::runtime_error("Invalid Qt list: " + key);
    return result;
}
std::string string(const Json &value, const char *key, const std::string &fallback = {})
{
    auto found = value.find(key);
    return found != value.end() && found->is_string() ? found->get<std::string>() : fallback;
}
} // namespace
Json readQtWorkspace(const std::filesystem::path &file)
{
    auto values = ini(file);
    Json result = {{"profiles", Json::array()},
                   {"tabs", Json::array()},
                   {"history", Json::array()},
                   {"language", values.count("ui/language") ? values["ui/language"] : "nl"},
                   {"active", 0}};
    auto profiles = variant(values, "connections/profiles");
    for (const auto &entry : profiles)
    {
        Profile p;
        p.id = string(entry, "id");
        p.name = string(entry, "name");
        p.host = string(entry, "host", "localhost");
        p.port = entry.contains("port")
                     ? (entry["port"].is_number() ? std::to_string(entry["port"].get<int>())
                                                  : entry["port"].get<std::string>())
                     : "5432";
        p.database = string(entry, "database", "postgres");
        p.user = string(entry, "user", "postgres");
        p.ssl = string(entry, "sslmode", "prefer");
        p.remember = entry.value("rememberPassword", false);
        p.timeout = std::clamp(entry.value("timeout", 10), 2, 120);
        result["profiles"].push_back(p);
    }
    auto target = [&](const Json &entry)
    {
        auto id = string(entry, "server");
        if (id.empty())
        {
            auto value = string(entry, "target");
            for (const auto &p : profiles)
            {
                auto server = string(p, "id");
                if (value == server || value.compare(0, server.size() + 1, server + "/") == 0)
                {
                    id = server;
                    break;
                }
            }
        }
        return id;
    };
    for (const auto &entry : variant(values, "editor/tabs"))
        result["tabs"].push_back({{"profile", target(entry)},
                                  {"database", string(entry, "database", "postgres")},
                                  {"name", string(entry, "tabName", "Query")},
                                  {"sql", string(entry, "sql")},
                                  {"path", string(entry, "path")},
                                  {"library", string(entry, "library")},
                                  {"cursor", entry.value("cursor", 0)},
                                  {"widths", entry.value("widths", Json::array())},
                                  {"bom", entry.value("bom", false)},
                                  {"dirty", entry.value("modified", false)}});
    for (const auto &entry : variant(values, "editor/history"))
    {
        result["history"].push_back({{"profile", target(entry)},
                                     {"database", string(entry, "database", "postgres")},
                                     {"sql", string(entry, "sql")}});
        if (result["history"].size() == 100)
            break;
    }
    if (values.count("editor/activeTab"))
    {
        try
        {
            result["active"] = std::stoi(values["editor/activeTab"]);
        }
        catch (...)
        {
        }
    }
    return result;
}
std::map<std::string, std::string> readQtCategories(const std::filesystem::path &file)
{
    std::map<std::string, std::string> out;
    if (!std::filesystem::exists(file))
        return out;
    for (const auto &entry : ini(file))
        if (entry.first.compare(0, 11, "categories/") == 0)
            out[percent(entry.first.substr(11))] = entry.second;
    return out;
}
} // namespace pgjr
