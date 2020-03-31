#include "config.h"

#include "types.hpp"

#include <nlohmann/json.hpp>

class VpdTool
{
  private:
    const std::string fruPath;
    const std::string recordName;
    const std::string keyword;
    const std::string value;

  public:
    /**
     * @brief Dump the complete inventory in JSON format
     *
     * @param[in] jsObject - Inventory JSON specified in configure file.
     */
    void dumpInventory(const nlohmann::basic_json<>& jsObject);

    /**
     * @brief Dump the given inventory object in JSON format
     *
     * @param[in] jsObject - Inventory JSON specified in configure file.
     */
    void dumpObject(const nlohmann::basic_json<>& jsObject);

    /**
     * @brief Read keyword
     * Read the given object path, record name and keyword
     * from the inventory and display the value of the keyword
     * in JSON format.
     */
    void readKeyword();

    /**
     * @brief Update Keyword
     * Update the given keyword with the given value.
     *
     * @return return code (Success(0)/Failure(-1))
     */
    int updateKeyword();

    /**
     * @brief Force Reset
     * Clearing the inventory cache data and restarting the
     * phosphor inventory manager and also retriggering all the
     * udev events.
     */
    void forceReset(const nlohmann::basic_json<>& jsObject);

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for dumpInventory option and
     * forceReset option.
     */
    VpdTool()
    {
    }

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for dumpObject option.
     */
    VpdTool(const std::string&& fru) : fruPath(std::move(fru))
    {
    }

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for readKeyword option.
     */
    VpdTool(const std::string&& fru, const std::string&& recName,
            const std::string&& kw) :
        fruPath(std::move(fru)),
        recordName(std::move(recName)), keyword(std::move(kw))
    {
    }

    /**
     * @brief Constructor
     * Constructor is called during the
     * object instantiation for updateKeyword option.
     */

    VpdTool(const std::string&& fru, const std::string&& recName,
            const std::string&& kw, const std::string&& val) :
        fruPath(std::move(fru)),
        recordName(std::move(recName)), keyword(std::move(kw)),
        value(std::move(val))
    {
    }
};
