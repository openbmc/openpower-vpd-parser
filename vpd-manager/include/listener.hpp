#pragma once

#include "worker.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <memory>

namespace vpd
{
/**
 * @brief Class to listen on events
 *
 * This class will be used for registering and handling events on the system.
 */
class Listener
{
  public:
    /**
     * Deleted methods for Listener
     */
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener& operator=(Listener&&) = delete;
    Listener(Listener&&) = delete;

    /**
     * @brief Constructor
     * @param[in] i_worker - Reference to worker class object.
     * @param[in] i_asioConnection - Dbus Connection.
     */
    Listener(
        const std::shared_ptr<Worker>& i_worker,
        const std::shared_ptr<sdbusplus::asio::connection>& i_asioConnection);

    /**
     * @brief API to register callback for Host state change.
     *
     */
    void registerHostStateChangeCallback() const noexcept;

    /**
     * @brief API to register callback for "AssetTag" property change.
     */
    void registerAssetTagChangeCallback() const noexcept;

    /**
     * @brief API to register "Present" property change callback
     *
     * This API registers "Present" property change callback for FRUs for
     * which "monitorPresence" is true in system config JSON.
     */
    void registerPresenceChangeCallback() noexcept;

  private:
    /**
     * @brief API to process host state change callback.
     *
     * @param[in] i_msg - Callback message.
     */
    void hostStateChangeCallBack(sdbusplus::message_t& i_msg) const noexcept;

    /**
     * @brief Callback API to be triggered on "AssetTag" property change.
     *
     * @param[in] i_msg - Callback message.
     */
    void assetTagChangeCallback(sdbusplus::message_t& i_msg) const noexcept;

    /**
     * @brief Callback API to be triggered on "Present" property change.
     *
     * @param[in] i_msg - Callback message.
     */
    void presentPropertyChangeCallback(
        sdbusplus::message_t& i_msg) const noexcept;

    // Shared pointer to worker class
    const std::shared_ptr<Worker>& m_worker;

    // Shared pointer to bus connection.
    const std::shared_ptr<sdbusplus::asio::connection>& m_asioConnection;
};
} // namespace vpd
