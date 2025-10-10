#include "utility/common_utility.hpp"

#include <utility/vpd_specific_utility.hpp>

#include <cassert>
#include <string>

#include <gtest/gtest.h>

using namespace vpd;

TEST(UtilsTest, TestValidValue)
{
    uint16_t l_errCode = 0;
    std::string key = "VINI";
    std::string encoding = "MAC";
    std::string expected = "56:49:4e:49";
    EXPECT_EQ(expected,
              vpdSpecificUtility::encodeKeyword(key, encoding, l_errCode));
    if (l_errCode)
    {
        logging::logMessage(
            "Failed to get encoded keyword value for : " + key +
            ", error : " + commonUtility::getErrCodeMsg(l_errCode));
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
