#include "sql_editor.h"
#include <FL/Fl.H>
#include <FL/Fl_Menu_Item.H>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
namespace pgjr
{
namespace
{
bool word(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || static_cast<unsigned char>(c) >= 128;
}
std::string text(Fl_Text_Buffer &buffer)
{
    char *p = buffer.text();
    std::string out = p;
    std::free(p);
    return out;
}
std::string lower(std::string value)
{
    for (char &c : value)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
} // namespace
SqlEditor::SqlEditor(int x, int y, int w, int h) : Fl_Text_Editor(x, y, w, h)
{
    textfont(FL_COURIER);
    textsize(14);
    linenumber_width(48);
    linenumber_font(FL_COURIER);
    linenumber_size(12);
}
int SqlEditor::handle(int event)
{
    if (event == FL_KEYDOWN)
    {
        auto key = Fl::event_key();
        if ((Fl::event_state() & FL_CTRL) && key == ' ')
        {
            complete();
            return 1;
        }
        if ((Fl::event_state() & FL_CTRL) && key == '/')
        {
            toggleComment();
            return 1;
        }
        if (key == FL_Tab)
        {
            indent(Fl::event_state() & FL_SHIFT);
            return 1;
        }
    }
    return Fl_Text_Editor::handle(event);
}
void SqlEditor::indent(bool remove)
{
    if (!buffer())
        return;
    int first = insert_position(), last = first;
    int selected_first = 0, selected_last = 0;
    bool selection = buffer()->selection_position(&selected_first, &selected_last) != 0;
    if (selection)
    {
        first = selected_first;
        last = selected_last;
    }
    if (!selection && !remove)
    {
        buffer()->insert(first, "    ");
        insert_position(first + 4);
        return;
    }
    int cursor = insert_position();
    first = buffer()->line_start(first);
    if (last > first && buffer()->char_at(last - 1) == '\n')
        --last;
    last = buffer()->line_end(last);
    char *raw = buffer()->text_range(first, last);
    std::string value = raw;
    std::free(raw);
    std::string result;
    std::size_t start = 0;
    while (start <= value.size())
    {
        auto end = value.find('\n', start);
        auto line = value.substr(start, end - start);
        if (remove)
        {
            if (!line.empty() && line[0] == '\t')
                line.erase(0, 1);
            else
            {
                std::size_t spaces = 0;
                while (spaces < line.size() && spaces < 4 && line[spaces] == ' ')
                    ++spaces;
                line.erase(0, spaces);
            }
        }
        else
            line = "    " + line;
        result += line;
        if (end == std::string::npos)
            break;
        result += '\n';
        start = end + 1;
    }
    buffer()->replace(first, last, result.c_str());
    if (selection)
        buffer()->select(first, first + static_cast<int>(result.size()));
    else
        insert_position(std::max(first, cursor - static_cast<int>(value.size() - result.size())));
}
void SqlEditor::toggleComment()
{
    if (!buffer())
        return;
    int first = insert_position(), last = first;
    int selected_first = 0, selected_last = 0;
    if (buffer()->selection_position(&selected_first, &selected_last))
    {
        first = selected_first;
        last = selected_last;
    }
    first = buffer()->line_start(first);
    if (last > first && buffer()->char_at(last - 1) == '\n')
        --last;
    last = buffer()->line_end(last);
    char *raw = buffer()->text_range(first, last);
    std::string value = raw;
    std::free(raw);
    bool remove = true;
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= value.size())
    {
        auto end = value.find('\n', start);
        lines.push_back(value.substr(start, end - start));
        auto pos = lines.back().find_first_not_of(" \t");
        if (pos != std::string::npos && lines.back().compare(pos, 2, "--"))
            remove = false;
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    std::string result;
    bool first_line = true;
    for (auto &line : lines)
    {
        auto at = line.find_first_not_of(" \t");
        if (remove && at != std::string::npos)
        {
            line.erase(at, line.compare(at, 3, "-- ") == 0 ? 3 : 2);
        }
        else if (!remove)
            line = "-- " + line;
        if (!first_line)
            result += '\n';
        first_line = false;
        result += line;
    }
    buffer()->replace(first, last, result.c_str());
    buffer()->select(first, first + static_cast<int>(result.size()));
}
void SqlEditor::complete()
{
    if (!buffer())
        return;
    auto source = text(*buffer());
    int end = insert_position(), start = end;
    while (start > 0 && word(source[start - 1]))
        --start;
    auto prefix = lower(source.substr(start, end - start));
    std::set<std::string> words(words_.begin(), words_.end());
    for (auto key : {"SELECT",  "FROM",      "WHERE", "INSERT",    "UPDATE",   "DELETE",   "CREATE",
                     "ALTER",   "DROP",      "JOIN",  "LEFT JOIN", "GROUP BY", "ORDER BY", "EXPLAIN",
                     "ANALYZE", "RETURNING", "BEGIN", "COMMIT",    "ROLLBACK", "VACUUM",   "LIMIT",
                     "WITH",    "DISTINCT",  "NULL",  "TRUE",      "FALSE"})
        words.insert(key);
    for (std::size_t i = 0; i < source.size();)
    {
        if (!word(source[i]))
        {
            ++i;
            continue;
        }
        auto first = i;
        while (i < source.size() && word(source[i]))
            ++i;
        words.insert(source.substr(first, i - first));
    }
    std::vector<std::string> matches;
    for (const auto &candidate : words)
        if (lower(candidate).compare(0, prefix.size(), prefix) == 0 &&
            candidate != source.substr(start, end - start))
        {
            matches.push_back(candidate);
            if (matches.size() == 40)
                break;
        }
    if (matches.empty())
        return;
    std::vector<Fl_Menu_Item> menu(matches.size() + 1);
    for (std::size_t i = 0; i < matches.size(); ++i)
        menu[i].label(matches[i].c_str());
    int x = this->x() + 55, y = this->y() + 25;
    position_to_xy(end, &x, &y);
    auto *chosen = menu.data()->popup(x, y + 20);
    if (chosen)
    {
        buffer()->replace(start, end, chosen->label());
        insert_position(start + static_cast<int>(std::string(chosen->label()).size()));
    }
}
bool SqlEditor::find(const std::string &needle, bool backwards, bool match_case, bool whole)
{
    if (!buffer() || needle.empty())
        return false;
    auto source = text(*buffer()), haystack = match_case ? source : lower(source),
         pattern = match_case ? needle : lower(needle);
    int a = 0, b = 0;
    bool selected = buffer()->selection_position(&a, &b) != 0;
    std::size_t begin = static_cast<std::size_t>(
        std::max(0, backwards ? (selected ? a : insert_position()) - 1 : (selected ? b : insert_position())));
    if (backwards && (selected ? a : insert_position()) <= 0)
        begin = source.size();
    for (int pass = 0; pass < 2; ++pass)
    {
        auto at = backwards ? haystack.rfind(pattern, begin) : haystack.find(pattern, begin);
        while (at != std::string::npos)
        {
            if (!whole || ((at == 0 || !word(source[at - 1])) &&
                           (at + pattern.size() == source.size() || !word(source[at + pattern.size()]))))
            {
                buffer()->select(static_cast<int>(at), static_cast<int>(at + needle.size()));
                insert_position(static_cast<int>(at));
                show_insert_position();
                return true;
            }
            if (backwards)
            {
                if (!at)
                    break;
                at = haystack.rfind(pattern, at - 1);
            }
            else
                at = haystack.find(pattern, at + 1);
        }
        begin = backwards ? source.size() : 0;
    }
    return false;
}
int SqlEditor::replace(const std::string &needle, const std::string &replacement, bool all, bool match_case,
                       bool whole)
{
    if (!buffer() || needle.empty())
        return 0;
    if (!all)
    {
        int a = 0, b = 0;
        if (buffer()->selection_position(&a, &b))
        {
            char *raw = buffer()->selection_text();
            std::string selected = raw;
            std::free(raw);
            auto source = text(*buffer());
            bool boundaries = !whole || ((a == 0 || !word(source[a - 1])) &&
                                         (b == static_cast<int>(source.size()) || !word(source[b])));
            if (boundaries &&
                (match_case ? selected : lower(selected)) == (match_case ? needle : lower(needle)))
            {
                buffer()->replace(a, b, replacement.c_str());
                insert_position(a + static_cast<int>(replacement.size()));
                return 1;
            }
        }
        find(needle, false, match_case, whole);
        return 0;
    }
    auto source = text(*buffer()), haystack = match_case ? source : lower(source),
         pattern = match_case ? needle : lower(needle);
    std::string result;
    std::size_t begin = 0, at = 0;
    int count = 0;
    while ((at = haystack.find(pattern, begin)) != std::string::npos)
    {
        if (whole && ((at && word(source[at - 1])) ||
                      (at + pattern.size() < source.size() && word(source[at + pattern.size()]))))
        {
            result += source.substr(begin, at + 1 - begin);
            begin = at + 1;
            continue;
        }
        result += source.substr(begin, at - begin) + replacement;
        begin = at + pattern.size();
        ++count;
    }
    result += source.substr(begin);
    if (count)
    {
        buffer()->replace(0, buffer()->length(), result.c_str());
        insert_position(0);
    }
    return count;
}
} // namespace pgjr
