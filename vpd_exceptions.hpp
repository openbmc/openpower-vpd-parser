#pragma once

#include <stdexcept>

namespace openpower
{
namespace vpd
{
namespace exceptions
{

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
    explicit VpdEccException(const std::string& msg, std::string rec,
                             std::string failedRecord, std::string failedEcc) :
        VPDException(msg),
        recordName(rec), failedRecordData(failedRecord),
        failedEccData(failedEcc)
    {
    }

    inline std::string getRecord() const
    {
        return recordName;
    }

    inline std::string getFailedRecordData() const
    {
        return failedRecordData;
    }

    inline std::string getFailedEccData() const
    {
        return failedEccData;
    }

  private:
    std::string recordName;
    std::string failedRecordData;
    std::string failedEccData;

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
     *  @param[in] - string to define exception
     */
    explicit VpdJsonException(const std::string& msg, const std::string& Path) :
        VPDException(msg), jsonPath(Path)
    {
    }

    inline std::string getJsonPath() const
    {
        return jsonPath;
    }

  private:
    /** To hold the path of Json that failed to parse*/
    std::string jsonPath;

}; // class VpdJSonException

class EepromException : public VPDException
{
  public:
    EepromException() = delete;
    EepromException(const EepromException&) = delete;
    EepromException(EepromException&&) = delete;
    EepromException& operator=(const EepromException&) = delete;

    ~EepromException() = default;

    /** @brief constructor
     * @param[in] -
     */
    explicit EepromException(const std::string& msg,
                             const std::string& eepromFile, const int error,
                             const std::string& strError) :
        VPDException(msg),
        eepromPath(eepromFile), errorCode(error), errorDesc(strError)
    {
    }

    inline std::string getEepromPath() const
    {
        return eepromPath;
    }

    inline int getErrno() const
    {
        return errorCode;
    }
    inline std::string getErrorDesc() const
    {
        return errorDesc;
    }

  private:
    std::string eepromPath;
    int errorCode;
    std::string errorDesc;

}; // class EepromException
} // namespace exceptions
} // namespace vpd
} // namespace openpower
