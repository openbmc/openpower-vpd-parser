#pragma once

#include "impl.hpp"
#include "types.hpp"

namespace openpower
{
namespace vpd
{
namespace memory
{
namespace parser
{

class memoryVpdParser
{
  public:
    /**
     * @brief Move Constructor
     *
     * Move kwVpdVector to parser object's kwVpdVector
     */
    memoryVpdParser(Binary&& memVpdVector) : memVpd(std::move(memVpdVector))
    {
    }

    /**
     * @brief Destructor
     *
     * Delete the parser object
     */
    ~memoryVpdParser(){};

    /**
     * @brief Parse the memory VPD binary data.
     *
     * Collects and emplace the keyword-value pairs in map.
     *
     * @return map of keyword:value
     */
    inventory::KeywordVpdMap parseMemVpd();

  private:
    /**
     */
    inventory::KeywordVpdMap readKeywords(Binary::const_iterator iterator);

    /**
     */
    Binary memVpd;
};
} // namespace parser
} // namespace memory
} // namespace vpd
} // namespace openpower