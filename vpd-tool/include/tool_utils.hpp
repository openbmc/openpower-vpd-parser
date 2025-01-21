#pragma once

#include "tool_constants.hpp"
#include "tool_types.hpp"

#include <nlohmann/json.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>

#include <fstream>
#include <iostream>

namespace vpd
{
namespace utils
{
/**
 * @brief An API to read property from Dbus.
 *
 * API reads the property value for the specified interface and object path from
 * the given Dbus service.
 *
 * The caller of the API needs to validate the validity and correctness of the
 * type and value of data returned. The API will just fetch and return the data
 * without any data validation.
 *
 * Note: It will be caller's responsibility to check for empty value returned
 * and generate appropriate error if required.
 *
 * @param[in] i_serviceName - Name of the Dbus service.
 * @param[in] i_objectPath - Object path under the service.
 * @param[in] i_interface - Interface under which property exist.
 * @param[in] i_property - Property whose value is to be read.
 *
 * @return - Value read from Dbus.
 *
 * @throw std::runtime_error
 */
inline types::DbusVariantType readDbusProperty(const std::string& i_serviceName,
                                               const std::string& i_objectPath,
                                               const std::string& i_interface,
                                               const std::string& i_property)
{
    types::DbusVariantType l_propertyValue;

    // Mandatory fields to make a dbus call.
    if (i_serviceName.empty() || i_objectPath.empty() || i_interface.empty() ||
        i_property.empty())
    {
        // TODO: Enable logging when verbose is enabled.
        /*std::cout << "One of the parameter to make Dbus read call is empty."
                  << std::endl;*/
        throw std::runtime_error("Empty Parameter");
    }

    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method =
            l_bus.new_method_call(i_serviceName.c_str(), i_objectPath.c_str(),
                                  "org.freedesktop.DBus.Properties", "Get");
        l_method.append(i_interface, i_property);

        auto result = l_bus.call(l_method);
        result.read(l_propertyValue);
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        // TODO: Enable logging when verbose is enabled.
        // std::cout << std::string(l_ex.what()) << std::endl;
        throw std::runtime_error(std::string(l_ex.what()));
    }
    return l_propertyValue;
}

/**
 * @brief An API to print json data on stdout.
 *
 * @param[in] i_jsonData - JSON object.
 */
inline void printJson(const nlohmann::json& i_jsonData)
{
    try
    {
        std::cout << i_jsonData.dump(constants::INDENTATION) << std::endl;
    }
    catch (const nlohmann::json::type_error& l_ex)
    {
        throw std::runtime_error("Failed to dump JSON data, error: " +
                                 std::string(l_ex.what()));
    }
}

/**
 * @brief An API to convert binary value into ascii/hex representation.
 *
 * If given data contains printable characters, ASCII formated string value of
 * the input data will be returned. Otherwise if the data has any non-printable
 * value, returns the hex represented value of the given data in string format.
 *
 * @param[in] i_keywordValue - Data in binary format.
 *
 * @throw - Throws std::bad_alloc or std::terminate in case of error.
 *
 * @return - Returns the converted string value.
 */
inline std::string getPrintableValue(const types::BinaryVector& i_keywordValue)
{
    bool l_allPrintable =
        std::all_of(i_keywordValue.begin(), i_keywordValue.end(),
                    [](const auto& l_byte) { return std::isprint(l_byte); });

    std::ostringstream l_oss;
    if (l_allPrintable)
    {
        l_oss << std::string(i_keywordValue.begin(), i_keywordValue.end());
    }
    else
    {
        l_oss << "0x";
        for (const auto& l_byte : i_keywordValue)
        {
            l_oss << std::setfill('0') << std::setw(2) << std::hex
                  << static_cast<int>(l_byte);
        }
    }

    return l_oss.str();
}

/**
 * @brief API to read keyword's value from hardware.
 *
 * This API reads keyword's value by requesting DBus service(vpd-manager) who
 * hosts the 'ReadKeyword' method to read keyword's value.
 *
 * @param[in] i_eepromPath - EEPROM file path.
 * @param[in] i_paramsToReadData - Property whose value has to be read.
 *
 * @return - Value read from hardware
 *
 * @throw std::runtime_error, sdbusplus::exception::SdBusError
 */
inline types::DbusVariantType
    readKeywordFromHardware(const std::string& i_eepromPath,
                            const types::ReadVpdParams i_paramsToReadData)
{
    if (i_eepromPath.empty())
    {
        throw std::runtime_error("Empty EEPROM path");
    }

    try
    {
        types::DbusVariantType l_propertyValue;

        auto l_bus = sdbusplus::bus::new_default();

        auto l_method = l_bus.new_method_call(
            constants::vpdManagerService, constants::vpdManagerObjectPath,
            constants::vpdManagerInfName, "ReadKeyword");

        l_method.append(i_eepromPath, i_paramsToReadData);
        auto l_result = l_bus.call(l_method);

        l_result.read(l_propertyValue);

        return l_propertyValue;
    }
    catch (const sdbusplus::exception::SdBusError& l_error)
    {
        throw;
    }
}

/**
 * @brief API to save keyword's value on file.
 *
 * API writes keyword's value on the given file path. If the data is in hex
 * format, API strips '0x' and saves the value on the given file.
 *
 * @param[in] i_filePath - File path.
 * @param[in] i_keywordValue - Keyword's value.
 *
 * @return - true on successfully writing to file, false otherwise.
 */
inline bool saveToFile(const std::string& i_filePath,
                       const std::string& i_keywordValue)
{
    bool l_returnStatus = false;

    if (i_keywordValue.empty())
    {
        // ToDo: log only when verbose is enabled
        std::cerr << "Save to file[ " << i_filePath
                  << "] failed, reason: Empty keyword's value received"
                  << std::endl;
        return l_returnStatus;
    }

    std::string l_keywordValue{i_keywordValue};
    if (i_keywordValue.substr(0, 2).compare("0x") == constants::STR_CMP_SUCCESS)
    {
        l_keywordValue = i_keywordValue.substr(2);
    }

    std::ofstream l_outPutFileStream;
    l_outPutFileStream.exceptions(std::ifstream::badbit |
                                  std::ifstream::failbit);
    try
    {
        l_outPutFileStream.open(i_filePath);

        if (l_outPutFileStream.is_open())
        {
            l_outPutFileStream.write(l_keywordValue.c_str(),
                                     l_keywordValue.size());
            l_returnStatus = true;
        }
        else
        {
            // ToDo: log only when verbose is enabled
            std::cerr << "Error opening output file " << i_filePath
                      << std::endl;
        }
    }
    catch (const std::ios_base::failure& l_ex)
    {
        // ToDo: log only when verbose is enabled
        std::cerr
            << "Failed to write to file: " << i_filePath
            << ", either base folder path doesn't exist or internal error occured, error: "
            << l_ex.what() << '\n';
    }

    return l_returnStatus;
}

/**
 * @brief API to print data in JSON format on console
 *
 * @param[in] i_fruPath - FRU path.
 * @param[in] i_keywordName - Keyword name.
 * @param[in] i_keywordStrValue - Keyword's value.
 */
inline void displayOnConsole(const std::string& i_fruPath,
                             const std::string& i_keywordName,
                             const std::string& i_keywordStrValue)
{
    nlohmann::json l_resultInJson = nlohmann::json::object({});
    nlohmann::json l_keywordValInJson = nlohmann::json::object({});

    l_keywordValInJson.emplace(i_keywordName, i_keywordStrValue);
    l_resultInJson.emplace(i_fruPath, l_keywordValInJson);

    printJson(l_resultInJson);
}

/**
 * @brief API to write keyword's value.
 *
 * This API writes keyword's value by requesting DBus service(vpd-manager) who
 * hosts the 'UpdateKeyword' method to update keyword's value.
 *
 * @param[in] i_vpdPath - EEPROM or object path, where keyword is present.
 * @param[in] i_paramsToWriteData - Data required to update keyword's value.
 *
 * @return - Number of bytes written on success, -1 on failure.
 *
 * @throw - std::runtime_error, sdbusplus::exception::SdBusError
 */
inline int writeKeyword(const std::string& i_vpdPath,
                        const types::WriteVpdParams& i_paramsToWriteData)
{
    if (i_vpdPath.empty())
    {
        throw std::runtime_error("Empty path");
    }

    int l_rc = constants::FAILURE;
    auto l_bus = sdbusplus::bus::new_default();

    auto l_method = l_bus.new_method_call(
        constants::vpdManagerService, constants::vpdManagerObjectPath,
        constants::vpdManagerInfName, "UpdateKeyword");

    l_method.append(i_vpdPath, i_paramsToWriteData);
    auto l_result = l_bus.call(l_method);

    l_result.read(l_rc);
    return l_rc;
}

/**
 * @brief API to write keyword's value on hardware.
 *
 * This API writes keyword's value by requesting DBus service(vpd-manager) who
 * hosts the 'WriteKeywordOnHardware' method to update keyword's value.
 *
 * Note: This API updates keyword's value only on the given hardware path, any
 * backup or redundant EEPROM (if exists) paths won't get updated.
 *
 * @param[in] i_eepromPath - EEPROM where keyword is present.
 * @param[in] i_paramsToWriteData - Data required to update keyword's value.
 *
 * @return - Number of bytes written on success, -1 on failure.
 *
 * @throw - std::runtime_error, sdbusplus::exception::SdBusError
 */
inline int
    writeKeywordOnHardware(const std::string& i_eepromPath,
                           const types::WriteVpdParams& i_paramsToWriteData)
{
    if (i_eepromPath.empty())
    {
        throw std::runtime_error("Empty path");
    }

    int l_rc = constants::FAILURE;
    auto l_bus = sdbusplus::bus::new_default();

    auto l_method = l_bus.new_method_call(
        constants::vpdManagerService, constants::vpdManagerObjectPath,
        constants::vpdManagerInfName, "WriteKeywordOnHardware");

    l_method.append(i_eepromPath, i_paramsToWriteData);
    auto l_result = l_bus.call(l_method);

    l_result.read(l_rc);

    if (l_rc > 0)
    {
        std::cout << "Data updated successfully " << std::endl;
    }
    return l_rc;
}

/**
 * @brief API to get data in binary format.
 *
 * This API converts given string value into array of binary data.
 *
 * @param[in] i_value - Input data.
 *
 * @return - Array of binary data on success, throws as exception in case
 * of any error.
 *
 * @throw std::runtime_error, std::out_of_range, std::bad_alloc,
 * std::invalid_argument
 */
inline types::BinaryVector convertToBinary(const std::string& i_value)
{
    if (i_value.empty())
    {
        throw std::runtime_error(
            "Provide a valid hexadecimal input. (Ex. 0x30313233)");
    }

    std::vector<uint8_t> l_binaryValue{};

    if (i_value.substr(0, 2).compare("0x") == constants::STR_CMP_SUCCESS)
    {
        if (i_value.length() % 2 != 0)
        {
            throw std::runtime_error(
                "Write option accepts 2 digit hex numbers. (Ex. 0x1 "
                "should be given as 0x01).");
        }

        auto l_value = i_value.substr(2);

        if (l_value.empty())
        {
            throw std::runtime_error(
                "Provide a valid hexadecimal input. (Ex. 0x30313233)");
        }

        if (l_value.find_first_not_of("0123456789abcdefABCDEF") !=
            std::string::npos)
        {
            throw std::runtime_error("Provide a valid hexadecimal input.");
        }

        for (size_t l_pos = 0; l_pos < l_value.length(); l_pos += 2)
        {
            uint8_t l_byte = static_cast<uint8_t>(
                std::stoi(l_value.substr(l_pos, 2), nullptr, 16));
            l_binaryValue.push_back(l_byte);
        }
    }
    else
    {
        l_binaryValue.assign(i_value.begin(), i_value.end());
    }
    return l_binaryValue;
}

/**
 * @brief API to parse respective JSON.
 *
 * @param[in] i_pathToJson - Path to JSON.
 *
 * @return Parsed JSON, throws exception in case of error.
 *
 * @throw std::runtime_error
 */
inline nlohmann::json getParsedJson(const std::string& i_pathToJson)
{
    if (i_pathToJson.empty())
    {
        throw std::runtime_error("Path to JSON is missing");
    }

    std::error_code l_ec;
    if (!std::filesystem::exists(i_pathToJson, l_ec))
    {
        std::string l_message{"file system call failed for file: " +
                              i_pathToJson};

        if (l_ec)
        {
            l_message += ", error: " + l_ec.message();
        }
        throw std::runtime_error(l_message);
    }

    if (std::filesystem::is_empty(i_pathToJson, l_ec))
    {
        throw std::runtime_error("Empty file: " + i_pathToJson);
    }
    else if (l_ec)
    {
        throw std::runtime_error("is_empty file system call failed for file: " +
                                 i_pathToJson + ", error: " + l_ec.message());
    }

    std::ifstream l_jsonFile(i_pathToJson);
    if (!l_jsonFile)
    {
        throw std::runtime_error("Failed to access Json path: " + i_pathToJson);
    }

    try
    {
        return nlohmann::json::parse(l_jsonFile);
    }
    catch (const nlohmann::json::parse_error& l_ex)
    {
        throw std::runtime_error("Failed to parse JSON file: " + i_pathToJson);
    }
}

/**
 * @brief API to get list of interfaces under a given object path.
 *
 * Given a DBus object path, this API returns a map of service -> implemented
 * interface(s) under that object path. This API calls DBus method GetObject
 * hosted by ObjectMapper DBus service.
 *
 * @param[in] i_objectPath - DBus object path.
 * @param[in] i_constrainingInterfaces - An array of result set constraining
 * interfaces.
 *
 * @return On success, returns a map of service -> implemented interface(s),
 * else returns an empty map. The caller of this
 * API should check for empty map.
 */
inline types::MapperGetObject GetServiceInterfacesForObject(
    const std::string& i_objectPath,
    const std::vector<std::string>& i_constrainingInterfaces) noexcept
{
    types::MapperGetObject l_serviceInfMap;
    if (i_objectPath.empty())
    {
        // TODO: log only when verbose is enabled
        std::cerr << "Object path is empty." << std::endl;
        return l_serviceInfMap;
    }

    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method = l_bus.new_method_call(
            constants::objectMapperService, constants::objectMapperObjectPath,
            constants::objectMapperInfName, "GetObject");

        l_method.append(i_objectPath, i_constrainingInterfaces);

        auto l_result = l_bus.call(l_method);
        l_result.read(l_serviceInfMap);
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        // TODO: log only when verbose is enabled
        // std::cerr << std::string(l_ex.what()) << std::endl;
    }
    return l_serviceInfMap;
}

/** @brief API to get list of sub tree paths for a given object path
 *
 * Given a DBus object path, this API returns a list of object paths under that
 * object path in the DBus tree. This API calls DBus method GetSubTreePaths
 * hosted by ObjectMapper DBus service.
 *
 * @param[in] i_objectPath - DBus object path.
 * @param[in] i_constrainingInterfaces - An array of result set constraining
 * interfaces.
 * @param[in] i_depth - The maximum subtree depth for which results should be
 * fetched. For unconstrained fetches use a depth of zero.
 *
 * @return On success, returns a std::vector<std::string> of object paths in
 * Phosphor Inventory Manager DBus service's tree, else returns an empty vector.
 * The caller of this API should check for empty vector.
 */
inline std::vector<std::string> GetSubTreePaths(
    const std::string i_objectPath, const int i_depth = 0,
    const std::vector<std::string>& i_constrainingInterfaces = {}) noexcept
{
    std::vector<std::string> l_objectPaths;

    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method = l_bus.new_method_call(
            constants::objectMapperService, constants::objectMapperObjectPath,
            constants::objectMapperInfName, "GetSubTreePaths");

        l_method.append(i_objectPath, i_depth, i_constrainingInterfaces);

        auto l_result = l_bus.call(l_method);
        l_result.read(l_objectPaths);
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        // TODO: log only when verbose is enabled
        std::cerr << std::string(l_ex.what()) << std::endl;
    }
    return l_objectPaths;
}

/**
 * @brief A class to print data in tabular format
 *
 * This class implements methods to print data in a two dimensional tabular
 * format. All entries in the table must be in string format.
 *
 */
class Table
{
    class Column : public types::TableColumnNameSizePair
    {
      public:
        /**
         * @brief API to get the name of the Column
         *
         * @return Name of the Column.
         */
        const std::string& Name() const
        {
            return this->first;
        }

        /**
         * @brief API to get the width of the Column
         *
         * @return Width of the Column.
         */
        std::size_t Width() const
        {
            return this->second;
        }
    };

    // Current width of the table
    std::size_t m_currentWidth;

    // Character to be used as fill character between entries
    char m_fillCharacter;

    // Separator character to be used between columns
    char m_separator;

    // Array of columns
    std::vector<Column> m_columns;

    /**
     * @brief API to Print Header
     *
     * Header line prints the names of the Column headers separated by the
     * specified separator character and spaced accordingly.
     *
     * @throw std::out_of_range, std::length_error, std::bad_alloc
     */
    void PrintHeader() const
    {
        for (const auto& l_column : m_columns)
        {
            PrintEntry(l_column.Name(), l_column.Width());
        }
        std::cout << m_separator << std::endl;
    }

    /**
     * @brief API to Print Horizontal Line
     *
     * A horizontal line is a sequence of '*'s.
     *
     * @throw std::out_of_range, std::length_error, std::bad_alloc
     */
    void PrintHorizontalLine() const
    {
        std::cout << std::string(m_currentWidth, '*') << std::endl;
    }

    /**
     * @brief API to print an entry in the table
     *
     * An entry is a separator character followed by the text to print.
     * The text is centre-aligned.
     *
     * @param[in] i_text - text to print
     * @param[in] i_columnWidth - width of the column
     *
     * @throw std::out_of_range, std::length_error, std::bad_alloc
     */
    void PrintEntry(const std::string& i_text, std::size_t i_columnWidth) const
    {
        const std::size_t l_textLength{i_text.length()};

        constexpr std::size_t l_minFillChars{3};
        const std::size_t l_numFillChars =
            ((l_textLength >= i_columnWidth ? l_minFillChars
                                            : i_columnWidth - l_textLength)) -
            1; // -1 for the separator character

        const unsigned l_oddFill = l_numFillChars % 2;

        std::cout << m_separator
                  << std::string((l_numFillChars / 2) + l_oddFill,
                                 m_fillCharacter)
                  << i_text << std::string(l_numFillChars / 2, m_fillCharacter);
    }

  public:
    /**
     * @brief Table Constructor
     *
     * Parameterized constructor for a Table object
     *
     */
    constexpr explicit Table(const char i_fillCharacter = ' ',
                             const char i_separator = '|') noexcept :
        m_currentWidth{0}, m_fillCharacter{i_fillCharacter},
        m_separator{i_separator}
    {}

    // deleted methods
    Table(const Table&) = delete;
    Table operator=(const Table&) = delete;
    Table(const Table&&) = delete;
    Table operator=(const Table&&) = delete;

    ~Table() = default;

    /**
     * @brief API to add column to Table
     *
     * @param[in] i_name - Name of the column.
     *
     * @param[in] i_width - Width to allocate for the column.
     *
     * @return On success returns 0, otherwise returns -1.
     */
    int AddColumn(const std::string& i_name, std::size_t i_width)
    {
        if (i_width < i_name.length())
            return constants::FAILURE;
        m_columns.emplace_back(types::TableColumnNameSizePair(i_name, i_width));
        m_currentWidth += i_width;
        return constants::SUCCESS;
    }

    /**
     * @brief API to print the Table to console.
     *
     * This API prints the table data to console.
     *
     * @param[in] i_tableData - The data to be printed.
     *
     * @return On success returns 0, otherwise returns -1.
     *
     * @throw std::out_of_range, std::length_error, std::bad_alloc
     */
    int Print(const types::TableInputData& i_tableData) const
    {
        PrintHorizontalLine();
        PrintHeader();
        PrintHorizontalLine();

        // print the table data
        for (const auto& l_row : i_tableData)
        {
            unsigned l_columnNumber{0};

            // number of columns in input data is greater than the number of
            // columns specified in Table
            if (l_row.size() > m_columns.size())
            {
                return constants::FAILURE;
            }

            for (const auto& l_entry : l_row)
            {
                PrintEntry(l_entry, m_columns[l_columnNumber].Width());

                ++l_columnNumber;
            }
            std::cout << m_separator << std::endl;
        }
        PrintHorizontalLine();
        return constants::SUCCESS;
    }
};

} // namespace utils
} // namespace vpd
