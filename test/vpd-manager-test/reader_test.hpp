#pragma once

#include "utilInterface.hpp"

#include <gmock/gmock.h>

using namespace openpower::vpd::utils::interface;

template <typename T>
class MockUtilCalls : public UtilityInterface<T>
{

  public:
    MOCK_METHOD(T, readBusProperty,
                (const string& obj, const string& inf, const string& prop),
                (override));
};