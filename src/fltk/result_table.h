#pragma once
#include "core.h"
#include <fltk_cell.h>
#include <fltk_cell_css.h>
#include <FL/Fl_Table.H>

namespace pgjr
{
// A read-only FLTK view over bounded STL result sections; all cell painting is delegated to fltk-cell.
class ResultTable : public Fl_Table
{
  public:
    ResultTable(int, int, int, int, FLTK_Css &);
    void clear(bool all);
    void append(std::size_t index, Section batch);
    const std::vector<Section> &sections() const
    {
        return sections_;
    }
    std::function<void(const Cell &)> inspect;
    std::string copyText() const;
    std::string columnLabel(int) const;
    std::vector<int> widths();
    void widths(const std::vector<int> &);

  protected:
    int handle(int) override;
    void draw_cell(TableContext, int, int, int, int, int, int) override;

  private:
    struct Row
    {
        std::size_t section, row;
        enum Kind
        {
            Title,
            Columns,
            Data,
            Status
        } kind;
    };
    int resizeColumnAt(int, int);
    int resizing_column_ = -1, resize_start_x_ = 0, resize_start_width_ = 0;
    void rebuild();
    FLTK_Css &css_;
    std::vector<Section> sections_;
    std::vector<Row> rows_;
    std::vector<int> saved_widths_;
    fltk_cell::TableCellTextDrawer text_;
    fltk_cell::TableCellNumberDrawer number_;
    fltk_cell::TableCellHexDrawer hex_;
    fltk_cell::TableCellCheckBoxDrawer check_;
    bool all_ = true;
    std::size_t last_ = 0;
};
} // namespace pgjr
