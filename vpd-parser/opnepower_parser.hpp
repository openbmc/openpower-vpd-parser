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
namespace parser
{

using ParserInterface = openpower::vpd::parser::interface::ParserInterface;
using kwdVpdMap = openpower::vpd::inventory::KeywordVpdMap;

class OpenpowerVpdParser : public ParserInterface
{
  public:
    OpenpowerVpdParser() = delete;
    OpenpowerVpdParser(const OpenpowerVpdParser&) = delete;
    OpenpowerVpdParser& operator=(const OpenpowerVpdParser&) = delete;
    OpenpowerVpdParser(OpenpowerVpdParser&&) = delete;
    OpenpowerVpdParser& operator=(OpenpowerVpdParser&&) = delete;
    ~OpenpowerVpdParser() = default;

    /**
     * @brief Constructor
     */
    OpenpowerVpdParser(Binary&& VpdVector) : vpd(std::move(VpdVector))
    {
    }

    /**
     * @brief Parse the memory VPD binary data.
     * Collects and emplace the keyword-value pairs in map.
     *
     * @return map of keyword:value
     */
    std::variant<kwdVpdMap, Store> parse();

    /**
     * @brief An api to return interface name with respect to
     * the parser selected.
     *
     * @return - Interface name for that vpd type.
     */
    std::string getInterfaceName() const;

    /** @brief API to write parsed VPD to inventory
     *
     *  @param [in] type - FRU type
     *  @param [in] vpdStore - Store object containing parsed VPD
     *  @param [in] path - FRU object path
     */
    void write(const std::string& type, const Store& vpdStore,
               const std::string& path);

  private:
    Binary vpd;
}; // class OpenpowerVpdParser

} // namespace parser
} // namespace vpd
} // namespace openpower
