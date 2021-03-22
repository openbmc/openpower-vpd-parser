#pragma once

#include "const.hpp"

#include <stdexcept>

namespace openpower
{
namespace vpd
{
namespace exceptions
{
using Severity = openpower::vpd::constants::severity::PelSeverity;

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

    /** @brief constructor
     *  @param[in] - string to define exception
     *  @param[in] - severity of the exception
     */
    VpdEccException(const std::string& msg, Severity severity) :
        VPDException(msg), exceptionSeverity(severity)
    {
    }

    /** @brief Severity getter method.
     * @return - Severity
     */
    inline Severity getSeverity() const
    {
        return exceptionSeverity;
    }

  private:
    /** To set the severity of the exception, required to set the value in case
     * PEL is logged, UNRECOVERABLE if not set otehrwise*/
    Severity exceptionSeverity = Severity::ERROR;

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

    /** @brief constructor
     *  @param[in] - string to define exception
     *  @param[in] - severity of the exception
     */
    VpdDataException(const std::string& msg, Severity severity) :
        VPDException(msg), exceptionSeverity(severity)
    {
    }

    /** @brief Severity getter method.
     * @return - Severity
     */
    inline Severity getSeverity() const
    {
        return exceptionSeverity;
    }

  private:
    /** To set the severity of the exception, required to set the value in case
     * PEL is logged, UNRECOVERABLE if not set otehrwise*/
    Severity exceptionSeverity = Severity::ERROR;
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
    VpdJsonException(const std::string& msg, const std::string& Path,
                     Severity severity) :
        VPDException(msg),
        jsonPath(Path), exceptionSeverity(severity)
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
     * @return - Severity
     */
    inline Severity getSeverity() const
    {
        return exceptionSeverity;
    }

  private:
    /** To hold the path of Json that failed to parse*/
    std::string jsonPath;

    /** To set the severity of the exception, required to set the value in case
     * PEL is logged, ERROR if not set otehrwise*/
    Severity exceptionSeverity = Severity::ERROR;

}; // class VpdJSonException

} // namespace exceptions
} // namespace vpd
} // namespace openpower