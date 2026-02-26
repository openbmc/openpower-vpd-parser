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
class CollectionOrchestrator final
{
  public:
    /*
     * Deleted methods
     */
    CollectionOrchestrator() = delete;
    CollectionOrchestrator(const CollectionOrchestrator&) = delete;
    CollectionOrchestrator operator=(const CollectionOrchestrator&) = delete;
    CollectionOrchestrator(const CollectionOrchestrator&&) = delete;
    CollectionOrchestrator operator=(const CollectionOrchestrator&&) = delete;

    /**
     * @brief Parameterized Constructor
     *
     * @param[in] i_collectionServiceName - service which does FRU VPD
     * collection
     * @param[in] i_objectPath - object path which hosts FRU VPD collection API
     * @param[in] i_interface - interface for FRU VPD collection API
     * @param[in] i_method - D-Bus method for triggering FRU VPD collection
     * @param[in] i_collectionStatusTimeout - Timeout in seconds for collection
     * status to be completed
     *
     * @throw std::bad_alloc
     */
    explicit CollectionOrchestrator(
        const std::string_view i_collectionServiceName,
        const std::string_view i_objectPath, const std::string_view i_interface,
        const std::string_view i_method,
        unsigned i_collectionStatusTimeout = 360) :
        m_collectionServiceName{i_collectionServiceName},
        m_objectPath{i_objectPath}, m_interface{i_interface},
        m_collectionMethodName{i_method}, m_timeout{i_collectionStatusTimeout},
        m_logger{vpd::Logger::getLoggerInstance()}
    {
        m_asioConn = std::make_shared<sdbusplus::asio::connection>(m_ioContext);
    }

    /**
     * @brief Destructor
     *
     *  Flushes pending D-Bus operations and releases the connection
     */
    ~CollectionOrchestrator()
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
     * m_collectionMethodName . After triggering the
     * collection it checks for the collection status property.
     *
     *
     * @throw std::runtime_error
     */
    void triggerFruVpdCollectionAndCheckStatus();

  private:
    /**
     * @brief API to register a listener on VPD collection status property
     *
     * @throw std::runtime_error
     */
    void registerVpdCollectionStatusListener();

    /**
     * @brief Callback API to process collect all FRU VPD D-Bus call status
     *
     * @param[in] i_msg - Callback message
     * @param[in] i_errorCode - error code
     *
     * @throw std::runtime_error
     */
    void collectAllFruVpdCallback(const boost::system::error_code i_ec,
                                  sdbusplus::message_t& i_msg);

    /**
     * @brief Callback API to process VPD collection status
     *
     * @param[in] i_msg - Callback message
     *
     * @throw std::runtime_error
     */
    void vpdCollectionStatusCallback(sdbusplus::message_t& i_msg);

    /**
     * @brief API to read the VPD collection status property from D-Bus
     *
     * This API reads VPD collection status property from D-Bus. If collection
     * status is read as done, it updates the collection done flag
     *
     * @throw std::runtime_error
     */
    void readCollectionStatusProperty();

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

    // collection status timeout
    std::chrono::seconds m_timeout{360};

    // flag which indicates whether collection status has been read as complete
    bool m_collectionDone{false};

    // match object required by Collection Status listener
    std::unique_ptr<sdbusplus::bus::match_t> m_collectionStatusMatch{nullptr};

    // logger instance
    std::shared_ptr<vpd::Logger> m_logger{nullptr};
};
