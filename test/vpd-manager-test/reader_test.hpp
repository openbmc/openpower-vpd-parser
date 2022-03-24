#pragma once

#include "utilInterface.hpp"

#include <gmock/gmock.h>

using namespace openpower::vpd::utils::interface;

class MockUtilCalls : public UtilityInterface
{
  public:
    MOCK_METHOD(openpower::vpd::inventory::Value, readBusProperty,
                (const string& obj, const string& inf, const string& prop),
                (override));
};
