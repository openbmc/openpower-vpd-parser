#pragma once

#include "tool_types.hpp"

#include <string>
#include <vector>

namespace vpd
{
/**
 * @brief A class to print data in tabular format
 *
 * This class implements methods to print data in a two dimensional tabular
 * format. All entries in the table must be in string format.
 *
 */
class Table
{
  public:
    // deleted methods
    Table(const Table&) = delete;
    Table operator=(const Table&) = delete;
    Table(const Table&&) = delete;
    Table operator=(const Table&&) = delete;

    ~Table() = default;

    /**
     * @brief Table Constructor
     *
     * Parameterized constructor for a Table object
     *
     * @param[in] fillCharacter - Character used to pad columns (default: ' ').
     * @param[in] separator     - Character used to separate columns
     *                            (default: '|').
     * @param[in] leftAlign     - If true, text is left-aligned; otherwise
     *                            centre-aligned (default: false).
     */
    constexpr explicit Table(const char fillCharacter = ' ',
                             const char separator = '|',
                             const bool leftAlign = false) noexcept :
        currentWidth{0}, fillCharacter{fillCharacter}, separator{separator},
        leftAlign{leftAlign}
    {}

    /**
     * @brief API to add column to Table
     *
     * @param[in] name  - Name of the column.
     *
     * @param[in] width - Width to allocate for the column.
     *
     * @return On success returns 0, otherwise returns -1.
     */
    int addColumn(const std::string& name, std::size_t width);

    /**
     * @brief API to print the Table to console.
     *
     * This API prints the table data to console. Text that exceeds a column's
     * width is automatically wrapped onto continuation lines, keeping all
     * columns aligned.
     *
     * @param[in] tableData    - The data to be printed.
     * @param[in] rowSeparator - If true, a dashed separator line is printed
     *                           after each row (default: false).
     *
     * @return On success returns 0, otherwise returns -1.
     *
     * @throw std::out_of_range, std::length_error, std::bad_alloc
     */
    int print(const types::TableInputData& tableData,
              const bool rowSeparator = false) const;

  private:
    class Column : public types::TableColumnNameSizePair
    {
      public:
        /**
         * @brief API to get the name of the Column
         *
         * @return Name of the Column.
         */
        const std::string& name() const
        {
            return this->first;
        }

        /**
         * @brief API to get the width of the Column
         *
         * @return Width of the Column.
         */
        std::size_t width() const
        {
            return this->second;
        }
    };

    // Current width of the table
    std::size_t currentWidth;

    // Character to be used as fill character between entries
    char fillCharacter;

    // Separator character to be used between columns
    char separator;

    // Flag to control left alignment (true) or centre alignment (false)
    bool leftAlign;

    // Array of columns
    std::vector<Column> columns;

    /**
     * @brief API to Print Header
     *
     * Header line prints the names of the Column headers separated by the
     * specified separator character and spaced accordingly.
     *
     * @throw std::out_of_range, std::length_error, std::bad_alloc
     */
    void printHeader() const;

    /**
     * @brief API to Print Horizontal Line
     *
     * A horizontal line is a sequence of '-'s.
     *
     * @throw std::out_of_range, std::length_error, std::bad_alloc
     */
    void printHorizontalLine() const;

    /**
     * @brief API to wrap text into lines that fit within a column width.
     *
     * Splits text into lines of at most (columnWidth - 2) characters,
     * breaking on word boundaries where possible.
     *
     * @param[in] text        - Text to wrap.
     * @param[in] columnWidth - Available column width (including padding).
     *
     * @return Vector of wrapped lines.
     *
     * @throw std::out_of_range, std::length_error, std::bad_alloc
     */
    std::vector<std::string> wrapText(const std::string& text,
                                      std::size_t columnWidth) const;

    /**
     * @brief API to print an entry in the table
     *
     * An entry is a separator character followed by the text to print.
     * Alignment (centre or left) is controlled by leftAlign.
     *
     * @param[in] text        - text to print
     * @param[in] columnWidth - width of the column
     *
     * @throw std::out_of_range, std::length_error, std::bad_alloc
     */
    void printEntry(const std::string& text, std::size_t columnWidth) const;
};
} // namespace vpd
