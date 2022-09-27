#pragma once

#include "utilInterface.hpp"

#include <gmock/gmock.h>

using namespace openpower::vpd::utils::interface;

class MockUtilCalls : public UtilityInterface
{
  public:
    MOCK_METHOD(std::string, readBusProperty,
                (const std::string& obj, const std::string& inf,
                 const std::string& prop),
                (override));
};
