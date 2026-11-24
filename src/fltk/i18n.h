#pragma once
#include <string>
class Fl_Widget;
namespace pgjr
{
void language(std::string);
const std::string &language();
const char *tr(const char *);
void localize(Fl_Widget &);
int chooseAction(const char *message, const char *cancel, const char *action, const char *alternative);
const char *safeInput(const char *message, const char *value = "");
const char *safePassword(const char *message, const char *value = "");
} // namespace pgjr
