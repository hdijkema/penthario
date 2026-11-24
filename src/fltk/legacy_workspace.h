#pragma once
#include "core.h"
#include <map>
namespace pgjr
{
// Bounded, read-only import of the former INI workspace and its binary value stream.
// Credential stores are never read or modified.
Json readLegacyWorkspace(const std::filesystem::path &);
std::map<std::string, std::string> readLegacyCategories(const std::filesystem::path &);
} // namespace pgjr
