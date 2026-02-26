#pragma once

#include "constants.hpp"
#include "logger.hpp"

/**
 * @brief Class to manage FRU VPD collection trigger and check for
 * collection status
 *
 * This class is used for handling FRU VPD collection trigger. This contains
 * methods to trigger FRU VPD collection and check FRU VPD collection
 * status.
 */
class FruVpdCollectionManager final
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
     * @param[in] i_collectionStatusTimeout - Timeout in seconds for collection
     * status to be completed
     *
     * @throw std::bad_alloc
     */
    explicit FruVpdCollectionManager(
        const std::string_view i_collectionServiceName,
        const std::string_view i_objectPath, const std::string_view i_interface,
        const std::string_view i_method,
        unsigned i_collectionStatusTimeout = 360) :
        m_collectionServiceName{i_collectionServiceName},
        m_objectPath{i_objectPath}, m_interface{i_interface},
        m_collectionMethodName{i_method},
        m_collectionStatusTimeout{i_collectionStatusTimeout},
        m_logger{vpd::Logger::getLoggerInstance()}
    {
        m_asioConn = std::make_shared<sdbusplus::asio::connection>(m_ioContext);
    }

    /**
     * @brief Destructor
     *
     *  Flushes pending D-Bus operations and releases the connection
     */
    ~FruVpdCollectionManager()
    {
        if (m_asioConn)
        {
            m_asioConn->flush();
        }
    }

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
     *
     * @throw DbusException
     */
    int triggerFruVpdCollectionAndCheckStatus();

  private:
    /**
     * @brief API to register a listener on VPD collection status property
     */
    void registerVpdCollectionStatusListener() noexcept;

    /**
     * @brief Callback API to process collect all FRU VPD D-Bus call status
     *
     * @param[in] i_msg - Callback message
     * @param[in] i_errorCode - error code
     */
    void collectAllFruVpdCallback(boost::system::error_code i_ec,
                                  sdbusplus::message_t& i_msg) noexcept;

    /**
     * @brief Callback API to process VPD collection status
     *
     * @param[in] i_msg - Callback message
     */
    void vpdCollectionStatusCallback(sdbusplus::message_t& i_msg) noexcept;

    /**
     * @brief API to handle collection status timer expiry
     *
     * @param[in] i_errorCode - error code
     *
     */
    void handleTimerExpiry(
        const boost::system::error_code& i_errorCode) noexcept;

    /**
     * @brief State enum to track collection progress
     */
    enum class State : int
    {
        CollectionCompleted, // Collection completed successfully
        Idle,                // Initial state, no operation started
        CollectionTriggered, // D-Bus call for collection has been triggered
        CollectionTriggerTimeout, // D-Bus call for collection trigger has timed
                                  // out
        Listening,                // Listening for collection status,
        Failed                    // Collection failed or internal error
    };

    // service which does FRU VPD collection
    std::string m_collectionServiceName;

    // object path for FRU VPD collection API
    std::string m_objectPath;

    // interface for FRU VPD collection API
    std::string m_interface;

    // D-Bus method name for triggering FRU VPD collection
    std::string m_collectionMethodName;

    // boost I/O context
    boost::asio::io_context m_ioContext;

    // shared pointer to D-Bus connection object
    std::shared_ptr<sdbusplus::asio::connection> m_asioConn;

    // timer to check if FRU VPD collection is done within a specified time
    std::shared_ptr<boost::asio::steady_timer> m_collectionTimer;

    // collection status timeout
    std::chrono::seconds m_collectionStatusTimeout{360};

    // logger instance
    std::shared_ptr<vpd::Logger> m_logger{nullptr};

    // current state of the collection process
    State m_state{State::Idle};
};
