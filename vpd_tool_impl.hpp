#include "config.h"

#include "types.hpp"

#include <nlohmann/json.hpp>

class VpdTool
{
  private:
    const std::string fruPath;

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
     * @brief A Constructor
     *
     * Constructor is called during the
     * object instantiation for dumpInventory option.
     */
    VpdTool()
    {
    }

    /**
     * @brief Move Constructor
     *
     * Move constructor is called during the
     * object instantiation for dumpObject option.
     */
    VpdTool(const std::string&& fru) : fruPath(std::move(fru))
    {
    }
};
