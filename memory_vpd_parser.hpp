#pragma once

#include "impl.hpp"
#include "keyword_vpd_types.hpp"

namespace vpd
{
namespace memory
{
namespace parser
{
using namespace openpower::vpd;
using namespace openpower::vpd::parser;
using namespace vpd::keyword::parser;

static constexpr auto PART_NUM_LEN = 7;
static constexpr auto SERIAL_NUM_LEN = 12;
static constexpr auto CCIN_LEN = 4;

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
    KeywordVpdMap parseMemVpd();

  private:
    /**
     */
    KeywordVpdMap readKeywords(Binary::const_iterator iterator);

    /**
     */
    Binary memVpd;
};
} // namespace parser
} // namespace memory
} // namespace vpd
