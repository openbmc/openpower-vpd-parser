#pragma once

#include "constants.hpp"
#include "error_codes.hpp"
#include "exceptions.hpp"
#include "logger.hpp"
#include "types.hpp"

#include <chrono>
#include <expected>
#include <format>

namespace vpd
{
/**
 * @brief The namespace defines utility methods for generic D-Bus operations.
 */
namespace dbusUtility
{

/**
 * @brief An API to get Map of service and interfaces for an object path.
 *
 * The API returns a Map of service name and interfaces for a given pair of
 * object path and interface list. It can be used to determine service name
 * which implements a particular object path and interface.
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
        Logger::getLoggerInstance()->logMessage(
            "Path value is empty, invalid call to GetObject");
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
    catch (const sdbusplus::exception::internal_exception& e)
    {
        // Logger::getLoggerInstance()->logMessage(e.what());
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
 * @param[in] service - Service name.
 * @param[in] objectPath - object path.
 * @param[in] interface - Interface, for the properties to be listed.
 *
 * @return - A map of property and value of an interface, if success.
 *           if failed, empty map.
 */
inline types::PropertyMap getPropertyMap(const std::string& service,
                                         const std::string& objectPath,
                                         const std::string& interface)
{
    types::PropertyMap propertyValueMap;
    if (service.empty() || objectPath.empty() || interface.empty())
    {
        Logger::getLoggerInstance()->logMessage(
            "Invalid parameters to get property map");
        return propertyValueMap;
    }

    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call(service.c_str(), objectPath.c_str(),
                                "org.freedesktop.DBus.Properties", "GetAll");
        method.append(interface);
        auto result = bus.call(method);
        result.read(propertyValueMap);
    }
    catch (const sdbusplus::exception::internal_exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(ex.what());
    }

    return propertyValueMap;
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
 * @param[in] objectPath - Path to search for an interface.
 * @param[in] depth - Maximum depth of the tree to search.
 * @param[in] interfaces - List of interfaces to search.
 *
 * @return - A map of object and its related services and interfaces, if
 *           success. If failed, empty map.
 */

inline types::MapperGetSubTree getObjectSubTree(
    const std::string& objectPath, const int& depth,
    const std::vector<std::string>& interfaces)
{
    types::MapperGetSubTree subTreeMap;

    if (objectPath.empty())
    {
        Logger::getLoggerInstance()->logMessage("Object path is empty.");
        return subTreeMap;
    }

    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(
            constants::objectMapperService, constants::objectMapperPath,
            constants::objectMapperInf, "GetSubTree");
        method.append(objectPath, depth, interfaces);
        auto result = bus.call(method);
        result.read(subTreeMap);
    }
    catch (const sdbusplus::exception::internal_exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(ex.what());
    }

    return subTreeMap;
}

/**
 * @brief API to get managed objects from D-Bus.
 *
 * The API calls GetManagedObjects dbus method on the ObjectManager interface
 * to retrieve all objects, their interfaces, and properties managed by
 * a service in a single D-Bus call.
 *
 * @param[in] serviceName - D-Bus service name.
 * @param[in] objectPath - Object path that implements ObjectManager.
 *
 * @return - std::expected containing either:
 *           - types::ObjectMap on success
 *           - error_code on failure (INVALID_INPUT_PARAMETER or DBUS_FAILURE)
 *
 * ToDo - Revisit in future to think and decide whether the approach can be made
 * generic , currently the scope is limited for the paths implementing
 * ObjectManager interface .
 */
inline std::expected<types::ObjectMap, error_code> getManagedObjects(
    const std::string& serviceName, const std::string& objectPath)
{
    if (serviceName.empty() || objectPath.empty())
    {
        return std::unexpected(error_code::INVALID_INPUT_PARAMETER);
    }

    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(
            serviceName.c_str(), objectPath.c_str(),
            "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");

        auto result = bus.call(method);
        types::ObjectMap managedObjects;
        result.read(managedObjects);
        return managedObjects;
    }
    catch (const sdbusplus::exception::internal_exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to get managed objects for service: {}, path: {}, "
            "error: {}",
            serviceName, objectPath, ex.what()));
        return std::unexpected(error_code::DBUS_FAILURE);
    }
}

/**
 * @brief An API to read property from Dbus.
 *
 * The caller of the API needs to validate the validatity and correctness of the
 * type and value of data returned. The API will just fetch and return the data
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
        Logger::getLoggerInstance()->logMessage(
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
    catch (const sdbusplus::exception::internal_exception& e)
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
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "DBus write failed, error: " + std::string(ex.what()));
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
        for (const auto& objectKeyValue : objectMap)
        {
            if (objectKeyValue.first.str.find(constants::pimPath, 0) !=
                std::string::npos)
            {
                auto nodeHandle = objectMap.extract(objectKeyValue.first);
                nodeHandle.key() = nodeHandle.key().str.replace(
                    0, std::strlen(constants::pimPath), "");
                objectMap.insert(std::move(nodeHandle));
            }
        }

        auto bus = sdbusplus::bus::new_default();
        auto pimMsg =
            bus.new_method_call(constants::pimServiceName, constants::pimPath,
                                constants::pimIntf, "Notify");
        pimMsg.append(std::move(objectMap));
        bus.call(pimMsg);
    }
    catch (const sdbusplus::exception::internal_exception& e)
    {
        return false;
    }
    return true;
}

/*
 * @brief API to update the VPD data on dbus.
 *
 * This method internally decides which method to call for dbus update
 * based on the configuration.
 * NOTE- In future, support else part to call other service/method.
 *
 * @param[in] objectMap - map of object and it's data
 *
 * @return bool - true if success, false otherwise.
 */
inline bool publishVpdOnDBus(types::ObjectMap&& objectMap)
{
    // Initialise it as default, otherwise compiler fails it assuming if
    // "if condition" doesn't match then it will be left uninitialised.
    // Once other service will be supported, it will be overwritten in
    // else section.
    [[maybe_unused]] bool (*dBusCall)(types::ObjectMap&&) = callPIM;

#if IBM_SYSTEM
    dBusCall = callPIM;
#endif

    auto reply = dBusCall(move(objectMap));

    return reply;
}

/**
 * @brief API to check if a D-Bus service is running or not.
 *
 * Any failure in calling the method "NameHasOwner" implies that the service is
 * not in a running state. Hence the API returns false in case of any exception
 * as well.
 *
 * @param[in] serviceName - D-Bus service name whose status is to be checked.
 * @return bool - True if the service is running, false otherwise.
 */
inline bool isServiceRunning(const std::string& serviceName)
{
    bool retVal = false;

    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method =
            bus.new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                "org.freedesktop.DBus", "NameHasOwner");
        method.append(serviceName);

        bus.call(method).read(retVal);
    }
    catch (const sdbusplus::exception::internal_exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Call to check service status failed with exception: " +
            std::string(ex.what()));
    }

    return retVal;
}

/**
 * @brief API to call "GetAttribute" method under BIOS manager.
 *
 * The API reads the given attribuute from BIOS and returns a tuple of both
 * current as well as pending value for that attribute.
 * The API return only the current attribute value if found.
 * API returns an empty variant of type BiosAttributeCurrentValue in case of any
 * error.
 *
 * @param[in] attributeName - Attribute to be read.
 * @return Tuple of PLDM attribute Type, current attribute value and pending
 * attribute value.
 */
inline types::BiosAttributeCurrentValue biosGetAttributeMethodCall(
    const std::string& attributeName) noexcept
{
    types::BiosGetAttrRetType attributeVal;
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(
            constants::biosConfigMgrService, constants::biosConfigMgrObjPath,
            constants::biosConfigMgrInterface, "GetAttribute");
        method.append(attributeName);

        auto result = bus.call(method);
        result.read(std::get<0>(attributeVal), std::get<1>(attributeVal),
                    std::get<2>(attributeVal));
    }
    catch (const sdbusplus::exception::internal_exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to read BIOS Attribute: " + attributeName +
            " due to error " + std::string(ex.what()));

        // TODO: Log an informational PEL here.
    }

    return std::get<1>(attributeVal);
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
    const auto hostState = dbusUtility::readDbusProperty(
        constants::hostService, constants::hostObjectPath,
        constants::hostInterface, "CurrentHostState");

    if (const auto currHostState = std::get_if<std::string>(&hostState))
    {
        if (*currHostState == constants::hostRunningState)
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
    const auto bmcState = dbusUtility::readDbusProperty(
        constants::bmcStateService, constants::bmcZeroStateObject,
        constants::bmcStateInterface, constants::currentBMCStateProperty);

    if (const auto currBMCState = std::get_if<std::string>(&bmcState))
    {
        if (*currBMCState == constants::bmcReadyState)
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
    int rc{constants::FAILURE};
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(
            constants::systemdService, constants::systemdObjectPath,
            constants::systemdManagerInterface, "StartUnit");
        method.append("reboot-guard-enable.service", "replace");
        bus.call_noreply(method);
        rc = constants::SUCCESS;
    }
    catch (const sdbusplus::exception::internal_exception& ex)
    {
        std::string errMsg =
            "D-Bus call to enable BMC reboot guard failed for reason: ";
        errMsg += ex.what();

        Logger::getLoggerInstance()->logMessage(errMsg);
    }
    return rc;
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
    int rc{constants::FAILURE};

    // A lambda which executes the DBus call to disable BMC reboot guard.
    auto executeDisableRebootGuard = []() -> int {
        int dBusCallRc{constants::FAILURE};
        try
        {
            auto bus = sdbusplus::bus::new_default();
            auto method = bus.new_method_call(
                constants::systemdService, constants::systemdObjectPath,
                constants::systemdManagerInterface, "StartUnit");
            method.append("reboot-guard-disable.service", "replace");
            bus.call_noreply(method);
            dBusCallRc = constants::SUCCESS;
        }
        catch (const sdbusplus::exception::internal_exception& ex)
        {}
        return dBusCallRc;
    };

    if (constants::FAILURE == executeDisableRebootGuard())
    {
        std::function<void()> retryDisableRebootGuard;

        // A lambda which tries to disable BMC reboot guard for 3 times at an
        // interval of 333 ms.
        retryDisableRebootGuard = [&]() {
            constexpr int maxRetries{3};
            static int numRetries{0};

            if (numRetries < maxRetries)
            {
                numRetries++;
                if (constants::FAILURE == executeDisableRebootGuard())
                {
                    // sleep for 333ms before next retry. This is just a random
                    // value so that 3 re-tries * 333ms takes ~1 second in the
                    // worst case.
                    const std::chrono::milliseconds sleepTime{333};
                    std::this_thread::sleep_for(sleepTime);
                    retryDisableRebootGuard();
                }
                else
                {
                    numRetries = 0;
                    rc = constants::SUCCESS;
                }
            }
            else
            {
                // Failed to Disable Reboot Guard even after 3 retries.
                Logger::getLoggerInstance()->logMessage(
                    "Failed to Disable Reboot Guard after " +
                    std::to_string(maxRetries) + " re-tries");
                numRetries = 0;
            }
        };

        retryDisableRebootGuard();
    }
    else
    {
        rc = constants::SUCCESS;
    }
    return rc;
}

/**
 * @brief API to notify FRU VPD Collection status.
 *
 * This API uses PIM's Notify method to update the given FRU VPD collection
 * status on D-bus.
 *
 * @param[in] inventoryPath - D-bus inventory path
 * @param[in] fruCollectionStatus - FRU VPD collection status.
 *
 * @return true if update succeeds, false otherwise.
 */
inline bool notifyFRUCollectionStatus(const std::string& inventoryPath,
                                      const std::string& fruCollectionStatus)
{
    types::ObjectMap objectMap;
    types::InterfaceMap interfaceMap;
    types::PropertyMap propertyMap;

    propertyMap.emplace("Status", fruCollectionStatus);
    interfaceMap.emplace(constants::vpdCollectionInterface, propertyMap);
    objectMap.emplace(inventoryPath, interfaceMap);

    // Call method to update the dbus
    if (!dbusUtility::publishVpdOnDBus(std::move(objectMap)))
    {
        return false;
    }

    return true;
}

/**
 * @brief API to get list of sub tree paths for a given object path
 *
 * Given a DBus object path, this API returns a list of object paths under that
 * object path in the DBus tree. This API calls DBus method GetSubTreePaths
 * hosted by ObjectMapper DBus service.
 *
 * @param[in] objectPath - DBus object path.
 * @param[in] depth - The maximum subtree depth for which results should be
 * fetched. For unconstrained fetches use a depth of zero.
 * @param[in] constrainingInterfaces - An array of result set constraining
 * interfaces.
 *
 * @return On success, returns a std::vector<std::string> of object paths,
 * else returns an empty vector.
 *
 * Note: The caller of this API should check for empty vector.
 */
inline std::vector<std::string> GetSubTreePaths(
    const std::string objectPath, const int depth = 0,
    const std::vector<std::string>& constrainingInterfaces = {}) noexcept
{
    std::vector<std::string> objectPaths;
    try
    {
        auto bus = sdbusplus::bus::new_default();
        auto method = bus.new_method_call(
            constants::objectMapperService, constants::objectMapperPath,
            constants::objectMapperInf, "GetSubTreePaths");

        method.append(objectPath, depth, constrainingInterfaces);

        auto result = bus.call(method);
        result.read(objectPaths);
    }
    catch (const sdbusplus::exception::internal_exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Error while getting GetSubTreePaths for path [" + objectPath +
            "], error: " + std::string(ex.what()));
    }
    return objectPaths;
}

/**
 * @brief API to get Dbus service name for given connection identifier.
 *
 * @param[in] connectionId - Dbus connection ID.
 *
 * @return On success, returns the DBus service associated with given connection
 * ID, empty string otherwise.
 */
inline std::string getServiceNameFromConnectionId(
    const std::string& connectionId) noexcept
{
    try
    {
        if (connectionId.empty())
        {
            throw std::runtime_error("Empty connection ID");
        }

        auto bus = sdbusplus::bus::new_default();

        // get PID corresponding to the connection ID
        auto method = bus.new_method_call(
            "org.freedesktop.DBus", "/org/freedesktop/DBus",
            "org.freedesktop.DBus", "GetConnectionUnixProcessID");
        method.append(connectionId);
        auto result = bus.call(method);

        auto pid = result.unpack<unsigned>();

        // use PID to get corresponding unit object path
        method = bus.new_method_call(
            "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager", "GetUnitByPID");
        method.append(pid);
        result = bus.call(method);

        auto unitObjectPath = result.unpack<sdbusplus::object_path>();

        // use unit object path to get service name
        method = bus.new_method_call("org.freedesktop.systemd1",
                                     std::string(unitObjectPath).c_str(),
                                     "org.freedesktop.DBus.Properties", "Get");
        method.append("org.freedesktop.systemd1.Unit", "Id");
        result = bus.call(method);
        auto serviceNameVar = result.unpack<types::DbusVariantType>();

        if (auto serviceNameStr = std::get_if<std::string>(&serviceNameVar))
        {
            return *serviceNameStr;
        }
        else
        {
            throw std::runtime_error(
                "Invalid type received while reading service name.");
        }
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(
            "Failed to get service name from connection ID: [" + connectionId +
            "]. error: " + std::string(ex.what()));
    }
    return std::string{};
}

/**
 * @brief API to fetch BMC inventory paths from PIM.
 *
 * The API performs a two-step lookup to identify BMC inventory paths:
 * 1.Calls GetSubTree on PIM to retrieve all object paths that implement
 *   the "xyz.openbmc_project.Common.PhysicalContext" interface.
 * 2.Filters the result to retain only those paths whose "Type" property
 *   under PhysicalContext interface is "Manager" This positively identifies all
 *   BMC inventory paths hosted by inventory manager service on D-Bus.
 *
 * @return On success, returns a types::ListOfPaths of BMC inventory
 * object paths. Returns std::unexpected(error_code::DBUS_FAILURE) if
 * the mapper call throws, or
 * std::unexpected(error_code::INVALID_VALUE_READ_FROM_DBUS) if the subtree
 * result is empty (no Common.PhysicalContext objects found under PIM).
 */
inline std::expected<types::ListOfPaths, error_code>
    getBMCInventoryPaths() noexcept
{
    try
    {
        const auto objectPaths = GetSubTreePaths(
            constants::pimPath, 0,
            std::vector<std::string>{constants::physicalContextInterface});

        if (objectPaths.empty())
        {
            return std::unexpected(error_code::INVALID_VALUE_READ_FROM_DBUS);
        }

        types::ListOfPaths bmcPaths;

        for (const auto& objectPath : objectPaths)
        {
            const auto typeVariant =
                readDbusProperty(constants::pimServiceName, objectPath,
                                 constants::physicalContextInterface,
                                 constants::physicalContextTypeProperty);

            const auto* typeStr = std::get_if<std::string>(&typeVariant);

            if (typeStr != nullptr &&
                *typeStr == constants::physicalContextManagerValue)
            {
                bmcPaths.emplace_back(objectPath);
            }
        }

        return bmcPaths;
    }
    catch (const std::exception& ex)
    {
        Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to get BMC inventory paths. Error: {}", ex.what()));
        return std::unexpected(error_code::DBUS_FAILURE);
    }
}

} // namespace dbusUtility
} // namespace vpd
