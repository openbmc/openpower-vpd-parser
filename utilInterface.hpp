#pragma once
#include "ibm_vpd_utils.hpp"

#include <string>

namespace openpower
{
namespace vpd
{
namespace utils
{
namespace interface
{

class UtilityInterface
{
  public:
    virtual ~UtilityInterface() {}

    virtual std::string readBusProperty(const std::string& obj,
                                        const std::string& inf,
                                        const std::string& prop) = 0;
};

class utility : public UtilityInterface
{
  public:
    virtual ~utility() {}

    std::string readBusProperty(const std::string& obj, const std::string& inf,
                                const std::string& prop) override
    {
        return openpower::vpd::readBusProperty(obj, inf, prop);
    }
};

} // namespace interface
} // namespace utils
} // namespace vpd
} // namespace openpower
