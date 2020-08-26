#pragma once

#include <stdexcept>

namespace openpower
{
namespace vpd
{
namespace exceptions
{

/** @class VPDException
 *  @brief This class inherits std::runtime_error and overrrides
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

    /** @brief constructor
     *  @param[in] - string to define exception
     */
    explicit VPDException(const std::string& msg) :
        std::runtime_error(msg), errMsg(msg)
    {
    }

    /** @brief destructor
     */
    virtual ~VPDException() noexcept
    {
    }

    /** @brief inlline method to return exception string
     * This is overridden method of std::runtime class
     */
    inline const char* what() const throw() override
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

    /** @brief constructor
     *  @param[in] - string to define exception
     */
    explicit VpdEccException(const std::string& msg) : VPDException(msg)
    {
    }

    /** @brief destructor
     */
    virtual ~VpdEccException() noexcept
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

    /** @brief constructor
     *  @param[in] - string to define exception
     */
    explicit VpdDataException(const std::string& msg) : VPDException(msg)
    {
    }

    /** @brief destructor
     */
    virtual ~VpdDataException() noexcept
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

    /** @brief constructor
     *  @param[in] - string to define exception
     */
    explicit VpdJsonException(const std::string& msg, const std::string& Path) :
        VPDException(msg), jsonPath(Path)
    {
    }

    /** @brief destructor
     */
    virtual ~VpdJsonException() noexcept
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

} // namespace exceptions
} // namespace vpd
} // namespace openpower