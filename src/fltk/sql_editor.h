#pragma once
#include <FL/Fl_Text_Editor.H>
#include <string>
#include <vector>
namespace pgjr
{
class SqlEditor : public Fl_Text_Editor
{
  public:
    SqlEditor(int, int, int, int);
    int handle(int) override;
    void completionWords(std::vector<std::string> words)
    {
        words_ = std::move(words);
    }
    void complete();
    void indent(bool remove = false);
    void toggleComment();
    bool find(const std::string &, bool backwards = false, bool match_case = false, bool whole = false);
    int replace(const std::string &, const std::string &, bool all, bool match_case = false,
                bool whole = false);

  private:
    std::vector<std::string> words_;
};
} // namespace pgjr
