#include "config.h"

#include "collection_orchestrator.hpp"
#include "constants.hpp"
#include "inventory_backup_handler.hpp"
#include "logger.hpp"
#include "prime_inventory.hpp"
#include "types.hpp"
#include "utility/common_utility.hpp"
#include "utility/dbus_utility.hpp"
#include "utility/event_logger_utility.hpp"

#include <CLI/CLI.hpp>
#include <xyz/openbmc_project/State/Decorator/PowerState/common.hpp>

#include <chrono>
#include <thread>

constexpr auto active = "active";
constexpr auto passive = "passive";

/**
 * @brief API to handle inventory backup data
 *
 * This API handles inventory backup data. It checks if there is any inventory
 * backup data and restores it if so. It also restarts the inventory manager
 * service so that the restored data is reflected on D-Bus, and then clears the
 * backup data.
 *
 * @return true if inventory backup data is found and restored successfully, and
 * inventory manager service is successfully restarted. It returns false if
 * there is any error encountered while restoring backup data or restarting
 * inventory manager service
 *
 * @throw std::runtime_error
 */
bool checkAndHandleInventoryBackup()
{
    bool l_rc{false};
    uint16_t l_errCode{0};
    auto l_logger = vpd::Logger::getLoggerInstance();

    InventoryBackupHandler l_inventoryBackupHandler{
        vpd::constants::pimServiceName, vpd::constants::pimPrimaryPath,
        vpd::constants::pimBackupPath};

    const auto l_restoreInventoryStartTime = std::chrono::steady_clock::now();
    if (!l_inventoryBackupHandler.restoreInventoryBackupData(l_errCode))
    {
        if (l_errCode)
        {
            l_logger->logMessage(
                "Failed to restore inventory backup data. Error: " +
                vpd::commonUtility::getErrCodeMsg(l_errCode));
        }
        return l_rc;
    }

    l_logger->logMessage(
        "Time taken to restore inventory backup data: " +
        std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - l_restoreInventoryStartTime)
                .count()) +
        "ms");

    // restart the inventory manager service so that the new inventory
    // data is reflected on D-Bus
    if (l_inventoryBackupHandler.restartInventoryManagerService(l_errCode))
    {
        // clear the backup inventory data
        l_inventoryBackupHandler.clearInventoryBackupData(l_errCode);

        // inventory backup restoration and service restart are
        // successful, so return success from here as FRU VPD collection
        // is not needed
        l_rc = true;
    }
    else
    {
        // Restarting the inventory manager service has failed,
        // and error code indicates inventory manager service is not running,
        // throw an exception so that wait-vpd-parsers service also fails.
        // if error code indicates inventory service is running, return false to
        // indicate to caller to proceed for FRU VPD collection.
        if (l_errCode == vpd::error_code::SERVICE_NOT_RUNNING)
        {
            throw std::runtime_error(
                "Failed to restart inventory manager service after restoring backup inventory data. Failing this service");
        }
    }
    return l_rc;
}

/**
 * @brief API to delete VPD for all FRUs.
 *
 * This API deletes VPD for all FRUs by calling Dbus API
 * "DeleteAllFRUVPD" exposed by vpd-manager
 *
 * @return - On success returns true, otherwise returns false
 */
inline int deleteAllFruVpd() noexcept
{
    bool l_rc{true};
    try
    {
        auto l_bus = sdbusplus::bus::new_default();
        auto l_method =
            l_bus.new_method_call(BUSNAME, OBJPATH, IFACE, "DeleteAllFRUVPD");

        l_bus.call_noreply(l_method);
    }
    catch (const std::exception& l_ex)
    {
        auto l_logger = vpd::Logger::getLoggerInstance();
        l_logger->logMessage("Failed to trigger delete all FRU VPD. Error: " +
                             std::string(l_ex.what()));
        l_rc = false;
    }
    return l_rc;
}

/**
 * @brief API to handle chassis poweron situation
 *
 * This API handles the functionality of this service when chassis is powered on
 * at BMC boot.
 *
 * @return - On success returns 0, otherwise returns 1
 */
int handleChassisPowerOn()
{
    vpd::Logger::getLoggerInstance()->logMessage(
        "Chassis is powered on at BMC boot. Skipping FRUs VPD collection and setting FRUs VPD collection status as complete.");

    // chassis is powered on at BMC boot. In order to optimize performance,
    // avoid FRUs VPD collection and simply update collection status property as
    // complete so that dependent services can proceed normally.
    if (!vpd::dbusUtility::writeDbusProperty(
            BUSNAME, OBJPATH, vpd::constants::vpdCollectionInterface, "Status",
            vpd::constants::vpdCollectionCompleted))
    {
        vpd::Logger::getLoggerInstance()->logMessage(
            std::format(
                "Failed to mark FRUs VPD collection as complete. Exiting wait-vpd-parser service with failure."),
            vpd::PlaceHolder::PEL,
            vpd::types::PelInfoTuple{vpd::types::ErrorType::DbusFailure,
                                     vpd::types::SeverityType::Warning, 0,
                                     std::nullopt, std::nullopt, std::nullopt,
                                     std::nullopt, std::nullopt});

        return vpd::constants::VALUE_1;
    }

    return vpd::constants::VALUE_0;
}

/**
 *  @brief API to handle BMC ReadyToRemove property
 *
 *  This API handles ReadyToRemove interface property for BMC. ReadyToRemove
 * property is used by Concurrent Maintenance flow to identify whether a FRU is
 * ready to be replaced. On redundant BMC systems, only Passive BMC is
 * concurrently maintenable and hence only Passive BMC should have the
 * ReadyToRemove property.
 *
 * Note: This API assumes that in "passive" role, this API is called after
 * deleteAllFruVpd(), so that inventory on PIM is already cleared and this API
 * only needs to publish "ReadyToRemove" on current BMC(passive)'s inventory
 * path.
 *
 *  @param[in] i_role - BMC role, which can be "active" or "passive"
 *
 *  @return - On success returns 0, otherwise returns 1
 */
int handleBmcReadyToRemove(
    [[maybe_unused]] const std::string_view i_role) noexcept
{
    int l_retVal{vpd::constants::VALUE_1};
    try
    {
        /*  @todo
            - read the BMC position published by IBM HE app on D-Bus
                - use the BMC position to determine the current BMC's inventory
           path
            - if the role parameter is "passive"
                - publish "ReadyToRemove" property as false under interface
           xyz.openbmc_project.State.ReadyToRemove under current BMC's inventory
           path
                - the "ReadyToRemove" interface on sibling(active) BMC should's
           inventory path is assumed to be cleared as part of deleteAllFruVpd()
                which is called before this method
            - if the role parameter is "active"
                - delete interface
           xyz.openbmc_project.State.ReadyToRemove under current BMC's inventory
           path
           - publish "ReadyToRemove" property as false under interface
           xyz.openbmc_project.State.ReadyToRemove under sibling(Passive) BMC's
           inventory path
        */
        l_retVal = vpd::constants::VALUE_0;
    }
    catch (const std::exception& l_ex)
    {
        vpd::Logger::getLoggerInstance()->logMessage(std::format(
            "Failed to handle ReadyToRemove property on {} BMC. Error: {}",
            i_role, l_ex.what()));
    }
    return l_retVal;
}

int main(int argc, char** argv)
{
    try
    {
        CLI::App l_app{"Wait VPD parser app"};

        // default collection status timeout in seconds
        unsigned l_collectionStatusTimeoutSecs{1200};

        // The BMC role can be active or passive. By default the BMC is
        // considered as active.
        std::string l_role{active};

        l_app.add_option("--collectionStatusTimeout, -s",
                         l_collectionStatusTimeoutSecs,
                         "VPD collection status timeout");
        l_app.add_option("--role, -b", l_role, "BMC role");

        CLI11_PARSE(l_app, argc, argv);

        if (l_role == passive)
        {
            const bool l_deleteFruVpdSuccess = deleteAllFruVpd();

            if (!handleBmcReadyToRemove(l_role))
            {
                vpd::Logger::getLoggerInstance()->logMessage(
                    "Failed to handle BMC ReadyToRemove property on Passive BMC");
            }

            return !vpd::dbusUtility::writeDbusProperty(
                BUSNAME, OBJPATH, vpd::constants::vpdCollectionInterface,
                "Status",
                l_deleteFruVpdSuccess ? vpd::constants::vpdCollectionCompleted
                                      : vpd::constants::vpdCollectionFailed);
        }

        if (!handleBmcReadyToRemove(l_role))
        {
            vpd::Logger::getLoggerInstance()->logMessage(
                "Failed to handle BMC ReadyToRemove property on Active BMC");
        }

        // Read the chassis PowerState property set by pgood-chassis-check.
        // On any failure (property missing, type mismatch, D-Bus error) log
        // and fall through to normal active flow (full VPD collection).
        using PowerStateIface = sdbusplus::common::xyz::openbmc_project::state::
            decorator::PowerState;

        const auto l_powerStateVar = vpd::dbusUtility::readDbusProperty(
            vpd::constants::pimServiceName, vpd::constants::systemInvPath,
            PowerStateIface::interface,
            PowerStateIface::property_names::power_state);

        if (const auto l_stateStr = std::get_if<std::string>(&l_powerStateVar))
        {
            const auto l_state =
                PowerStateIface::convertStringToState(*l_stateStr);

            if (l_state == PowerStateIface::State::On)
            {
                return handleChassisPowerOn();
            }
        }
        else
        {
            vpd::Logger::getLoggerInstance()->logMessage(
                "Failed to read chassis PowerState from D-Bus. "
                "Proceeding with full VPD collection.");
        }

        // check and see if there is any inventory backup data. If it's there
        // restore the data.
        if (checkAndHandleInventoryBackup())
        {
            // backup data is found and restored successfully, return success
            // here as we don't need to go for FRU VPD collection.
            return vpd::constants::VALUE_0;
        }

        PrimeInventory l_primeObj;
        l_primeObj.primeSystemBlueprint();

        // trigger FRU VPD collection and check for status
        CollectionOrchestrator l_collectionOrchestrator{
            BUSNAME, OBJPATH, IFACE, "CollectAllFRUVPD",
            l_collectionStatusTimeoutSecs};

        l_collectionOrchestrator.triggerFruVpdCollectionAndCheckStatus();

        return vpd::constants::VALUE_0;
    }
    catch (const std::exception& l_ex)
    {
        const auto l_logger = vpd::Logger::getLoggerInstance();
        l_logger->logMessage(
            std::format("Exiting from wait-vpd-parser, reason: {}",
                        l_ex.what()),
            vpd::PlaceHolder::PEL,
            vpd::types::PelInfoTuple{vpd::EventLogger::getErrorType(l_ex),
                                     vpd::types::SeverityType::Warning, 0,
                                     std::nullopt, std::nullopt, std::nullopt,
                                     std::nullopt, std::nullopt});
        return vpd::constants::VALUE_1;
    }
}
