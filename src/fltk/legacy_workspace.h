#pragma once
#include "core.h"
#include <map>
namespace pgjr
{
// Read only the Qt 4.0 QDataStream subset written by QSettings for our former workspace.
// This reader has no Qt dependency and never reads or modifies credential stores.
Json readQtWorkspace(const std::filesystem::path &);
std::map<std::string, std::string> readQtCategories(const std::filesystem::path &);
} // namespace pgjr
