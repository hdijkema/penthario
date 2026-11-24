#include "result_table.h"
#include "i18n.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <algorithm>
#include <cstdlib>

namespace pgjr
{
ResultTable::ResultTable(int x, int y, int w, int h, FLTK_Css &css) : Fl_Table(x, y, w, h), css_(css)
{
    color(FL_BACKGROUND2_COLOR);
    cols(1);
    rows(0);
    col_width_all(200);
    col_resize(1);
    row_height_all(27);
    row_header(0);
    col_header(1);
    col_header_height(28);
    tooltip("Drag column-header borders to resize. Double-click a cell for details.");
    col_resize_min(40);
    when(FL_WHEN_RELEASE);
    end();
    number_.format.decimals = -1;
    callback(
        [](Fl_Widget *widget, void *)
        {
            auto &table = *static_cast<ResultTable *>(widget);
            if (Fl::event() != FL_RELEASE || Fl::event_button() != FL_LEFT_MOUSE ||
                table.callback_context() != CONTEXT_CELL || !Fl::event_clicks())
                return;
            auto row = table.callback_row(), col = table.callback_col();
            if (row < 0 || col < 0 || static_cast<std::size_t>(row) >= table.rows_.size())
                return;
            const auto &ref = table.rows_[row];
            if (ref.kind != Row::Data)
                return;
            const auto &values = table.sections_[ref.section].rows[ref.row];
            if (static_cast<std::size_t>(col) < values.size() && table.inspect)
            {
                Fl::event_clicks(0);
                table.inspect(values[col]);
            }
        });
}
// FLTK's built-in header grip is only three pixels wide. Keep the visual border
// thin, but accept an eight-pixel margin on either side for both hover and drag.
int ResultTable::resizeColumnAt(int px, int py)
{
    if (!col_resize() || !col_header())
        return -1;
    int hx, hy, hw, hh;
    get_bounds(CONTEXT_COL_HEADER, hx, hy, hw, hh);
    if (px < hx || px >= hx + hw || py < hy || py >= hy + hh)
        return -1;
    int nearest = -1, distance = 9;
    for (int col = std::max(0, leftcol - 1); col <= rightcol && col < cols(); ++col)
    {
        int x, y, w, h;
        if (find_cell(CONTEXT_COL_HEADER, 0, col, x, y, w, h) < 0)
            continue;
        const int border = x + w;
        const int delta = std::abs(px - border);
        if (border >= hx && border < hx + hw && delta < distance)
        {
            nearest = col;
            distance = delta;
        }
    }
    return nearest;
}
int ResultTable::handle(int event)
{
    if (resizing_column_ >= 0)
    {
        if (event == FL_DRAG)
        {
            col_width(resizing_column_, std::clamp(resize_start_width_ + Fl::event_x() - resize_start_x_,
                                                   col_resize_min(), 4096));
            change_cursor(FL_CURSOR_WE);
            if (window())
                window()->cursor(FL_CURSOR_WE);
            redraw();
            return 1;
        }
        if (event == FL_RELEASE)
        {
            resizing_column_ = -1;
            change_cursor(resizeColumnAt(Fl::event_x(), Fl::event_y()) >= 0 ? FL_CURSOR_WE
                                                                            : FL_CURSOR_DEFAULT);
            return 1;
        }
    }
    if (event == FL_MOVE || event == FL_PUSH)
    {
        const int col = resizeColumnAt(Fl::event_x(), Fl::event_y());
        if (col >= 0 && (event == FL_MOVE || Fl::event_button() == FL_LEFT_MOUSE))
        {
            change_cursor(FL_CURSOR_WE);
            if (window())
                window()->cursor(FL_CURSOR_WE);
            if (event == FL_PUSH)
            {
                resizing_column_ = col;
                resize_start_x_ = Fl::event_x();
                resize_start_width_ = col_width(col);
            }
            return 1;
        }
    }
    return Fl_Table::handle(event);
}
void ResultTable::clear(bool all)
{
    sections_.clear();
    rows_.clear();
    all_ = all;
    last_ = 0;
    rows(0);
    redraw();
}
void ResultTable::append(std::size_t index, Section batch)
{
    if (!all_ && index != last_)
        sections_.clear();
    last_ = index;
    auto position = all_ ? index : 0;
    if (sections_.size() <= position)
        sections_.resize(position + 1);
    auto &section = sections_[position];
    section.title = batch.title;
    section.columns = std::move(batch.columns);
    section.status = std::move(batch.status);
    section.count = batch.count;
    section.elapsed_ms = batch.elapsed_ms;
    section.clipped = section.clipped || batch.clipped;
    section.error = batch.error;
    section.rows.insert(section.rows.end(), std::make_move_iterator(batch.rows.begin()),
                        std::make_move_iterator(batch.rows.end()));
    rebuild();
}
void ResultTable::rebuild()
{
    rows_.clear();
    int columns = 1;
    for (std::size_t i = 0; i < sections_.size(); ++i)
    {
        const auto &section = sections_[i];
        columns = std::max(columns, static_cast<int>(section.columns.size()));
        if (sections_.size() > 1)
            rows_.push_back({i, 0, Row::Title});
        if (sections_.size() > 1 && !section.columns.empty())
            rows_.push_back({i, 0, Row::Columns});
        for (std::size_t row = 0; row < section.rows.size(); ++row)
            rows_.push_back({i, row, Row::Data});
        rows_.push_back({i, 0, Row::Status});
    }
    saved_widths_ = widths();
    int previous = cols();
    cols(columns);
    for (int col = previous; col < columns; ++col)
        col_width(col, col < static_cast<int>(saved_widths_.size()) ? saved_widths_[col] : 200);
    rows(static_cast<int>(rows_.size()));
    row_height_all(27);
    redraw();
}
std::string ResultTable::columnLabel(int col) const
{
    if (col < 0)
        return {};
    if (sections_.size() == 1)
        return static_cast<std::size_t>(col) < sections_.front().columns.size()
                   ? sections_.front().columns[col]
                   : "";
    return sections_.empty() ? "" : std::to_string(col + 1);
}
void ResultTable::draw_cell(TableContext context, int row, int col, int x, int y, int w, int h)
{
    if (context == CONTEXT_COL_HEADER)
    {
        FLTK_Css_Node node;
        node.type = "cell";
        node.classes = {"header"};
        auto style = fltk_cell::cellStyle(css_, node);
        fltk_cell::CellValue value = columnLabel(col);
        text_.draw({{x, y, w, h}, value, style, {}});
        return;
    }
    if (context != CONTEXT_CELL || row < 0 || static_cast<std::size_t>(row) >= rows_.size())
        return;
    const auto &ref = rows_[row];
    const auto &section = sections_[ref.section];
    FLTK_Css_Node node;
    node.type = "cell";
    fltk_cell::CellValue value = std::string();
    const fltk_cell::TableCellDrawer *drawer = &text_;
    if (ref.kind == Row::Title || ref.kind == Row::Status)
    {
        if (col != leftcol)
            return;
        x = this->x() + 2;
        w = this->w() - 20;
        node.classes = {ref.kind == Row::Title ? "section" : "status"};
        value = ref.kind == Row::Title
                    ? std::to_string(ref.section + 1) + ". " + section.title
                    : section.status + " | " + std::to_string(section.count) + " rows | " +
                          std::to_string(section.elapsed_ms) + " ms (client, including fetch)" +
                          (section.clipped ? " [PREVIEW CLIPPED]" : "");
    }
    else if (ref.kind == Row::Columns)
    {
        node.classes = {"header"};
        if (static_cast<std::size_t>(col) < section.columns.size())
            value = section.columns[col];
    }
    else if (static_cast<std::size_t>(col) < section.rows[ref.row].size())
    {
        const auto &cell = section.rows[ref.row][col];
        if (cell.null)
        {
            value = std::monostate{};
            node.classes = {"null"};
        }
        else if (cell.type == 17 && !cell.clipped)
        {
            value = fltk_cell::BytesView{cell.binary.data(), cell.binary.size()};
            drawer = &hex_;
        }
        else if (cell.type == 16)
        {
            value = cell.text == "t";
            drawer = &check_;
        }
        else if (cell.type == 20 || cell.type == 21 || cell.type == 23 || cell.type == 26 ||
                 cell.type == 700 || cell.type == 701 || cell.type == 1700)
        {
            value = fltk_cell::Decimal{cell.text};
            drawer = &number_;
        }
        else
        {
            auto end = std::min<std::size_t>(cell.text.size(), 1024);
            while (end && end < cell.text.size() &&
                   (static_cast<unsigned char>(cell.text[end]) & 0xc0) == 0x80)
                --end;
            value = cell.text.substr(0, end) + (end < cell.text.size() || cell.clipped ? " ..." : "");
        }
    }
    auto style = fltk_cell::cellStyle(css_, node);
    if (ref.kind == Row::Title || ref.kind == Row::Columns)
        style.font = FL_HELVETICA_BOLD;
    number_.format.decimal_separator = language() == "en" ? '.' : ',';
    number_.format.group_separator = language() == "en" ? ',' : '.';
    drawer->draw({{x, y, w, h}, value, style, {is_selected(row, col) != 0, false, true}});
}
std::vector<int> ResultTable::widths()
{
    auto out = saved_widths_;
    out.resize(std::max(out.size(), static_cast<std::size_t>(cols())));
    for (int col = 0; col < cols(); ++col)
        out[col] = col_width(col);
    return out;
}
void ResultTable::widths(const std::vector<int> &values)
{
    saved_widths_ = values;
    for (std::size_t i = 0; i < values.size() && i < static_cast<std::size_t>(cols()); ++i)
        col_width(static_cast<int>(i), std::clamp(values[i], 40, 4096));
}
std::string ResultTable::copyText() const
{
    std::string out;
    auto field = [](std::string value)
    {
        if (value.find_first_of("\t\r\n\"") != std::string::npos)
            return csvField(value);
        return value;
    };
    for (const auto &section : sections_)
    {
        out += section.title + '\n';
        for (std::size_t col = 0; col < section.columns.size(); ++col)
        {
            if (col)
                out += '\t';
            out += field(section.columns[col]);
        }
        out += '\n';
        for (const auto &row : section.rows)
        {
            for (std::size_t col = 0; col < row.size(); ++col)
            {
                if (col)
                    out += '\t';
                out += field(cellText(row[col]));
            }
            out += '\n';
        }
        out += section.status + " | " + std::to_string(section.count) + " rows | " +
               std::to_string(section.elapsed_ms) + " ms (client, including fetch)" +
               (section.clipped ? " [PREVIEW CLIPPED]" : "") + "\n\n";
    }
    return out;
}
} // namespace pgjr
