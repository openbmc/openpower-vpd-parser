#pragma once

#include "store.hpp"
#include "types.hpp"

#include <variant>

namespace openpower
{
namespace vpd
{
namespace parser
{
namespace interface
{
using kwdVpdMap = openpower::vpd::inventory::KeywordVpdMap;

/** @class ParserInterface
 *  @brief Interface class for vpd parsers.
 *
 *  Any concrete parser class, implementing the parser logic needs to
 *  derive from this interface class and override the methods declared
 *  in this class.
 */
class ParserInterface
{
  public:
    /**
     * @brief An api to implement parsing logic for VPD file.
     * Needs to be implemented by all the class deriving from
     * parser interface.
     *
     * @return parsed format for vpd data, depending upon the
     * parsing logic.
     */
    virtual std::variant<kwdVpdMap, Store> parse() = 0;

    /**
     * @brief An api to return interface name which will hold the
     * data on cache.
     * Needs to be implemented by all the class deriving fronm
     * parser interface
     *
     * @return - Interface name for that vpd type.
     */
    virtual std::string getInterfaceName() const = 0;
    // virtual void test() = 0;

    /**
     * @brief Virtual destructor for the interface.
     */
    virtual ~ParserInterface() {}

}; // class Parserinterface
} // namespace interface
} // namespace parser
} // namespace vpd
} // namespace openpower
