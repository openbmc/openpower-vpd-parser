#include "parser.hpp"
#include "types.hpp"
#include "utility/json_utility.hpp"
#include "utility/vpd_specific_utility.hpp"

#include <iostream>

#include <gtest/gtest.h>

using namespace vpd;

TEST(IsFruPowerOffOnlyTest, PositiveTestCase)
{
    uint16_t l_errCode = 0;
    const std::string l_jsonPath{"/usr/local/share/vpd/50001001.json"};
    const std::string l_vpdPath{"/sys/bus/spi/drivers/at25/spi12.0/eeprom"};
    const nlohmann::json l_parsedJson =
        jsonUtility::getParsedJson(l_jsonPath, l_errCode);

    if (l_errCode)
    {
        logging::logMessage(
            "Failed to parse JSON file [" + l_jsonPath +
            "], error : " + vpdSpecificUtility::getErrCodeMsg(l_errCode));
    }

    const bool l_result =
        jsonUtility::isFruPowerOffOnly(l_parsedJson, l_vpdPath);
    EXPECT_TRUE(l_result);
}

TEST(IsFruPowerOffOnlyTest, NegativeTestCase)
{
    uint16_t l_errCode = 0;
    const std::string l_jsonPath{"/usr/local/share/vpd/50001001.json"};
    const std::string l_vpdPath{"/sys/bus/i2c/drivers/at24/4-0050/eeprom"};
    const nlohmann::json l_parsedJson =
        jsonUtility::getParsedJson(l_jsonPath, l_errCode);

    if (l_errCode)
    {
        logging::logMessage(
            "Failed to parse JSON file [" + l_jsonPath +
            "], error : " + vpdSpecificUtility::getErrCodeMsg(l_errCode));
    }

    const bool l_result =
        jsonUtility::isFruPowerOffOnly(l_parsedJson, l_vpdPath);
    EXPECT_FALSE(l_result);
}
