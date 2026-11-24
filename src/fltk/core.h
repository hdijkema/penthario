#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace pgjr
{
using Json = nlohmann::json;
struct Profile
{
    std::string id, name, host = "localhost", port = "5432", user = "postgres", database = "postgres",
                          ssl = "prefer";
    bool remember = false;
    int timeout = 10;
};
void to_json(Json &, const Profile &);
void from_json(const Json &, Profile &);
struct Statement
{
    std::string sql, title, keyword;
};
// Split PostgreSQL statements outside strings, dollar bodies and nested comments; preserve result
// annotations.
std::vector<Statement> splitSql(const std::string &, std::string &error);
std::string quoteIdentifier(const std::string &);
std::string quoteLiteral(const std::string &);
std::string csvField(const std::string &);
std::string readFile(const std::filesystem::path &);
std::string readSqlFile(const std::filesystem::path &, bool *bom = nullptr);
// Write beside the destination and atomically replace it; failed publication preserves the previous file.
void writeFile(const std::filesystem::path &, const std::string &);
// Separate workspace; former settings are read only through explicit import.
std::filesystem::path configDirectory();
std::filesystem::path executableDirectory();
std::string newId();
std::string password(const Profile &);
std::string credentialKey(const Profile &);
void erasePassword(const Profile &);
void rememberPassword(const Profile &, const std::string &);
struct Cell
{
    std::string text;
    std::vector<std::uint8_t> binary;
    bool null = false, clipped = false;
    std::uint32_t type = 0;
};
struct Section
{
    std::string title, status;
    std::vector<std::string> columns;
    std::vector<std::vector<Cell>> rows;
    std::uint64_t count = 0;
    std::int64_t elapsed_ms = 0;
    bool clipped = false, error = false;
};
std::string cellText(const Cell &);
std::string exportCsv(const std::vector<Section> &);
} // namespace pgjr
