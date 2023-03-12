#include <utility/vpd_specific_utility.hpp>

#include <cassert>
#include <string>

#include <gtest/gtest.h>

using namespace vpd;

TEST(UtilsTest, TestValidValue)
{
    std::string key = "VINI";
    std::string encoding = "MAC";
    std::string expected = "56:49:4e:49";
    EXPECT_EQ(expected, vpdSpecificUtility::encodeKeyword(key, encoding));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
