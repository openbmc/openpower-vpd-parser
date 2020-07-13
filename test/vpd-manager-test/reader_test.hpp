#pragma once

#include "utilInterface.hpp"

#include <gmock/gmock.h>

using namespace std;
using namespace openpower::vpd::utils::interface;

class MockUtilCalls : public UtilityInterface
{
  public:
    MOCK_METHOD(std::string, readBusProperty,
                (const string& obj, const string& inf, const string& prop),
                (override));
};
