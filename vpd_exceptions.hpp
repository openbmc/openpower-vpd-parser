#pragma once

#include "const.hpp"

#include <stdexcept>

namespace openpower
{
namespace vpd
{
namespace exceptions
{

inline std::string mapSeverityToInterface(
    const openpower::vpd::constants::severity::PelSeverity& severity)
{
    std::string pelSeverity{};

    using LogSeverity = openpower::vpd::constants::severity::PelSeverity;

    switch (severity)
    {
        case (LogSeverity::INFORMATIONAL):
            pelSeverity =
                "xyz.openbmc_project.Logging.Entry.Level.Informational";
            break;

        case (LogSeverity::DEBUG):
            pelSeverity = "xyz.openbmc_project.Logging.Entry.Level.Debug";
            break;

        case (LogSeverity::NOTICE):
            pelSeverity = "xyz.openbmc_project.Logging.Entry.Level.Notice";
            break;

        case (LogSeverity::WARNING):
            pelSeverity = "xyz.openbmc_project.Logging.Entry.Level.Warning";
            break;

        case (LogSeverity::CRITICAL):
            pelSeverity = "xyz.openbmc_project.Logging.Entry.Level.Critical";
            break;

        case (LogSeverity::EMERGENCY):
            pelSeverity = "xyz.openbmc_project.Logging.Entry.Level.Emergency";
            break;

        case (LogSeverity::ERROR):
            pelSeverity = "xyz.openbmc_project.Logging.Entry.Level.Error";
            break;

        case (LogSeverity::ALERT):
            pelSeverity = "xyz.openbmc_project.Logging.Entry.Level.Alert";
            break;

        default:
            pelSeverity = "xyz.openbmc_project.Logging.Entry.Level.Error";
            break;
    }

    return pelSeverity;
}

/** @class VPDException
 * @brief This class inherits std::runtime_error and overrrides
 * "what" method to return the description of exception.
 * This class also works as base class for custom exception
 * classes of openpower-vpd repository.
 */
class VPDException : public std::runtime_error
{
  public:
    // deleted methods
    VPDException() = delete;
    VPDException(const VPDException&) = delete;
    VPDException(VPDException&&) = delete;
    VPDException& operator=(const VPDException&) = delete;

    // default destructor
    ~VPDException() = default;

    /** @brief constructor
     *  @param[in] - string to define exception
     */
    explicit VPDException(const std::string& msg) :
        std::runtime_error(msg), errMsg(msg)
    {
    }

    /** @brief inline method to return exception string
     * This is overridden method of std::runtime class
     */
    inline const char* what() const noexcept override
    {
        return errMsg.c_str();
    }

  private:
    /** @brief string to hold the reason of exception */
    std::string errMsg;

}; // class VPDException

/** @class VpdEccException
 *  @brief This class extends Exceptions class and define
 *  type for ECC related exception in VPD
 */
class VpdEccException : public VPDException
{
  public:
    // deleted methods
    VpdEccException() = delete;
    VpdEccException(const VpdEccException&) = delete;
    VpdEccException(VpdEccException&&) = delete;
    VpdEccException& operator=(const VpdEccException&) = delete;

    // default destructor
    ~VpdEccException() = default;

    /** @brief constructor
     *  @param[in] - string to define exception
     */
    explicit VpdEccException(const std::string& msg) : VPDException(msg)
    {
    }

}; // class VpdEccException

/** @class VpdDataException
 *  @brief This class extends Exceptions class and define
 *  type for data related exception in VPD
 */
class VpdDataException : public VPDException
{
  public:
    // deleted methods
    VpdDataException() = delete;
    VpdDataException(const VpdDataException&) = delete;
    VpdDataException(VpdDataException&&) = delete;
    VpdDataException& operator=(const VpdDataException&) = delete;

    // default destructor
    ~VpdDataException() = default;

    /** @brief constructor
     *  @param[in] - string to define exception
     */
    explicit VpdDataException(const std::string& msg) : VPDException(msg)
    {
    }

}; // class VpdDataException

class VpdJsonException : public VPDException
{
  public:
    // deleted methods
    VpdJsonException() = delete;
    VpdJsonException(const VpdJsonException&) = delete;
    VpdJsonException(VpdDataException&&) = delete;
    VpdJsonException& operator=(const VpdDataException&) = delete;

    // default destructor
    ~VpdJsonException() = default;

    /** @brief constructor
     * @param[in] - string to define exception
     * @param[in] - Json path
     */
    VpdJsonException(const std::string& msg, const std::string& Path) :
        VPDException(msg), jsonPath(Path)
    {
    }

    /** @brief constructor
     * @param[in] - string to define exception
     * @param[in] - Json path
     * @param[in] - severity of the error
     */
    VpdJsonException(
        const std::string& msg, const std::string& Path,
        openpower::vpd::constants::severity::PelSeverity severity) :
        VPDException(msg),
        jsonPath(Path), pelSeverity(severity)
    {
    }

    /** @brief Json path getter method.
     * @return - Json path
     */
    inline std::string getJsonPath() const
    {
        return jsonPath;
    }

    /** @brief Severity getter method.
     * @return - Severity interface
     */
    inline std::string getSeverity() const
    {
        return mapSeverityToInterface(pelSeverity);
    }

  private:
    /** To hold the path of Json that failed to parse*/
    std::string jsonPath;

    /** To set the severity of the exception, required to set the value in case
     * PEL is logged, UNRECOVERABLE if not set otehrwise*/
    openpower::vpd::constants::severity::PelSeverity pelSeverity =
        openpower::vpd::constants::severity::PelSeverity::UNRECOVERABLE;

}; // class VpdJSonException

} // namespace exceptions
} // namespace vpd
} // namespace openpower