#pragma once

#include "types.hpp"

#include <stdexcept>
#include <system_error>

namespace vpd
{
/** @class Exception
 * @brief This class inherits std::runtime_error and overrides "what" method
 * to return the description of exception.
 * This class also works as base class for custom exception classes for
 * VPD repository.
 */
class Exception : public std::runtime_error
{
  public:
    // deleted methods
    Exception() = delete;
    Exception(const Exception&) = delete;
    Exception(Exception&&) = delete;
    Exception& operator=(const Exception&) = delete;

    // default destructor
    ~Exception() = default;

    /** @brief constructor
     *
     *  @param[in] msg - Information w.r.t exception.
     */
    explicit Exception(const std::string& msg) :
        std::runtime_error(msg), m_errMsg(msg)
    {}

    /** @brief inline method to return exception string.
     *
     * This is overridden method of std::runtime class.
     */
    inline const char* what() const noexcept override
    {
        return m_errMsg.c_str();
    }

    // TODO: Create getErrorType api by defining VPD default error type

  private:
    /** @brief string to hold the reason of exception */
    std::string m_errMsg;

}; // class Exception

/** @class EccException
 *
 *  @brief This class extends Exceptions class and define type for ECC related
 * exception in VPD.
 */
class EccException : public Exception
{
  public:
    // deleted methods
    EccException() = delete;
    EccException(const EccException&) = delete;
    EccException(EccException&&) = delete;
    EccException& operator=(const EccException&) = delete;

    // default destructor
    ~EccException() = default;

    /** @brief constructor
     *
     *  @param[in] msg - Information w.r.t exception.
     */
    explicit EccException(const std::string& msg) : Exception(msg) {}

    /** @brief Method to get error type
     *
     * @return Error type which has to be logged for errors of type
     * EccException.
     */
    types::ErrorType getErrorType() const
    {
        return types::ErrorType::EccCheckFailed;
    }

}; // class EccException

/** @class DataException
 *
 * @brief This class extends Exceptions class and define type for data related
 * exception in VPD
 */
class DataException : public Exception
{
  public:
    // deleted methods
    DataException() = delete;
    DataException(const DataException&) = delete;
    DataException(DataException&&) = delete;
    DataException& operator=(const DataException&) = delete;

    // default destructor
    ~DataException() = default;

    /** @brief constructor
     *
     *  @param[in] msg - string to define exception
     */
    explicit DataException(const std::string& msg) : Exception(msg) {}

    /** @brief Method to get error type
     *
     * @return Error type which has to be logged for errors of type
     * DataException.
     */
    types::ErrorType getErrorType() const
    {
        return types::ErrorType::InvalidVpdMessage;
    }
}; // class DataException

class JsonException : public Exception
{
  public:
    // deleted methods
    JsonException() = delete;
    JsonException(const JsonException&) = delete;
    JsonException(JsonException&&) = delete;
    JsonException& operator=(const JsonException&) = delete;

    // default destructor
    ~JsonException() = default;

    /** @brief constructor
     *  @param[in] msg - Information w.r.t. exception.
     *  @param[in] path - Json path
     */
    JsonException(const std::string& msg, const std::string& path) :
        Exception(msg), m_jsonPath(path)
    {}

    /** @brief constructor
     *  @param[in] msg - Information w.r.t. exception.
     */
    JsonException(const std::string& msg) : Exception(msg) {}

    /** @brief Json path getter method.
     *
     *  @return - Json path
     */
    inline std::string getJsonPath() const
    {
        return m_jsonPath;
    }

    /** @brief Method to get error type
     *
     * @return Error type which has to be logged for errors of type
     * JsonException.
     */
    types::ErrorType getErrorType() const
    {
        return types::ErrorType::JsonFailure;
    }

  private:
    /** To hold the path of Json that failed*/
    std::string m_jsonPath;

}; // class JSonException

/** @class GpioException
 *  @brief Custom handler for GPIO exception.
 *
 *  This class extends Exceptions class and define
 *  type for GPIO related exception in VPD.
 */
class GpioException : public Exception
{
  public:
    // deleted methods
    GpioException() = delete;
    GpioException(const GpioException&) = delete;
    GpioException(GpioException&&) = delete;
    GpioException& operator=(const GpioException&) = delete;

    // default destructor
    ~GpioException() = default;

    /** @brief constructor
     *  @param[in] msg - string to define exception
     */
    explicit GpioException(const std::string& msg) : Exception(msg) {}

    /** @brief Method to get error type
     *
     * @return Error type which has to be logged for errors of type
     * GpioException.
     */
    types::ErrorType getErrorType() const
    {
        return types::ErrorType::GpioError;
    }
};

/** @class DbusException
 *  @brief Custom handler for Dbus exception.
 *
 *  This class extends Exceptions class and define
 *  type for DBus related exception in VPD.
 */
class DbusException : public Exception
{
  public:
    // deleted methods
    DbusException() = delete;
    DbusException(const DbusException&) = delete;
    DbusException(DbusException&&) = delete;
    DbusException& operator=(const DbusException&) = delete;

    // default destructor
    ~DbusException() = default;

    /** @brief constructor
     *  @param[in] msg - string to define exception
     */
    explicit DbusException(const std::string& msg) : Exception(msg) {}

    /** @brief Method to get error type
     *
     * @return Error type which has to be logged for errors of type
     * DbusException.
     */
    types::ErrorType getErrorType() const
    {
        return types::ErrorType::DbusFailure;
    }
};

/** @class FirmwareException
 *  @brief Custom handler for firmware exception.
 *
 *  This class extends Exceptions class and define
 *  type for generic firmware related exception in VPD.
 */
class FirmwareException : public Exception
{
  public:
    // deleted methods
    FirmwareException() = delete;
    FirmwareException(const FirmwareException&) = delete;
    FirmwareException(FirmwareException&&) = delete;
    FirmwareException& operator=(const FirmwareException&) = delete;

    // default destructor
    ~FirmwareException() = default;

    /** @brief constructor
     *  @param[in] msg - string to define exception
     */
    explicit FirmwareException(const std::string& msg) : Exception(msg) {}

    /** @brief Method to get error type
     *
     * @return Error type which has to be logged for errors of type
     * FirmwareException.
     */
    types::ErrorType getErrorType() const
    {
        return types::ErrorType::InternalFailure;
    }
};

/** @class EepromException
 *  @brief Custom handler for EEPROM exception.
 *
 *  This class extends Exceptions class and define
 *  type for EEPROM related exception in VPD.
 */
class EepromException : public Exception
{
  public:
    // deleted methods
    EepromException() = delete;
    EepromException(const EepromException&) = delete;
    EepromException(EepromException&&) = delete;
    EepromException& operator=(const EepromException&) = delete;

    // default destructor
    ~EepromException() = default;

    /** @brief constructor
     *  @param[in] msg - string to define exception
     */
    explicit EepromException(const std::string& msg) : Exception(msg) {}

    /** @brief Method to get error type
     *
     * @return Error type which has to be logged for errors of type
     * EepromException.
     */
    types::ErrorType getErrorType() const
    {
        return types::ErrorType::InvalidEeprom;
    }
}; // class EepromException

/** @class SystemException
 *  @brief Custom handler for system-level (errno-based) exceptions.
 *
 *  This class extends Exception and defines type for system-level
 *  (errno-based) exception in VPD.
 */
class SystemException final : public Exception
{
  public:
    // deleted methods
    SystemException() = delete;
    SystemException(const SystemException&) = delete;
    SystemException(SystemException&&) = delete;
    SystemException& operator=(const SystemException&) = delete;

    // default destructor
    ~SystemException() = default;

    /** @brief Constructor
     *
     *  @param[in] i_errnoVal - errno value captured after the failing call.
     *  @param[in] i_msg      - Additional context message.
     */
    SystemException(int i_errnoVal, const std::string& i_msg) :
        Exception(
            i_msg + ": " +
            std::error_code(i_errnoVal, std::generic_category()).message()),
        m_errno(i_errnoVal)
    {}

    /** @brief Method to get errno value. */
    int getErrno() const noexcept
    {
        return m_errno;
    }

    /** @brief Method to get error type
     *
     * @return Error type which has to be logged for errors of type
     * SystemException.
     */
    types::ErrorType getErrorType() const
    {
        // TODO Exact reason needs to be put down else the error message in
        // the PEL will read as if the EEPROM is ok but the code failed to
        // read the EEPROM, which can be misleading. Support runtime ErrorType.
        return types::ErrorType::EssentialFru;
    }

  private:
    int m_errno;

}; // class SystemException

} // namespace vpd
