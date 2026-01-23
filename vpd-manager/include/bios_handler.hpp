#pragma once
#include "logger.hpp"
#include "manager.hpp"
#include "types.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>

namespace vpd
{

/**
 * @brief Interface class for BIOS handling.
 *
 * The class layout has the virtual methods required to be implemented by any
 * concrete class that intends to use the feature provided via BIOS handler
 * class.
 */
class BiosHandlerInterface
{
  public:
    /**
     * @brief API to back up or restore BIOS attributes.
     *
     * Concrete class should implement the API and read the backed up data from
     * its designated location and take a call if it should be backed up or
     * restored.
     */
    virtual void backUpOrRestoreBiosAttributes() = 0;

    /**
     * @brief Callback API to be triggered on BIOS attribute change.
     *
     * Concrete class should implement the API to extract the attribute and its
     * value from DBus message broadcasted on BIOS attribute change.
     * The definition should be overridden in concrete class to deal with BIOS
     * attributes interested in.
     *
     * @param[in] i_msg - The callback message.
     */
    virtual void biosAttributesCallback(sdbusplus::message_t& i_msg) = 0;
};

/**
 * @brief IBM specifc BIOS handler class.
 */
class IbmBiosHandler : public BiosHandlerInterface
{
  public:
    /**
     * @brief Construct a new IBM BIOS Handler object
     *
     * This constructor constructs a new IBM BIOS Handler object
     * @param[in] i_manager - Manager object.
     */
    explicit IbmBiosHandler(const std::shared_ptr<Manager>& i_manager);

    /**
     * @brief API to back up or restore BIOS attributes.
     *
     * The API will read the backed up data from the VPD keyword and based on
     * its value, either backs up or restores the data.
     */
    virtual void backUpOrRestoreBiosAttributes();

    /**
     * @brief Callback API to be triggered on BIOS attribute change.
     *
     * The API to extract the required attribute and its value from DBus message
     * broadcasted on BIOS attribute change.
     *
     * @param[in] i_msg - The callback message.
     */
    virtual void biosAttributesCallback(sdbusplus::message_t& i_msg);

  private:
    /**
     * @brief API to read given attribute from BIOS table.
     *
     * @param[in] attributeName - Attribute to be read.
     * @return - Bios attribute current value.
     */
    types::BiosAttributeCurrentValue readBiosAttribute(
        const std::string& attributeName);

    /**
     * @brief API to process "hb_field_core_override" attribute.
     *
     * The API checks value stored in VPD. If found default then the BIOS value
     * is saved to VPD else VPD value is restored in BIOS pending attribute
     * table.
     *
     * @param[in] i_attributeData - JSON object containing the VPD record and
     * keyword mapping for FCO attribute.
     *
     */
    void processFieldCoreOverride(const nlohmann::json& i_attributeData);

    /**
     * @brief API to save FCO data into VPD.
     *
     * @param[in] i_fcoInBios - FCO value.
     * @param[in] i_attributeData - JSON object containing the VPD record and
     * keyword mapping for FCO attribute.
     */
    void saveFcoToVpd(int64_t i_fcoInBios,
                      const nlohmann::json& i_attributeData);

    /**
     * @brief API to save given value to "hb_field_core_override" attribute.
     *
     * @param[in] i_fcoVal - FCO value.
     */
    void saveFcoToBios(const types::BinaryVector& i_fcoVal);

    /**
     * @brief API to save AMM data into VPD.
     *
     * @param[in] i_memoryMirrorMode - Memory mirror mode value.
     */
    void saveAmmToVpd(const std::string& i_memoryMirrorMode);

    /**
     * @brief API to save given value to "hb_memory_mirror_mode" attribute.
     *
     * @param[in] i_ammVal - AMM value.
     */
    void saveAmmToBios(const uint8_t& i_ammVal);

    /**
     * @brief API to process "hb_memory_mirror_mode" attribute.
     *
     * The API checks value stored in VPD. If found default then the BIOS value
     * is saved to VPD else VPD value is restored in BIOS pending attribute
     * table.
     *
     * @param[in] i_attributeData - JSON object containing the VPD record and
     * keyword mapping for "hb_memory_mirror_mode" attribute.
     *
     */
    void processActiveMemoryMirror(const nlohmann::json& i_attributeData);

    /**
     * @brief API to process "pvm_create_default_lpar" attribute.
     *
     * The API reads the value from VPD and restore it to the BIOS attribute
     * in BIOS pending attribute table.
     *
     * @param[in] i_attributeData - JSON object containing the VPD record and
     * keyword mapping for "pvm_create_default_lpar" attribute.
     */
    void processCreateDefaultLpar(const nlohmann::json& i_attributeData);

    /**
     * @brief API to save given value to "pvm_create_default_lpar" attribute.
     *
     * @param[in] i_createDefaultLparVal - Value to be saved;
     */
    void saveCreateDefaultLparToBios(const std::string& i_createDefaultLparVal);

    /**
     * @brief API to save given value to VPD.
     *
     * @param[in] i_createDefaultLparVal - Value to be saved.
     *
     */
    void saveCreateDefaultLparToVpd(const std::string& i_createDefaultLparVal);

    /**
     * @brief API to process "pvm_clear_nvram" attribute.
     *
     * The API reads the value from VPD and restores it to the BIOS pending
     * attribute table.
     *
     * @param[in] i_attributeData - JSON object containing the VPD record and
     * keyword mapping for "pvm_clear_nvram" attribute.
     *
     */
    void processClearNvram(const nlohmann::json& i_attributeData);

    /**
     * @brief API to save given value to "pvm_clear_nvram" attribute.
     *
     * @param[in] i_clearNvramVal - Value to be saved.
     */
    void saveClearNvramToBios(const std::string& i_clearNvramVal);

    /**
     * @brief API to save given value to VPD.
     *
     * @param[in] i_clearNvramVal - Value to be saved.
     */
    void saveClearNvramToVpd(const std::string& i_clearNvramVal);

    /**
     * @brief API to process "pvm_keep_and_clear" attribute.
     *
     * The API reads the value from VPD and restore it to the BIOS pending
     * attribute table.
     *
     * @param[in] i_attributeData - JSON object containing the VPD record and
     * keyword mapping for "pvm_keep_and_clear" attribute.
     *
     */
    void processKeepAndClear(const nlohmann::json& i_attributeData);

    /**
     * @brief API to save given value to "pvm_keep_and_clear" attribute.
     *
     * @param[in] i_KeepAndClearVal - Value to be saved.
     */
    void saveKeepAndClearToBios(const std::string& i_KeepAndClearVal);

    /**
     * @brief API to save given value to VPD.
     *
     * @param[in] i_KeepAndClearVal - Value to be saved.
     */
    void saveKeepAndClearToVpd(const std::string& i_KeepAndClearVal);

    // const reference to shared pointer to Manager object.
    const std::shared_ptr<Manager>& m_manager;

    // Shared pointer to Logger object
    std::shared_ptr<Logger> m_logger;

    // Bios config json object
    nlohmann::json m_biosConfigJson{};
};

/**
 * @brief A class to operate upon BIOS attributes.
 *
 * The class along with specific BIOS handler class(es), provides a feature
 * where specific BIOS attributes identified by the concrete specific class can
 * be listened for any change and can be backed up to a desired location or
 * restored back to the BIOS table.
 *
 * To use the feature, "BiosHandlerInterface" should be implemented by a
 * concrete class and the same should be used to instantiate BiosHandler.
 *
 * This class registers call back to listen to PLDM service as it is being used
 * for reading/writing BIOS attributes.
 *
 * The feature can be used in a factory reset scenario where backed up values
 * can be used to restore BIOS.
 *
 */
template <typename T>
class BiosHandler
{
  public:
    // deleted APIs
    BiosHandler() = delete;
    BiosHandler(const BiosHandler&) = delete;
    BiosHandler& operator=(const BiosHandler&) = delete;
    BiosHandler& operator=(BiosHandler&&) = delete;
    ~BiosHandler() = default;

    /**
     * @brief Constructor.
     *
     * @param[in] i_connection - Asio connection object.
     * @param[in] i_manager - Manager object.
     */
    BiosHandler(
        const std::shared_ptr<sdbusplus::asio::connection>& i_connection,
        const std::shared_ptr<Manager>& i_manager) : m_asioConn(i_connection)
    {
        try
        {
            m_specificBiosHandler = std::make_shared<T>(i_manager);
            checkAndListenPldmService();
        }
        catch (std::exception& l_ex)
        {
            // catch any exception here itself and don't pass it to main as it
            // will mark the service failed. Since VPD-Manager is a critical
            // service, failing it can push BMC to quiesced state whic is not
            // required in this case.
            std::string l_errMsg = "Instantiation of BIOS Handler failed. { ";
            l_errMsg += l_ex.what() + std::string(" }");
            EventLogger::createSyncPel(
                types::ErrorType::FirmwareError, types::SeverityType::Warning,
                __FILE__, __FUNCTION__, 0, l_errMsg, std::nullopt, std::nullopt,
                std::nullopt, std::nullopt);
        }
    }

  private:
    /**
     * @brief API to check if PLDM service is running and run BIOS sync.
     *
     * This API checks if the PLDM service is running and if yes it will start
     * an immediate sync of BIOS attributes. If the service is not running, it
     * registers a listener to be notified when the service starts so that a
     * restore can be performed.
     */
    void checkAndListenPldmService();

    /**
     * @brief Register listener for BIOS attribute property change.
     *
     * The VPD manager needs to listen for property change of certain BIOS
     * attributes that are backed in VPD. When the attributes change, the new
     * value is written back to the VPD keywords that backs them up.
     */
    void listenBiosAttributes();

    // Reference to the connection.
    const std::shared_ptr<sdbusplus::asio::connection>& m_asioConn;

    // shared pointer to specific BIOS handler.
    std::shared_ptr<T> m_specificBiosHandler;
};
} // namespace vpd
