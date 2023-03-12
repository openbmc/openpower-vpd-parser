#include "logger.hpp"
#include "parser.hpp"
#include "parser_interface.hpp"
#include "types.hpp"
#include "worker.hpp"

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include <parser_factory.hpp>

#include <filesystem>
#include <iostream>

/**
 * @brief This file implements a generic parser APP.
 *
 * It recieves path of the VPD file(mandatory) and path to a config
 * file(optional) as arguments. It will parse the data and return parsed data in
 * a required format.
 *
 * Steps to get parsed VPD.
 * - Pass VPD file path and config file (if applicable).
 * - Read VPD file to vector.
 * - Pass that to parser_factory to get the parser and call parse API on that
 * parser object to get the Parsed VPD map.
 * - If VPD format is other than the existing formats. Follow the steps
 * - a) Add logic in parser_factory.cpp, vpdTypeCheck API to detect the format.
 * - b) Implement a custom parser class.
 * - c) Override parse API in the newly added parser class.
 * - d) Add type of parsed data returned by parse API into types.hpp,
 * "VPDMapVariant".
 *
 */

int main(int argc, char** argv)
{
    try
    {
        std::string vpdFilePath{};
        CLI::App app{"VPD-parser-app - APP to parse VPD. "};

        app.add_option("-f, --file", vpdFilePath, "VPD file path")->required();

        std::string configFilePath{};

        app.add_option("-c,--config", configFilePath, "Path to JSON config");

        CLI11_PARSE(app, argc, argv);

        vpd::logging::logMessage("VPD file path recieved" + vpdFilePath);

        // VPD file path is a mandatory parameter to execute any parser.
        if (vpdFilePath.empty())
        {
            throw std::runtime_error("Empty VPD file path");
        }

        nlohmann::json json;
        vpd::types::VPDMapVariant parsedVpdDataMap;

        // Below are two different ways of parsing the VPD.
        if (!configFilePath.empty())
        {
            vpd::logging::logMessage("Processing with config file - " +
                                     configFilePath);

            std::shared_ptr<vpd::Worker> objWorker =
                std::make_shared<vpd::Worker>(configFilePath);
            parsedVpdDataMap = objWorker->parseVpdFile(vpdFilePath);

            // Based on requirement, call appropriate public API of worker class
            /*If required to publish the FRU data on Dbus*/
            // objWorker->publishFruDataOnDbus(parsedVpdDataMap);
        }
        else
        {
            // Will work with empty JSON
            std::shared_ptr<vpd::Parser> vpdParser =
                std::make_shared<vpd::Parser>(vpdFilePath, json);
            parsedVpdDataMap = vpdParser->parse();
        }

        // If custom handling is required then custom logic to be implemented
        // based on the type of variant,
        //  eg: for IPZ VPD format
        if (auto ipzVpdMap =
                std::get_if<vpd::types::IPZVpdMap>(&parsedVpdDataMap))
        {
            // get rid of unused variable warning/error
            (void)ipzVpdMap;
            // implement code that needs to handle parsed IPZ VPD.
        }
    }
    catch (const std::exception& ex)
    {
        vpd::logging::logMessage(ex.what());
        return -1;
    }

    return 0;
}
