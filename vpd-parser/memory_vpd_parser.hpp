#pragma once

#include "impl.hpp"
#include "parser_interface.hpp"
#include "types.hpp"

namespace openpower
{
namespace vpd
{
namespace memory
{
namespace parser
{
using ParserInterface = openpower::vpd::parser::interface::ParserInterface;
using kwdVpdMap = openpower::vpd::inventory::KeywordVpdMap;

class memoryVpdParser : public ParserInterface
{
  public:
    memoryVpdParser() = delete;
    memoryVpdParser(const memoryVpdParser&) = delete;
    memoryVpdParser& operator=(const memoryVpdParser&) = delete;
    memoryVpdParser(memoryVpdParser&&) = delete;
    memoryVpdParser& operator=(memoryVpdParser&&) = delete;
    ~memoryVpdParser() = default;

    /**
     * @brief Constructor
     *
     * Move memVpdVector to parser object's memVpdVector
     */
    explicit memoryVpdParser(const Binary& VpdVector) : memVpd(VpdVector)
    {
    }

    /**
     * @brief Parse the memory VPD binary data.
     * Collects and emplace the keyword-value pairs in map.
     *
     * @return map of keyword:value
     */
    std::variant<kwdVpdMap, Store> parse() override;

    /**
     * @brief An api to return interface name with respect to
     * publish data on cache
     *
     * @return - Interface name for that vpd type.
     */
    std::string getInterfaceName() const override;

  private:
    /**
     * @brief An api to read keywords.
     *
     * @return- map of kwd:value
     */
    kwdVpdMap readKeywords(Binary::const_iterator iterator);

    /**
     * @brief This function calculates dimm size from DIMM VPD
     *
     * @param[in] iterator - iterator to buffer containing VPD
     * @return calculated data or 0 in case of any error.
     */
    auto getDimmSize(Binary::const_iterator iterator);

    // vdp file to be parsed
    const Binary& memVpd;
};
} // namespace parser
} // namespace memory
} // namespace vpd
	} // namespace openpower
