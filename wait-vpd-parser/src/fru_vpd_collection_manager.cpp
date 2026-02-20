#include "config.h"

#include "fru_vpd_collection_manager.hpp"

#include "utility/dbus_utility.hpp"

int FruVpdCollectionManager::triggerFruVpdCollectionAndCheckStatus() noexcept
{
    try
    {
        return collectAllFruVpd() ? checkVpdCollectionStatus()
                                  : vpd::constants::VALUE_1;
    }
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to trigger all FRU VPD collection. Error: " +
            std::string(l_ex.what()));
    }
}

inline bool FruVpdCollectionManager::collectAllFruVpd() noexcept
{
    bool l_rc{true};
    try
    {
        // setup connection to dbus
        boost::asio::io_context l_ioContext;
        auto l_connection =
            std::make_shared<sdbusplus::asio::connection>(l_ioContext);

        auto l_message = l_connection->new_method_call(
            m_collectionServiceName.c_str(), m_objectPath.c_str(),
            m_interface.c_str(), m_collectionMethodName.c_str());

        constexpr uint64_t l_collectionStatusTimeOutUs{
            1 * 60 * 1000000}; // 1 min
                               // timeout

        l_connection->async_send(
            l_message,
            [](boost::system::error_code l_ec, sdbusplus::message_t& l_ret) {
                auto m_logger = vpd::Logger::getLoggerInstance();
                m_logger->logMessage("_SR async_send callback");

                if (l_ec == boost::system::errc::timed_out)
                {
                    m_logger->logMessage(
                        "wait-vpd-parsers timed out trying to call \"CollectAllFRUVPD\"");
                    exit(EXIT_FAILURE);
                }
                else if (l_ec || l_ret.is_method_error())
                {
                    m_logger->logMessage(
                        "Error with \"CollectAllFRUVPD\" async_send" +
                        l_ec.message());
                    return;
                }

                bool l_retVal{false};
                l_ret.read(l_retVal);
                m_logger->logMessage(
                    "CollectAllFRUVPD " +
                    std::string(l_retVal ? "successful" : "failed"));
            },
            l_collectionStatusTimeOutUs);

        // setup a D-bus listener on "CollectionStatus" property

        l_ioContext.run();
    }
    catch (const std::exception& l_ex)
    {
        auto m_logger = vpd::Logger::getLoggerInstance();
        m_logger->logMessage(
            "Failed to trigger all FRU VPD collection. Error: " +
            std::string(l_ex.what()));
        l_rc = false;
    }
    return l_rc;
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
