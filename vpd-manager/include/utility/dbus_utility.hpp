#pragma once

#include "constants.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <chrono>

namespace vpd
{
/**
 * @brief The namespace defines utlity methods for generic D-Bus operations.
 */
namespace dbusUtility
{

/**
 * @brief An API to get Map of service and interfaces for an object path.
 *
 * The API returns a Map of service name and interfaces for a given pair of
 * object path and interface list. It can be used to determine service name
 * which implemets a particular object path and interface.
 *
 * Note: It will be caller's responsibility to check for empty map returned and
 * generate appropriate error.
 *
 * @param [in] objectPath - Object path under the service.
 * @param [in] interfaces - Vector of interface(s).
 * @return - A Map of service name to object to interface(s), if success.
 *           If failed,  empty map.
 */
inline types::MapperGetObject getObjectMap(
    const std::string& objectPath, const std::vector<std::string>& interfaces)
{
    types::MapperGetObject getObjectMap;

    // interface list is optional argument, hence no check required.
    if (objectPath.empty())
    {
        logging::logMessage("Path value is empty, invalid call to GetObject");
        return getObjectMap;
    }

    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(
            "xyz.openbmc_project.ObjectMapper",
            "/xyz/openbmc_project/object_mapper",
            "xyz.openbmc_project.ObjectMapper", "GetObject");

        method.append(objectPath, interfaces);
        auto result = bus.call(method);
        result.read(getObjectMap);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        // logging::logMessage(e.what());
        return getObjectMap;
    }

    return getObjectMap;
}

/**
 * @brief An API to get property map for an interface.
 *
 * This API returns a map of property and its value with respect to a particular
 * interface.
 *
 * Note: It will be caller's responsibility to check for empty map returned and
 * generate appropriate error.
 *
 * @param[in] i_service - Service name.
 * @param[in] i_objectPath - object path.
 * @param[in] i_interface - Interface, for the properties to be listed.
 *
 * @return - A map of property and value of an interface, if success.
 *           if failed, empty map.
 */
inline types::PropertyMap getPropertyMap(const std::string& i_service,
                                         const std::string& i_objectPath,
                                         const std::string& i_interface)
{
    types::PropertyMap l_propertyValueMap;
    if (i_service.empty() || i_objectPath.empty() || i_interface.empty())
    {
        logging::logMessage("Invalid parameters to get property map");
        return l_propertyValueMap;
    }

    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method =
            l_bus.new_method_call(i_service.c_str(), i_objectPath.c_str(),
                                  "org.freedesktop.DBus.Properties", "GetAll");
        l_method.append(i_interface);
        auto l_result = l_bus.call(l_method);
        l_result.read(l_propertyValueMap);
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        logging::logMessage(l_ex.what());
    }

    return l_propertyValueMap;
}

/**
 * @brief API to get object subtree from D-bus.
 *
 * The API returns the map of object, services and interfaces in the
 * subtree that implement a certain interface. If no interfaces are provided
 * then all the objects, services and interfaces under the subtree will
 * be returned.
 *
 * Note: Depth can be 0 and interfaces can be null.
 * It will be caller's responsibility to check for empty vector returned
 * and generate appropriate error.
 *
 * @param[in] i_objectPath - Path to search for an interface.
 * @param[in] i_depth - Maximum depth of the tree to search.
 * @param[in] i_interfaces - List of interfaces to search.
 *
 * @return - A map of object and its related services and interfaces, if
 *           success. If failed, empty map.
 */

inline types::MapperGetSubTree getObjectSubTree(
    const std::string& i_objectPath, const int& i_depth,
    const std::vector<std::string>& i_interfaces)
{
    types::MapperGetSubTree l_subTreeMap;

    if (i_objectPath.empty())
    {
        logging::logMessage("Object path is empty.");
        return l_subTreeMap;
    }

    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method = l_bus.new_method_call(
            constants::objectMapperService, constants::objectMapperPath,
            constants::objectMapperInf, "GetSubTree");
        l_method.append(i_objectPath, i_depth, i_interfaces);
        auto l_result = l_bus.call(l_method);
        l_result.read(l_subTreeMap);
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        logging::logMessage(l_ex.what());
    }

    return l_subTreeMap;
}

/**
 * @brief An API to read property from Dbus.
 *
 * The caller of the API needs to validate the validatity and correctness of the
 * type and value of data returned. The API will just fetch and retun the data
 * without any data validation.
 *
 * Note: It will be caller's responsibility to check for empty value returned
 * and generate appropriate error if required.
 *
 * @param [in] serviceName - Name of the Dbus service.
 * @param [in] objectPath - Object path under the service.
 * @param [in] interface - Interface under which property exist.
 * @param [in] property - Property whose value is to be read.
 * @return - Value read from Dbus, if success.
 *           If failed, empty variant.
 */
inline types::DbusVariantType readDbusProperty(
    const std::string& serviceName, const std::string& objectPath,
    const std::string& interface, const std::string& property)
{
    types::DbusVariantType propertyValue;

    // Mandatory fields to make a read dbus call.
    if (serviceName.empty() || objectPath.empty() || interface.empty() ||
        property.empty())
    {
        logging::logMessage(
            "One of the parameter to make Dbus read call is empty.");
        return propertyValue;
    }

    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call(serviceName.c_str(), objectPath.c_str(),
                                "org.freedesktop.DBus.Properties", "Get");
        method.append(interface, property);

        auto result = bus.call(method);
        result.read(propertyValue);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        return propertyValue;
    }
    return propertyValue;
}

/**
 * @brief An API to write property on Dbus.
 *
 * The caller of this API needs to handle exception thrown by this method to
 * identify any write failure. The API in no other way indicate write  failure
 * to the caller.
 *
 * @param [in] serviceName - Name of the Dbus service.
 * @param [in] objectPath - Object path under the service.
 * @param [in] interface - Interface under which property exist.
 * @param [in] property - Property whose value is to be written.
 * @param [in] propertyValue - The value to be written.
 * @return True if write on DBus is success, false otherwise.
 */
inline bool writeDbusProperty(
    const std::string& serviceName, const std::string& objectPath,
    const std::string& interface, const std::string& property,
    const types::DbusVariantType& propertyValue)
{
    try
    {
        // Mandatory fields to make a write dbus call.
        if (serviceName.empty() || objectPath.empty() || interface.empty() ||
            property.empty())
        {
            throw std::runtime_error("Dbus write failed, Parameter empty");
        }

        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call(serviceName.c_str(), objectPath.c_str(),
                                "org.freedesktop.DBus.Properties", "Set");
        method.append(interface, property, propertyValue);
        bus.call(method);

        return true;
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "DBus write failed, error: " + std::string(l_ex.what()));
        return false;
    }
}

/**
 * @brief API to publish data on PIM
 *
 * The API calls notify on PIM object to publlish VPD.
 *
 * @param[in] objectMap - Object, its interface and data.
 * @return bool - Status of call to PIM notify.
 */
inline bool callPIM(types::ObjectMap&& objectMap)
{
    try
    {
        for (const auto& l_objectKeyValue : objectMap)
        {
            if (l_objectKeyValue.first.str.find(constants::pimPath, 0) !=
                std::string::npos)
            {
                auto l_nodeHandle = objectMap.extract(l_objectKeyValue.first);
                l_nodeHandle.key() = l_nodeHandle.key().str.replace(
                    0, std::strlen(constants::pimPath), "");
                objectMap.insert(std::move(l_nodeHandle));
            }
        }

        auto bus = sdbusplus::bus::new_default();
        auto pimMsg =
            bus.new_method_call(constants::pimServiceName, constants::pimPath,
                                constants::pimIntf, "Notify");
        pimMsg.append(std::move(objectMap));
        bus.call(pimMsg);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        return false;
    }
    return true;
}

/**
 * @brief API to call Entity manager method
 */
template <typename... Args>
inline bool callEmMethod(Args&&... args)
{
    // Once decided, define it as constants
    constexpr auto serviceName = "";
    constexpr auto objectPath = "";
    constexpr auto interface = "";
    constexpr auto methodName = "";

    auto bus = sdbusplus::bus::new_default();
    auto methodCall =
        bus.new_method_call(serviceName, objectPath, interface, methodName);

    // Will decide (move(args) ?) once we have a final method signature decided.
    // Based on signature, we will know what is the return type, or it's void.
    // Currently we dont know which method and what are the arguments.
    methodCall.append(args...);
    bus.call(methodCall);

    return true;
}

/**
 * @brief API to publish data on DBus using PIM.
 */
template <typename... Args>
inline bool callPimNotify(Args&&... args)
{
    logging::logMessage("DBG:callPimNotify called");
    try
    {
        // Extract objectMap and verify
        types::ObjectMap* objectMapPtr = nullptr;
        ((std::is_same_v<Args, types::ObjectMap> ? objectMapPtr = &args
                                                 : objectMapPtr),
         ...);

        if (objectMapPtr == nullptr)
            return false; // throw error;

        auto objectMap = *objectMapPtr;
        for (const auto& l_objectKeyValue : objectMap)
        {
            if (l_objectKeyValue.first.str.find(constants::pimPath, 0) !=
                std::string::npos)
            {
                auto l_nodeHandle = objectMap.extract(l_objectKeyValue.first);
                l_nodeHandle.key() = l_nodeHandle.key().str.replace(
                    0, std::strlen(constants::pimPath), "");
                objectMap.insert(std::move(l_nodeHandle));
            }
        }

        auto bus = sdbusplus::bus::new_default();
        auto pimMsg =
            bus.new_method_call(constants::pimServiceName, constants::pimPath,
                                constants::pimIntf, "Notify");
        pimMsg.append(std::move(objectMap));
        bus.call(pimMsg);
        logging::logMessage("DBG:callPimNotify Done");
    }
    catch (const sdbusplus::exception::SdBusError& ex)
    {
        logging::logMessage("Exception thrown:" + std::string(ex.what()));
        return false;
    }
    return true;
}

inline bool publishVpdOnDBus(types::ObjectMap&& objectMap)
{
    logging::logMessage("DBG:Alpana patch running");

    bool (*dBusCall)(types::ObjectMap&&);

#if IBM_SYSTEM
    dBusCall = callPimNotify;
#else
    dBusCall = callEmMethod;
#endif

    auto reply = dBusCall(move(objectMap));
    logging::logMessage(
        "DBG:dbus method call completed, check the output on dbus");

    return reply;
}

/**
 * @brief API to check if a D-Bus service is running or not.
 *
 * Any failure in calling the method "NameHasOwner" implies that the service is
 * not in a running state. Hence the API returns false in case of any exception
 * as well.
 *
 * @param[in] i_serviceName - D-Bus service name whose status is to be checked.
 * @return bool - True if the service is running, false otherwise.
 */
inline bool isServiceRunning(const std::string& i_serviceName)
{
    bool l_retVal = false;

    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method = l_bus.new_method_call(
            "org.freedesktop.DBus", "/org/freedesktop/DBus",
            "org.freedesktop.DBus", "NameHasOwner");
        l_method.append(i_serviceName);

        l_bus.call(l_method).read(l_retVal);
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        logging::logMessage(
            "Call to check service status failed with exception: " +
            std::string(l_ex.what()));
    }

    return l_retVal;
}

/**
 * @brief API to call "GetAttribute" method uner BIOS manager.
 *
 * The API reads the given attribuute from BIOS and returns a tuple of both
 * current as well as pending value for that attribute.
 * The API return only the current attribute value if found.
 * API returns an empty variant of type BiosAttributeCurrentValue in case of any
 * error.
 *
 * @param[in] i_attributeName - Attribute to be read.
 * @return Tuple of PLDM attribute Type, current attribute value and pending
 * attribute value.
 */
inline types::BiosAttributeCurrentValue biosGetAttributeMethodCall(
    const std::string& i_attributeName) noexcept
{
    types::BiosGetAttrRetType l_attributeVal;
    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method = l_bus.new_method_call(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "GetAttribute");
        l_method.append(i_attributeName);

        auto l_result = l_bus.call(l_method);
        l_result.read(std::get<0>(l_attributeVal), std::get<1>(l_attributeVal),
                      std::get<2>(l_attributeVal));
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        logging::logMessage(
            "Failed to read BIOS Attribute: " + i_attributeName +
            " due to error " + std::string(l_ex.what()));

        // TODO: Log an informational PEL here.
    }

    return std::get<1>(l_attributeVal);
}

/**
 * @brief API to check if Chassis is powered on.
 *
 * This API queries Phosphor Chassis State Manager to know whether
 * Chassis is powered on.
 *
 * @return true if chassis is powered on, false otherwise
 */
inline bool isChassisPowerOn()
{
    auto powerState = dbusUtility::readDbusProperty(
        "xyz.openbmc_project.State.Chassis",
        "/xyz/openbmc_project/state/chassis0",
        "xyz.openbmc_project.State.Chassis", "CurrentPowerState");

    if (auto curPowerState = std::get_if<std::string>(&powerState))
    {
        if ("xyz.openbmc_project.State.Chassis.PowerState.On" == *curPowerState)
        {
            return true;
        }
        return false;
    }

    /*
        TODO: Add PEL.
        Callout: Firmware callout
        Type: Informational
        Description: Chassis state can't be determined, defaulting to chassis
        off. : e.what()
    */
    return false;
}

/**
 * @brief API to check if host is in running state.
 *
 * This API reads the current host state from D-bus and returns true if the host
 * is running.
 *
 * @return true if host is in running state. false otherwise.
 */
inline bool isHostRunning()
{
    const auto l_hostState = dbusUtility::readDbusProperty(
        constants::hostService, constants::hostObjectPath,
        constants::hostInterface, "CurrentHostState");

    if (const auto l_currHostState = std::get_if<std::string>(&l_hostState))
    {
        if (*l_currHostState == constants::hostRunningState)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief API to check if BMC is in ready state.
 *
 * This API reads the current state of BMC from D-bus and returns true if BMC is
 * in ready state.
 *
 * @return true if BMC is ready, false otherwise.
 */
inline bool isBMCReady()
{
    const auto l_bmcState = dbusUtility::readDbusProperty(
        constants::bmcStateService, constants::bmcZeroStateObject,
        constants::bmcStateInterface, constants::currentBMCStateProperty);

    if (const auto l_currBMCState = std::get_if<std::string>(&l_bmcState))
    {
        if (*l_currBMCState == constants::bmcReadyState)
        {
            return true;
        }
    }

    return false;
}

/**
 * @brief An API to enable BMC reboot guard
 *
 * This API does a D-Bus method call to enable BMC reboot guard.
 *
 * @return On success, returns 0, otherwise returns -1.
 */
inline int EnableRebootGuard() noexcept
{
    int l_rc{constants::FAILURE};
    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method = l_bus.new_method_call(
            constants::systemdService, constants::systemdObjectPath,
            constants::systemdManagerInterface, "StartUnit");
        l_method.append("reboot-guard-enable.service", "replace");
        l_bus.call_noreply(l_method);
        l_rc = constants::SUCCESS;
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        std::string l_errMsg =
            "D-Bus call to enable BMC reboot guard failed for reason: ";
        l_errMsg += l_ex.what();

        logging::logMessage(l_errMsg);
    }
    return l_rc;
}

/**
 * @brief An API to disable BMC reboot guard
 *
 * This API disables BMC reboot guard. This API has an inbuilt re-try mechanism.
 * If Disable Reboot Guard fails, this API attempts to Disable Reboot Guard for
 * 3 more times at an interval of 333ms.
 *
 * @return On success, returns 0, otherwise returns -1.
 */
inline int DisableRebootGuard() noexcept
{
    int l_rc{constants::FAILURE};

    // A lambda which executes the DBus call to disable BMC reboot guard.
    auto l_executeDisableRebootGuard = []() -> int {
        int l_dBusCallRc{constants::FAILURE};
        try
        {
            auto l_bus = sdbusplus::bus::new_default();
            auto l_method = l_bus.new_method_call(
                constants::systemdService, constants::systemdObjectPath,
                constants::systemdManagerInterface, "StartUnit");
            l_method.append("reboot-guard-disable.service", "replace");
            l_bus.call_noreply(l_method);
            l_dBusCallRc = constants::SUCCESS;
        }
        catch (const sdbusplus::exception::SdBusError& l_ex)
        {}
        return l_dBusCallRc;
    };

    if (constants::FAILURE == l_executeDisableRebootGuard())
    {
        std::function<void()> l_retryDisableRebootGuard;

        // A lambda which tries to disable BMC reboot guard for 3 times at an
        // interval of 333 ms.
        l_retryDisableRebootGuard = [&]() {
            constexpr int MAX_RETRIES{3};
            static int l_numRetries{0};

            if (l_numRetries < MAX_RETRIES)
            {
                l_numRetries++;
                if (constants::FAILURE == l_executeDisableRebootGuard())
                {
                    // sleep for 333ms before next retry. This is just a random
                    // value so that 3 re-tries * 333ms takes ~1 second in the
                    // worst case.
                    const std::chrono::milliseconds l_sleepTime{333};
                    std::this_thread::sleep_for(l_sleepTime);
                    l_retryDisableRebootGuard();
                }
                else
                {
                    l_numRetries = 0;
                    l_rc = constants::SUCCESS;
                }
            }
            else
            {
                // Failed to Disable Reboot Guard even after 3 retries.
                logging::logMessage("Failed to Disable Reboot Guard after " +
                                    std::to_string(MAX_RETRIES) + " re-tries");
                l_numRetries = 0;
            }
        };

        l_retryDisableRebootGuard();
    }
    else
    {
        l_rc = constants::SUCCESS;
    }
    return l_rc;
}

/**
 * @brief API to notify FRU VPD Collection status.
 *
 * This API uses PIM's Notify method to update the given FRU VPD collection
 * status on D-bus.
 *
 * @param[in] i_inventoryPath - D-bus inventory path
 * @param[in] i_fruCollectionStatus - FRU VPD collection status.
 *
 * @return true if update succeeds, false otherwise.
 */
inline bool notifyFRUCollectionStatus(const std::string& i_inventoryPath,
                                      const std::string& i_fruCollectionStatus)
{
    types::ObjectMap l_objectMap;
    types::InterfaceMap l_interfaceMap;
    types::PropertyMap l_propertyMap;

    l_propertyMap.emplace("Status", i_fruCollectionStatus);
    l_interfaceMap.emplace(constants::vpdCollectionInterface, l_propertyMap);
    l_objectMap.emplace(i_inventoryPath, l_interfaceMap);

    if (!dbusUtility::callPIM(std::move(l_objectMap)))
    {
        return false;
    }

    return true;
}

/**
 * @brief API to read IM keyword from Dbus.
 *
 * @return IM value read from Dbus, Empty in case of any error.
 */
inline types::BinaryVector getImFromDbus()
{
    const auto& l_retValue = dbusUtility::readDbusProperty(
        constants::pimServiceName, constants::systemVpdInvPath,
        constants::vsbpInf, constants::kwdIM);

    auto l_imValue = std::get_if<types::BinaryVector>(&l_retValue);
    if (!l_imValue || (*l_imValue).size() != constants::VALUE_4)
    {
        return types::BinaryVector{};
    }

    return *l_imValue;
}

/**
 * @brief API to return prefix of functional firmware image.
 *
 * Every functional image belongs to a series which is denoted by the first two
 * characters of the image name. The API extracts that and returns it to the
 * caller.
 *
 * @return Two character string, empty string in case of any error.
 */
inline std::string getImagePrefix()
{
    try
    {
        types::DbusVariantType l_retVal = readDbusProperty(
            constants::objectMapperService, constants::functionalImageObjPath,
            constants::associationInterface, "endpoints");

        auto l_listOfFunctionalPath =
            std::get_if<std::vector<std::string>>(&l_retVal);

        if (!l_listOfFunctionalPath || (*l_listOfFunctionalPath).empty())
        {
            throw DbusException("failed to get functional image path.");
        }

        for (const auto& l_imagePath : *l_listOfFunctionalPath)
        {
            types::DbusVariantType l_retValPriority =
                readDbusProperty(constants::imageUpdateService, l_imagePath,
                                 constants::imagePrirotyInf, "Priority");

            auto l_imagePriority = std::get_if<uint8_t>(&l_retValPriority);
            if (!l_imagePriority)
            {
                throw DbusException(
                    "failed to read functional image priority for path [" +
                    l_imagePath + "]");
            }

            // only interested in running image.
            if (*l_imagePriority != constants::VALUE_0)
            {
                continue;
            }

            types::DbusVariantType l_retExVer = readDbusProperty(
                constants::imageUpdateService, l_imagePath,
                constants::imageExtendedVerInf, "ExtendedVersion");

            auto l_imageExtendedVersion = std::get_if<std::string>(&l_retExVer);
            if (!l_imageExtendedVersion)
            {
                throw DbusException(
                    "Unable to read extended version for the functional image [" +
                    l_imagePath + "]");
            }

            if ((*l_imageExtendedVersion).empty() ||
                (*l_imageExtendedVersion).length() <= constants::VALUE_2)
            {
                throw DbusException("Invalid extended version read for path [" +
                                    l_imagePath + "]");
            }

            // return first two character from image name.
            return (*l_imageExtendedVersion)
                .substr(constants::VALUE_0, constants::VALUE_2);
        }
        throw std::runtime_error("No Image found with required priority.");
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(l_ex.what());
        return std::string{};
    }
}

/**
 * @brief API to read DBus present property for the given inventory.
 *
 * @param[in] i_invObjPath - Inventory path.
 * @return Present property value, false in case of any error.
 */
inline bool isInventoryPresent(const std::string& i_invObjPath)
{
    if (i_invObjPath.empty())
    {
        return false;
    }

    const auto& l_retValue =
        dbusUtility::readDbusProperty(constants::pimServiceName, i_invObjPath,
                                      constants::inventoryItemInf, "Present");

    auto l_ptrPresence = std::get_if<bool>(&l_retValue);
    if (!l_ptrPresence)
    {
        return false;
    }

    return (*l_ptrPresence);
}

/**
 * @brief API to get list of sub tree paths for a given object path
 *
 * Given a DBus object path, this API returns a list of object paths under that
 * object path in the DBus tree. This API calls DBus method GetSubTreePaths
 * hosted by ObjectMapper DBus service.
 *
 * @param[in] i_objectPath - DBus object path.
 * @param[in] i_depth - The maximum subtree depth for which results should be
 * fetched. For unconstrained fetches use a depth of zero.
 * @param[in] i_constrainingInterfaces - An array of result set constraining
 * interfaces.
 *
 * @return On success, returns a std::vector<std::string> of object paths,
 * else returns an empty vector.
 *
 * Note: The caller of this API should check for empty vector.
 */
inline std::vector<std::string> GetSubTreePaths(
    const std::string i_objectPath, const int i_depth = 0,
    const std::vector<std::string>& i_constrainingInterfaces = {}) noexcept
{
    std::vector<std::string> l_objectPaths;
    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method = l_bus.new_method_call(
            constants::objectMapperService, constants::objectMapperPath,
            constants::objectMapperInf, "GetSubTreePaths");

        l_method.append(i_objectPath, i_depth, i_constrainingInterfaces);

        auto l_result = l_bus.call(l_method);
        l_result.read(l_objectPaths);
    }
    catch (const sdbusplus::exception::SdBusError& l_ex)
    {
        logging::logMessage(
            "Error while getting GetSubTreePaths for path [" + i_objectPath +
            "], error: " + std::string(l_ex.what()));
    }
    return l_objectPaths;
}

/**
 * @brief API to get Dbus service name for given connection identifier.
 *
 * @param[in] i_connectionId - Dbus connection ID.
 *
 * @return On success, returns the DBus service associated with given connection
 * ID, empty string otherwise.
 */
inline std::string getServiceNameFromConnectionId(
    const std::string& i_connectionId) noexcept
{
    try
    {
        if (i_connectionId.empty())
        {
            throw std::runtime_error("Empty connection ID");
        }

        auto l_bus = sdbusplus::bus::new_default();

        // get PID corresponding to the connection ID
        auto l_method = l_bus.new_method_call(
            "org.freedesktop.DBus", "/org/freedesktop/DBus",
            "org.freedesktop.DBus", "GetConnectionUnixProcessID");
        l_method.append(i_connectionId);
        auto l_result = l_bus.call(l_method);

        unsigned l_pid;
        l_result.read(l_pid);

        // use PID to get corresponding unit object path
        l_method = l_bus.new_method_call(
            "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager", "GetUnitByPID");
        l_method.append(l_pid);
        l_result = l_bus.call(l_method);

        sdbusplus::message::object_path l_unitObjectPath;
        l_result.read(l_unitObjectPath);

        // use unit object path to get service name
        l_method = l_bus.new_method_call(
            "org.freedesktop.systemd1", std::string(l_unitObjectPath).c_str(),
            "org.freedesktop.DBus.Properties", "Get");
        l_method.append("org.freedesktop.systemd1.Unit", "Id");
        l_result = l_bus.call(l_method);
        types::DbusVariantType l_serviceNameVar;
        l_result.read(l_serviceNameVar);

        if (auto l_serviceNameStr = std::get_if<std::string>(&l_serviceNameVar))
        {
            return *l_serviceNameStr;
        }
        else
        {
            throw std::runtime_error(
                "Invalid type received while reading service name.");
        }
    }
    catch (const std::exception& l_ex)
    {
        logging::logMessage(
            "Failed to get service name from connection ID: [" +
            i_connectionId + "]. error: " + std::string(l_ex.what()));
    }
    return std::string{};
}

} // namespace dbusUtility
} // namespace vpd
