#include "fru_vpd_collection_manager.hpp"

int FruVpdCollectionManager::triggerFruVpdCollectionAndCheckStatus() noexcept
{
    int l_rc{vpd::constants::VALUE_1};
    try
    {}
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to trigger all FRU VPD collection. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}

inline bool FruVpdCollectionManager::collectAllFruVpd() noexcept
{
    bool l_rc{false};
    try
    {}
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage(
            "Failed to trigger all FRU VPD collection. Error: " +
            std::string(l_ex.what()));
    }
    return l_rc;
}

int FruVpdCollectionManager::checkVpdCollectionStatus() noexcept
{
    int l_rc{vpd::constants::VALUE_1};
    try
    {}
    catch (const std::exception& l_ex)
    {
        m_logger->logMessage("Error while checking VPD collection status: " +
                             std::string(l_ex.what()));
    }
    return l_rc;
}
