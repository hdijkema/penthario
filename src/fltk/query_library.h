#pragma once
#include "core.h"
namespace pgjr
{
struct SavedQuery
{
    std::string name, category;
    std::vector<int> widths;
};
class QueryLibrary
{
  public:
    explicit QueryLibrary(std::filesystem::path directory) : directory_(std::move(directory)) {}
    std::vector<SavedQuery> entries() const;
    std::string existingName(const std::string &) const;
    std::string read(const std::string &) const;
    void save(const std::string &, const std::string &, const std::string &category,
              const std::vector<int> &widths = {});
    void rename(const std::string &, const std::string &);
    void remove(const std::string &);
    void categorize(const std::string &, const std::string &);
    void widths(const std::string &, const std::vector<int> &);
    static void validate(const std::string &);

  private:
    Json metadata() const;
    void metadata(const Json &) const;
    std::filesystem::path file(const std::string &) const;
    std::filesystem::path directory_;
};
} // namespace pgjr
