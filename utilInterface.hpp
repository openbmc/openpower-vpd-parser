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

template <typename T>
class UtilityInterface
{
  public:
    virtual ~UtilityInterface()
    {
    }

    virtual T readBusProperty(const std::string& obj, const std::string& inf,
                              const std::string& prop) = 0;
};

template <typename T>
class utility : public UtilityInterface<T>
{
  public:
    virtual ~utility()
    {
    }

    T readBusProperty(const std::string& obj, const std::string& inf,
                      const std::string& prop) override
    {
        return openpower::vpd::readBusProperty<std::string>(obj, inf, prop);
    }
};

} // namespace interface
} // namespace utils
} // namespace vpd
} // namespace openpower
