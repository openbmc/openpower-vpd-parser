#include "config_manager.hpp"
#include "exceptions.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <memory>

using namespace vpd;

// Mock Manager class to create ConfigManager instances for testing
class MockManager
{
  public:
    // Helper method to create ConfigManager with given JSON file
    static std::unique_ptr<ConfigManager> createConfigManager(
        const std::string& jsonPath)
    {
        // Create the passkey (only Manager can do this)
        ConfigManager::ManagerPassKey key;
        
        // Create and return ConfigManager instance
        return std::make_unique<ConfigManager>(key, jsonPath);
    }
};

// Test fixture for ConfigManager validation tests
class ConfigManagerValidationTest : public ::testing::Test
{
  protected:
    std::string testJsonPath;

    void SetUp() override
    {
        // Create a temporary path for test JSON files
        testJsonPath = "/tmp/test_config_manager_validation.json";
    }

    void TearDown() override
    {
        // Clean up test JSON file
        if (std::filesystem::exists(testJsonPath))
        {
            std::filesystem::remove(testJsonPath);
        }
    }

    // Helper to write JSON to file
    void writeJsonToFile(const nlohmann::json& json)
    {
        std::ofstream file(testJsonPath);
        file << json.dump(4);
        file.close();
    }

    // Helper to create a valid minimal config JSON
    nlohmann::json createValidConfigJson()
    {
        return nlohmann::json{
            {"frus",
             {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
               nlohmann::json::array(
                   {{{"inventoryPath",
                      "/xyz/openbmc_project/inventory/system/chassis/"
                      "motherboard"},
                     {"serviceName", "xyz.openbmc_project.Inventory.Manager"},
                     {"extraInterfaces",
                      {{"xyz.openbmc_project.Inventory.Item.Board.Motherboard",
                        nullptr}}}}})}}}}};
    }

    // Helper to attempt ConfigManager creation and expect success
    void expectConfigManagerCreationSuccess(const nlohmann::json& json)
    {
        writeJsonToFile(json);
        EXPECT_NO_THROW({
            auto configMgr = MockManager::createConfigManager(testJsonPath);
            EXPECT_NE(configMgr, nullptr);
        });
    }

    // Helper to attempt ConfigManager creation and expect JsonException
    void expectConfigManagerCreationFailure(const nlohmann::json& json,
                                           const std::string& expectedErrorSubstring)
    {
        writeJsonToFile(json);
        EXPECT_THROW(
            {
                try
                {
                    auto configMgr = MockManager::createConfigManager(testJsonPath);
                }
                catch (const JsonException& e)
                {
                    EXPECT_NE(std::string(e.what()).find(expectedErrorSubstring),
                              std::string::npos)
                        << "Expected error message to contain: " << expectedErrorSubstring
                        << "\nActual error message: " << e.what();
                    throw;
                }
            },
            JsonException);
    }
};

// ============================================================================
// validateConfigJson Tests - Testing through ConfigManager constructor
// ============================================================================

TEST_F(ConfigManagerValidationTest, ValidConfigJson_Success)
{
    auto json = createValidConfigJson();
    expectConfigManagerCreationSuccess(json);
}

TEST_F(ConfigManagerValidationTest, MissingFrusSection_ThrowsException)
{
    nlohmann::json json = {{"someOtherField", "value"}};
    expectConfigManagerCreationFailure(json, "Missing required 'frus' section");
}

TEST_F(ConfigManagerValidationTest, FrusNotObject_ThrowsException)
{
    nlohmann::json json = {{"frus", "not an object"}};
    expectConfigManagerCreationFailure(json, "'frus' section must be an object");
}

TEST_F(ConfigManagerValidationTest, FrusEmpty_ThrowsException)
{
    nlohmann::json json = {{"frus", nlohmann::json::object()}};
    expectConfigManagerCreationFailure(json, "'frus' section cannot be empty");
}

TEST_F(ConfigManagerValidationTest, FruNotArray_ThrowsException)
{
    nlohmann::json json = {
        {"frus", {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom", "not array"}}}};
    expectConfigManagerCreationFailure(json, "must be an array");
}

TEST_F(ConfigManagerValidationTest, FruArrayEmpty_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array()}}}};
    expectConfigManagerCreationFailure(json, "cannot be empty");
}

TEST_F(ConfigManagerValidationTest, SubFruNotObject_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array({"not an object"})}}}};
    expectConfigManagerCreationFailure(json, "must be an object");
}

// ============================================================================
// Mandatory Fields Tests
// ============================================================================

TEST_F(ConfigManagerValidationTest, MissingInventoryPath_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"serviceName", "xyz.openbmc_project.Inventory.Manager"}}})}}}};
    expectConfigManagerCreationFailure(json, "inventoryPath");
}

TEST_F(ConfigManagerValidationTest, MissingServiceName_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/"
                  "motherboard"}}})}}}};
    expectConfigManagerCreationFailure(json, "serviceName");
}

TEST_F(ConfigManagerValidationTest, InventoryPathNotString_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath", 123},
                 {"serviceName", "xyz.openbmc_project.Inventory.Manager"}}})}}}};
    expectConfigManagerCreationFailure(json, "must be a string");
}

TEST_F(ConfigManagerValidationTest, ServiceNameNotString_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/"
                  "motherboard"},
                 {"serviceName", true}}})}}}};
    expectConfigManagerCreationFailure(json, "must be a string");
}

// ============================================================================
// Optional Fields Tests
// ============================================================================

TEST_F(ConfigManagerValidationTest, ValidOptionalFields_Success)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/"
                  "motherboard"},
                 {"serviceName", "xyz.openbmc_project.Inventory.Manager"},
                 {"extraInterfaces",
                  {{"xyz.openbmc_project.Inventory.Item.Board.Motherboard",
                    nullptr}}},
                 {"isSystemVpd", true},
                 {"replaceableAtRuntime", false},
                 {"copyRecords", nlohmann::json::array({"VSYS"})},
                 {"offset", 0},
                 {"size", 65504}}})}}}};
    expectConfigManagerCreationSuccess(json);
}

TEST_F(ConfigManagerValidationTest, ExtraInterfacesNotObject_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/"
                  "motherboard"},
                 {"serviceName", "xyz.openbmc_project.Inventory.Manager"},
                 {"extraInterfaces", "not an object"}}})}}}};
    expectConfigManagerCreationFailure(json, "extraInterfaces");
}

TEST_F(ConfigManagerValidationTest, BooleanFieldNotBoolean_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/"
                  "motherboard"},
                 {"serviceName", "xyz.openbmc_project.Inventory.Manager"},
                 {"isSystemVpd", "not a boolean"}}})}}}};
    expectConfigManagerCreationFailure(json, "must be a boolean");
}

TEST_F(ConfigManagerValidationTest, CopyRecordsNotArray_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/"
                  "motherboard"},
                 {"serviceName", "xyz.openbmc_project.Inventory.Manager"},
                 {"copyRecords", "not an array"}}})}}}};
    expectConfigManagerCreationFailure(json, "must be an array");
}

TEST_F(ConfigManagerValidationTest, IntegerFieldNotInteger_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/"
                  "motherboard"},
                 {"serviceName", "xyz.openbmc_project.Inventory.Manager"},
                 {"offset", "not an integer"}}})}}}};
    expectConfigManagerCreationFailure(json, "must be an integer");
}

TEST_F(ConfigManagerValidationTest, PreActionNotObject_ThrowsException)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/"
                  "motherboard"},
                 {"serviceName", "xyz.openbmc_project.Inventory.Manager"},
                 {"preAction", "not an object"}}})}}}};
    expectConfigManagerCreationFailure(json, "preAction");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ConfigManagerValidationTest, CompleteValidJson_Success)
{
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/motherboard"},
                 {"serviceName", "xyz.openbmc_project.Inventory.Manager"},
                 {"isSystemVpd", true},
                 {"extraInterfaces",
                  {{"xyz.openbmc_project.Inventory.Item.Board.Motherboard",
                    nullptr},
                   {"xyz.openbmc_project.Inventory.Decorator.LocationCode",
                    {{"LocationCode", "Ufcs-N00-P0"}}}}}}})},
          {"/sys/bus/i2c/drivers/at24/7-0051/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/motherboard/"
                  "dcm0"},
                 {"serviceName", "xyz.openbmc_project.Inventory.Manager"},
                 {"replaceableAtRuntime", true},
                 {"pollingRequired", true},
                 {"preAction",
                  {{"collection", {{"gpioPresence", {{"pin", "GPIO1"}}}}}}},
                 {"postAction",
                  {{"collection", {{"systemCmd", {{"cmd", "echo done"}}}}}}},
                 {"extraInterfaces",
                  {{"xyz.openbmc_project.Inventory.Item.Cpu", nullptr}}}}})}}}}};

    expectConfigManagerCreationSuccess(json);
}

TEST_F(ConfigManagerValidationTest, MultipleErrors_ThrowsFirstError)
{
    // JSON with multiple errors - should throw on first error encountered
    nlohmann::json json = {
        {"frus",
         {{"/sys/bus/i2c/drivers/at24/8-0050/eeprom",
           nlohmann::json::array(
               {{{"inventoryPath",
                  "/xyz/openbmc_project/inventory/system/chassis/motherboard"},
                 // Missing serviceName
                 {"isSystemVpd", "not a boolean"}, // Wrong type
                 {"extraInterfaces", "not an object"} // Wrong type
                 }})}}}};

    // Should fail on missing serviceName (first mandatory field check)
    expectConfigManagerCreationFailure(json, "serviceName");
}

// Made with Bob
