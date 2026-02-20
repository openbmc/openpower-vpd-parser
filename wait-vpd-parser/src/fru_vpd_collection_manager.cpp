#include "config.h"

#include "fru_vpd_collection_manager.hpp"

#include "utility/dbus_utility.hpp"

int FruVpdCollectionManager::triggerFruVpdCollectionAndCheckStatus() noexcept
{
    int l_rc{vpd::constants::VALUE_1};
    try
    {
        // do D-Bus call to trigger FRU VPD collection
        collectAllFruVpd();

        // start checking for VPD collection status
        l_rc = checkVpdCollectionStatus();

        // run the event loop
        m_ioContext.run();
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to trigger all FRU VPD collection. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}

void FruVpdCollectionManager::collectAllFruVpd() noexcept
{
    try
    {
        // setup connection to D-Bus
        auto l_connection =
            std::make_shared<sdbusplus::asio::connection>(m_ioContext);

        auto l_message = l_connection->new_method_call(
            m_collectionServiceName.c_str(), m_objectPath.c_str(),
            m_interface.c_str(), m_collectionMethodName.c_str());

        constexpr uint64_t l_collectionStatusTimeOutUs{
            1 * 60 * 1000000}; // 1 min
                               // timeout

        l_connection->async_send(
            l_message,
            [this](boost::system::error_code l_ec,
                   sdbusplus::message_t& l_ret) {
                auto m_logger = vpd::Logger::getLoggerInstance();
                m_logger->logMessage("_SR async_send callback");

                if (l_ec == boost::system::errc::timed_out)
                {
                    m_logger->logMessage(
                        "wait-vpd-parsers timed out trying to call \"CollectAllFRUVPD\"");
                }
                else if (l_ec || l_ret.is_method_error())
                {
                    m_logger->logMessage(
                        "Error with \"CollectAllFRUVPD\" async_send" +
                        l_ec.message());
                }
                else
                {
                    bool l_rc{false};

                    // D-Bus call has returned
                    l_ret.read(l_rc);

                    m_logger->logMessage(
                        "CollectAllFRUVPD " +
                        std::string(l_rc ? "successful" : "failed"));
                }

                // stop the event loop
                m_ioContext.stop();
            },
            l_collectionStatusTimeOutUs);
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to trigger all FRU VPD collection. Error: " +
            std::string(l_ex.what()));
    }
}

int FruVpdCollectionManager::checkVpdCollectionStatus() noexcept
{
    try
    {
        m_logger->logMessage("Checking every " +
                             std::to_string(m_collectionStatusSleepTimeSecs) +
                             "s for VPD collection status ....");

        for (unsigned l_retries = m_collectionStatusRetryLimit;
             l_retries != vpd::constants::VALUE_0; --l_retries)
        {
            // check at specified time interval
            std::this_thread::sleep_for(
                std::chrono::seconds(m_collectionStatusSleepTimeSecs));

            const auto l_propValue = vpd::dbusUtility::readDbusProperty(
                BUSNAME, OBJPATH, vpd::constants::vpdCollectionInterface,
                "Status");

            if (auto l_val = std::get_if<std::string>(&l_propValue))
            {
                if (*l_val == vpd::constants::vpdCollectionCompleted)
                {
                    m_logger->logMessage("VPD collection is completed");
                    return vpd::constants::VALUE_0;
                }
            }

            m_logger->logMessage(
                "Waiting for VPD status update. Retries remaining: " +
                std::to_string(l_retries));
        }

        m_logger->logMessage(
            "Exit wait for VPD services to finish with timeout");
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage("Error while checking VPD collection status: " +
                             std::string(l_ex.what()));
    }

    return vpd::constants::VALUE_1;
}
