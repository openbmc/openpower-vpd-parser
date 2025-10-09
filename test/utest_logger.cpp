#include "logger.hpp"

#include <gtest/gtest.h>

using namespace vpd;

TEST(VpdWriteLoggerRotationTest, PositiveTestCase)
{
    auto l_logger = Logger::getLoggerInstance();

    // log 200 messages
    constexpr auto l_totalMessages{200};
    for (unsigned l_messageCount = 0; l_messageCount < l_totalMessages;
         ++l_messageCount)
    {
        l_logger->logMessage("Dummy VPD write message", PlaceHolder::VPD_WRITE);
    }
}
