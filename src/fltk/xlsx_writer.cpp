#include "xlsx_writer.h"
#include <xlsxwriter.h>
#include <algorithm>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace pgjr
{
namespace
{
void check(lxw_error error)
{
    if (error)
        throw std::runtime_error(lxw_strerror(error));
}
} // namespace
struct XlsxWriter::Impl
{
    lxw_workbook *book = nullptr;
    lxw_worksheet *sheet = nullptr;
    lxw_format *heading = nullptr;
    std::uint32_t next = 0;
};
XlsxWriter::XlsxWriter(const std::filesystem::path &file) : impl_(std::make_unique<Impl>())
{
    lxw_workbook_options options{};
    options.constant_memory = LXW_TRUE;
    impl_->book = workbook_new_opt(file.u8string().c_str(), &options);
    if (!impl_->book)
        throw std::runtime_error("Cannot create Excel workbook");
    impl_->sheet = workbook_add_worksheet(impl_->book, "Results");
    impl_->heading = workbook_add_format(impl_->book);
    if (!impl_->sheet || !impl_->heading)
    {
        workbook_close(impl_->book);
        impl_->book = nullptr;
        throw std::runtime_error("Cannot create Excel worksheet");
    }
    format_set_bold(impl_->heading);
    format_set_bg_color(impl_->heading, 0xDEEAF5);
}
XlsxWriter::~XlsxWriter()
{
    if (impl_->book)
        workbook_close(impl_->book);
}
void XlsxWriter::widths(const std::vector<int> &values)
{
    for (std::size_t i = 0; i < values.size() && i < 16384; ++i)
        check(worksheet_set_column(impl_->sheet, static_cast<lxw_col_t>(i), static_cast<lxw_col_t>(i),
                                   std::clamp((values[i] - 5) / 7.0, 5.0, 255.0), nullptr));
}
void XlsxWriter::row(const std::vector<std::string> &values, bool heading)
{
    if (!impl_->book)
        throw std::runtime_error("Workbook already closed");
    if (impl_->next >= 1048576 || values.size() > 16384)
        throw std::runtime_error("Excel row/column limit exceeded; use CSV");
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        std::size_t characters = 0;
        for (unsigned char ch : values[i])
            if ((ch & 0xc0) != 0x80)
                ++characters;
        if (characters > 32767 || values[i].find('\0') != std::string::npos)
            throw std::runtime_error("Excel cell limit exceeded; use CSV to preserve the full value");
        // Explicit strings preserve exact numbers and prevent formulas in exported server text.
        check(worksheet_write_string(impl_->sheet, impl_->next, static_cast<lxw_col_t>(i), values[i].c_str(),
                                     heading ? impl_->heading : nullptr));
    }
    ++impl_->next;
}
void XlsxWriter::close()
{
    if (impl_->book)
    {
        auto *book = impl_->book;
        impl_->book = nullptr;
        check(workbook_close(book));
    }
}
void publishFile(const std::filesystem::path &temporary, const std::filesystem::path &destination)
{
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot publish output file");
#else
    std::filesystem::rename(temporary, destination);
#endif
}
void exportXlsx(const std::filesystem::path &file, const std::vector<Section> &sections,
                const std::vector<int> &widths, const std::atomic_bool *cancelled)
{
    auto temporary = file;
    temporary += "." + newId() + ".tmp";
    try
    {
        {
            XlsxWriter writer(temporary);
            writer.widths(widths);
            for (const auto &section : sections)
            {
                writer.row({section.title}, true);
                writer.row(section.columns, true);
                for (const auto &cells : section.rows)
                {
                    if (cancelled && cancelled->load())
                        throw std::runtime_error("Export cancelled; previous file was retained");
                    std::vector<std::string> values;
                    for (const auto &cell : cells)
                        values.push_back(cellText(cell));
                    writer.row(values);
                }
                writer.row({section.status + (section.clipped ? " [PREVIEW CLIPPED]" : "")});
            }
            writer.close();
        }
        if (cancelled && cancelled->load())
            throw std::runtime_error("Export cancelled; previous file was retained");
        publishFile(temporary, file);
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
} // namespace pgjr
