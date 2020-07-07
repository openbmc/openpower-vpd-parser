#pragma once

#include "types.hpp"
#include "utilInterface.hpp"

#include <nlohmann/json.hpp>
namespace openpower
{
namespace vpd
{
namespace manager
{
namespace reader
{

using IUtil = openpower::vpd::utils::interface::UtilityInterface;
/** @class ReaderImpl
 *  @brief Implements functionalities related to reading of VPD related data
 *  from the system.
 */
class ReaderImpl
{
  public:
    ReaderImpl() = default;
    ReaderImpl(const ReaderImpl&) = default;
    ReaderImpl& operator=(const ReaderImpl&) = delete;
    ReaderImpl(ReaderImpl&&) = default;
    ReaderImpl& operator=(ReaderImpl&&) = delete;
    ~ReaderImpl() = default;

#ifdef ManagerTest
    explicit ReaderImpl(IUtil& obj) : utilObj(obj)
    {
    }
#endif

    /** @brief An API to expand a given unexpanded location code.
     *  @param[in] locationCode - unexpanded location code.
     *  @param[in] nodeNumber - node on which we are looking for location code.
     *  @param[in] frusLocationCode - mapping of inventory path and location
     * code.
     *  @return Expanded location code.
     */
    inventory::LocationCode getExpandedLocationCode(
        const inventory::LocationCode& locationCode,
        const inventory::NodeNumber& nodeNumber,
        const inventory::LocationCodeMap& frusLocationCode) const;

    /** @brief An API to get list of all the FRUs at the given location code
     *  @param[in] - location code in unexpanded format
     *  @param[in] - node number
     *  @param[in] - mapping of location code and Inventory path
     *  @return list of Inventory paths at the given location
     */
    inventory::ListOfPaths getFrusAtLocation(
        const inventory::LocationCode& locationCode,
        const inventory::NodeNumber& nodeNumber,
        const inventory::LocationCodeMap& frusLocationCode) const;

    /** @brief An API to split expanded location code to its un-expanded
     *  format as represented in VPD JSON and the node number.
     *  @param[in] Location code in expanded format.
     *  @return Location code in un-expanded format and its node number.
     */
    std::tuple<inventory::LocationCode, inventory::NodeNumber>
        getCollapsedLocationCode(
            const inventory::LocationCode& locationCode) const;

    /** @brief An API to read keyword data from vpd file
     *  @param[in] vpd file path.
     *  @param[in] Record Name
     *  @param[in] Keyword Name
     *  @return data of the given keyword
     */
    std::string readKwdData(const std::string& vpdFilePath,
                            const std::string& recName,
                            const std::string& kwdName);

  private:
    /** @brief An api to check validity of location code
     *  @param[in] - location code
     *  @return true/false based on validity check
     */
    bool isValidLocationCode(const inventory::LocationCode& locationCode) const;

#ifdef ManagerTest
    IUtil& utilObj;
#endif

}; // class ReaderImpl

/** @brief This API will be used to find out Parent FRU of Module/CPU
 *
 * @param[in] - jsonFile to process and find out parent
 * @param[in] - moduleObjPath, object path of that FRU
 * @param[in] - fruType, Type of Parent FRU
 *              for Module/CPU Parent Type- FruAndModule
 *
 * @return returns vpd file path of Parent Fru of that Module
 */
std::string getSysPathForThisFruType(const nlohmann::json& jsonFile,
                                     const std::string& moduleObjPath,
                                     const std::string& fruType);

} // namespace reader
} // namespace manager
} // namespace vpd
} // namespace openpower
