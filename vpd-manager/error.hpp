#pragma once

#include <sdbusplus/exception.hpp>

namespace openpower
{
namespace vpd
{
namespace manager
{
struct DbusTestException : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.TestError";
    }
    const char* description() const noexcept override
    {
        return "Test error";
    }
    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.Test: Test error";
    }

    /*  int get_errno() const noexcept override
      {
          return EACCES;
      }*/
};

struct LocationNotFound final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.LocationNotFound";
    }

    const char* description() const noexcept override
    {
        return "Location is not found.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.LocationNotFound: Location is not found.";
    }
};

struct NodeNotFound final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.NodeNotFound";
    }

    const char* description() const noexcept override
    {
        return "Node number is not found.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.NodeNotFound: Node number is not found.";
    }
};

struct PathNotFound final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.PathNotFound";
    }

    const char* description() const noexcept override
    {
        return "Inventory path is not found.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.PathNotFound: Inventory path is not found.";
    }
};

struct RecordNotFound final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.RecordNotFound";
    }

    const char* description() const noexcept override
    {
        return "Record not found.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.RecordNotFound: Record not found.";
    }
};

struct KeywordNotFound final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.KeywordNotFound";
    }

    const char* description() const noexcept override
    {
        return "Keyword is not found.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.KeywordNotFound: Keyword is not found.";
    }
};

struct BlankSystemVPD final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.BlankSystemVPD";
    }

    const char* description() const noexcept override
    {
        return "System VPD is blank on both hardware and cache. On IBM "
               "systems, "
               "certain VPD data must be available for the system to boot. "
               "This error "
               "is used to indicate that no valid data was found by the BMC.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.BlankSystemVPD: System VPD is blank on both "
               "hardware and cache. On IBM systems, certain VPD data must be "
               "available for the system to boot. This error is used to "
               "indicate that "
               "no valid data was found by the BMC.";
    }
};

struct InvalidEepromPath final
    : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.InvalidEepromPath";
    }

    const char* description() const noexcept override
    {
        return "EEPROM path is invalid. Parser failed to access the path.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.InvalidEepromPath: EEPROM path is invalid. "
               "Parser "
               "failed to access the path.";
    }
};

struct InvalidVPD final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.InvalidVPD";
    }

    const char* description() const noexcept override
    {
        return "VPD file is not valid. Mandatory records are missing in VPD "
               "file.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.InvalidVPD: VPD file is not valid. Mandatory "
               "records are missing in VPD file.";
    }
};

struct EccCheckFailed final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.EccCheckFailed";
    }

    const char* description() const noexcept override
    {
        return "";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.EccCheckFailed: ";
    }
};

struct InvalidJson final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.InvalidJson";
    }

    const char* description() const noexcept override
    {
        return "Invalid Json file.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.InvalidJson: Invalid Json file.";
    }
};

struct DbusFailure final : public sdbusplus::exception::generated_exception
{
    const char* name() const noexcept override
    {
        return "com.ibm.VPD.Error.DbusFailure";
    }

    const char* description() const noexcept override
    {
        return "DBus error occurred.";
    }

    const char* what() const noexcept override
    {
        return "com.ibm.VPD.Error.DbusFailure: DBus error occurred.";
    }
};

} // namespace manager
} // namespace vpd
} // namespace openpower