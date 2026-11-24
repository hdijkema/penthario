#pragma once
#include "core.h"
#include <memory>
#include <atomic>
namespace pgjr
{
// Small STL bridge to libxlsxwriter. No C handles escape this class.
class XlsxWriter
{
  public:
    explicit XlsxWriter(const std::filesystem::path &);
    ~XlsxWriter();
    void widths(const std::vector<int> &);
    void row(const std::vector<std::string> &, bool heading = false);
    void close();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
void exportXlsx(const std::filesystem::path &, const std::vector<Section> &, const std::vector<int> &,
                const std::atomic_bool *cancelled = nullptr);
void publishFile(const std::filesystem::path &temporary, const std::filesystem::path &destination);
} // namespace pgjr
