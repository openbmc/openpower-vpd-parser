#pragma once

#include "constants.hpp"
#include "logger.hpp"

/**
 * @brief Class to manage FRU VPD collection trigger and check for collection
 * status
 *
 * This class is used for handling FRU VPD collection trigger. This contains
 * methods to trigger FRU VPD collection and check FRU VPD collection status.
 */
class FruVpdCollectionManager
{
  public:
    /*
     * Deleted methods
     */
    FruVpdCollectionManager() = delete;
    FruVpdCollectionManager(const FruVpdCollectionManager&) = delete;
    FruVpdCollectionManager operator=(const FruVpdCollectionManager&) = delete;
    FruVpdCollectionManager(const FruVpdCollectionManager&&) = delete;
    FruVpdCollectionManager operator=(const FruVpdCollectionManager&&) = delete;

    /**
     * @brief Parameterized Constructor
     *
     * @param[in] i_collectionServiceName - service which does FRU VPD
     * collection
     * @param[in] i_objectPath - object path which hosts FRU VPD collection API
     * manager primary data location
     * @param[in] i_interface - interface for FRU VPD collection API
     * @param[in] i_method - D-Bus method for triggering FRU VPD collection
     * @param[in] i_retryLimit - Collection Status check re-try limit
     * @param[in] i_sleepDurationInSeconds - Sleep time in seconds between each
     * re-try
     */
    explicit FruVpdCollectionManager(
        const std::string_view i_collectionServiceName,
        const std::string_view i_objectPath, const std::string_view i_interface,
        const std::string_view i_method, unsigned i_retryLimit,
        unsigned i_sleepDurationInSeconds) :
        m_collectionServiceName{i_collectionServiceName},
        m_objectPath{i_objectPath}, m_interface{i_interface},
        m_collectionMethodName{i_method},
        m_collectionStatusRetryLimit{i_retryLimit},
        m_collectionStatusSleepTimeSecs{i_sleepDurationInSeconds},
        m_logger{vpd::Logger::getLoggerInstance()}
    {}

    /**
     * @brief API to trigger FRU VPD collection and check it's status
     *
     * This API triggers VPD collection for all FRUs by calling D-Bus API
     * "CollectAllFRUVPD" exposed by vpd-manager. After triggering the
     * collection it checks for the collection status property.
     *
     *
     * @return - returns 0 if VPD collection has been triggered and
     * "CollectionStatus" property has been read as "Completed", otherwise
     * returns 1.
     */
    int triggerFruVpdCollectionAndCheckStatus() noexcept;

  private:
    /**
     * @brief API to trigger VPD collection for all FRUs.
     *
     * This API triggers VPD collection for all FRUs by calling Dbus API
     * "CollectAllFRUVPD" exposed by vpd-manager
     *
     * @return - On success returns true, otherwise returns false
     */
    inline bool collectAllFruVpd() noexcept;

    /**
     * @brief API to check for VPD collection status
     *
     * This API checks for VPD manager collection status by reading the
     * collection "Status" property exposed by vpd-manager on Dbus. The read
     * logic uses a retry loop with a specific number of retries with specific
     * sleep time between each retry.
     *
     * @return If "CollectionStatus" property is "Completed", returns 0,
     * otherwise returns 1.
     */
    int checkVpdCollectionStatus() noexcept;

    // service which does FRU VPD collection
    std::string m_collectionServiceName;

    // object path for FRU VPD collection API
    std::string m_objectPath;

    // interface for FRU VPD collection API
    std::string m_interface;

    // D-Bus method name for triggering FRU VPD collection
    std::string m_collectionMethodName;

    // Collection Status check re-try limit
    unsigned m_collectionStatusRetryLimit;

    // Sleep duration in seconds between each collection status check re-try
    unsigned m_collectionStatusSleepTimeSecs;

    // logger instance
    std::shared_ptr<vpd::Logger> m_logger{nullptr};
};
