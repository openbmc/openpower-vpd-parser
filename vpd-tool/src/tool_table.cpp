#include "tool_table.hpp"

#include "tool_constants.hpp"

#include <algorithm>
#include <iostream>

namespace vpd
{
void Table::printHeader() const
{
    for (const auto& column : columns)
    {
        printEntry(column.name(), column.width());
    }
    std::cout << separator << std::endl;
}

void Table::printHorizontalLine() const
{
    std::cout << std::string(currentWidth, '-') << std::endl;
}

std::vector<std::string> Table::wrapText(const std::string& text,
                                         std::size_t columnWidth) const
{
    // Reserve space for the leading and trailing column separators.
    const std::size_t maxChars = columnWidth > constants::VALUE_2
                                     ? columnWidth - constants::VALUE_2
                                     : constants::VALUE_1;

    if (text.empty() || text.size() <= maxChars)
    {
        return {text};
    }

    std::vector<std::string> lines;
    std::size_t pos{constants::VALUE_0};

    while (pos < text.size())
    {
        const std::size_t remaining = text.size() - pos;
        std::string_view chunk{text.data() + pos,
                               std::min(maxChars, remaining)};

        // Prefer breaking at a word boundary when the text exceeds
        // the available column width.
        if (chunk.size() == maxChars && pos + chunk.size() < text.size())
        {
            if (const auto lastSpace = chunk.rfind(' ');
                lastSpace != std::string_view::npos &&
                lastSpace > constants::VALUE_0)
            {
                chunk = chunk.substr(constants::VALUE_0, lastSpace);
            }
            // For paths or other slash-separated values, break after '/'.
            else if (const auto lastSlash = chunk.rfind('/');
                     lastSlash != std::string_view::npos)
            {
                chunk = chunk.substr(constants::VALUE_0,
                                     lastSlash + constants::VALUE_1);
            }
        }

        lines.emplace_back(chunk);
        pos += chunk.size();

        if (pos < text.length() && text[pos] == ' ')
        {
            ++pos;
        }
    }
    return lines;
}

void Table::printEntry(const std::string& text, std::size_t columnWidth) const
{
    const std::size_t textLength{text.length()};

    // total fill = column width - text length, minimum 2 (1 lead + 1 trail)
    // subtract 1 for the separator character printed before the cell
    constexpr std::size_t minFillChars{2};
    const std::size_t totalFill =
        (textLength >= columnWidth ? minFillChars : columnWidth - textLength) -
        constants::VALUE_1; // -1 for the separator character

    if (leftAlign)
    {
        // 1 leading space already printed; remaining fill goes on the right
        // guard against underflow: if totalFill is 0, right pad is 0
        const std::size_t rightPad = totalFill > constants::VALUE_1
                                         ? totalFill - constants::VALUE_1
                                         : constants::VALUE_0;
        std::cout << separator << fillCharacter << text
                  << std::string(rightPad, fillCharacter);
    }
    else
    {
        const std::size_t half = totalFill / 2;
        std::cout << separator << std::string(totalFill - half, fillCharacter)
                  << text << std::string(half, fillCharacter);
    }
}

int Table::addColumn(const std::string& name, std::size_t width)
{
    if (width < name.length())
    {
        return constants::FAILURE;
    }
    columns.emplace_back(types::TableColumnNameSizePair(name, width));
    currentWidth += width;
    return constants::SUCCESS;
}

int Table::print(const types::TableInputData& tableData,
                 const bool rowSeparator) const
{
    printHorizontalLine();
    printHeader();
    printHorizontalLine();

    for (const auto& row : tableData)
    {
        if (row.size() > columns.size())
        {
            return constants::FAILURE;
        }

        // wrap each cell's text into lines that fit the column width
        std::vector<std::vector<std::string>> wrappedCells;
        wrappedCells.reserve(columns.size());

        std::size_t maxLines{1};
        for (std::size_t col = 0; col < columns.size(); ++col)
        {
            const std::string& text = col < row.size() ? row[col] : "";
            auto lines = wrapText(text, columns[col].width());
            maxLines = std::max(maxLines, lines.size());
            wrappedCells.push_back(std::move(lines));
        }

        // print one screen-line per wrapped line, padding shorter cells
        for (std::size_t line = 0; line < maxLines; ++line)
        {
            for (std::size_t col = 0; col < columns.size(); ++col)
            {
                const std::string& text = line < wrappedCells[col].size()
                                              ? wrappedCells[col][line]
                                              : "";
                printEntry(text, columns[col].width());
            }
            std::cout << separator << std::endl;
        }

        if (rowSeparator)
        {
            printHorizontalLine();
        }
    }

    // when rowSeparator is true every row already received its own
    // horizontal line above, so the table is already closed; when false,
    // print the single closing border now.
    if (!rowSeparator)
    {
        printHorizontalLine();
    }

    return constants::SUCCESS;
}
} // namespace vpd
