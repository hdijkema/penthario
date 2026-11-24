#pragma once
#include "core.h"
#include <atomic>
#include <mutex>
#include <thread>
namespace pgjr
{
enum class Task
{
    Backup,
    Restore,
    Create,
    Drop,
    Vacuum,
    Analyze,
    Reindex
};
struct ToolRequest
{
    Task task = Task::Backup;
    Profile profile;
    std::string database, secret;
    std::filesystem::path file, bin;
    bool plain = false, clean = false, skip_owners = true, schema_only = false;
    std::string schema, relation;
};
std::string nameError(const std::string &);
std::string maintenanceSql(Task, const std::string &database, const std::string &schema = {},
                           const std::string &relation = {}, bool analyze = true, bool concurrent = true);
std::string roleSql(const Json &desired, const Json &original = Json::object());
std::string rightsSql(const std::string &role, const std::string &database, const Json &desired,
                      const Json &original, bool owner);
std::string rolesQuery();
std::string rightsQuery(const std::string &role, const std::string &database);
class ToolJob
{
  public:
    explicit ToolJob(ToolRequest);
    ~ToolJob();
    void cancel()
    {
        cancelled_ = true;
    }
    bool busy() const
    {
        return busy_;
    }
    bool success() const
    {
        return success_;
    }
    std::string output();

  private:
    void run(ToolRequest);
    void log(const std::string &);
    int process(const std::filesystem::path &, const std::vector<std::string> &, const Profile &,
                const std::string &, const std::filesystem::path &, const std::filesystem::path &);
    std::atomic_bool cancelled_{false}, busy_{true}, success_{false};
    std::mutex mutex_;
    std::string log_, pending_;
    std::thread worker_;
};
} // namespace pgjr
