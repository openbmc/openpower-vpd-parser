#pragma once

#include "const.hpp"
#include "parser_interface.hpp"
#include "store.hpp"
#include "types.hpp"

#include <vector>

namespace openpower
{
namespace vpd
{

using ParserInterface = openpower::vpd::ParserInterface;

class IpzVpdParser : public ParserInterface
{
  public:
    IpzVpdParser() = delete;
    IpzVpdParser(const IpzVpdParser&) = delete;
    IpzVpdParser& operator=(const IpzVpdParser&) = delete;
    IpzVpdParser(IpzVpdParser&&) = delete;
    IpzVpdParser& operator=(IpzVpdParser&&) = delete;
    ~IpzVpdParser() = default;

    /**
     * @brief Constructor
     */
    IpzVpdParser(const types::Binary& VpdVector, const std::string& path) :
        vpd(VpdVector), inventoryPath(path)
    {
    }

    /**
     * @brief Parse the memory VPD binary data.
     * Collects and emplace the keyword-value pairs in map.
     *
     * @return map of keyword:value
     */
    std::variant<types::KeywordVpdMap, Store> parse();

    /**
     * @brief An api to return interface name with respect to
     * the parser selected.
     *
     * @return - Interface name for that vpd type.
     */
    std::string getInterfaceName() const;

    /** @brief API to check vpd header
     *  @param [in] vpd - VPDheader in binary format
     */
    void processHeader();

  private:
    const types::Binary& vpd;

    /*Inventory path of the FRU */
    const std::string inventoryPath;
}; // class IpzVpdParser

} // namespace vpd
} // namespace openpower
