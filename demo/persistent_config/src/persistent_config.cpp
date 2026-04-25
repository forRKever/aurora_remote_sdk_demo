/*
 * Copyright 2013-2025 SLAMTEC Co., Ltd.
 * Aurora Remote SDK - Persistent Config Demo
 *
 * This demo shows how to use the PersistentConfigManager to enumerate,
 * get, set, and reset device configuration entries.
 */

#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>

using namespace rp::standalone::aurora;

// ANSI color codes for terminal output
#ifdef _WIN32
    #define COLOR_RESET   ""
    #define COLOR_RED     ""
    #define COLOR_GREEN   ""
    #define COLOR_YELLOW  ""
    #define COLOR_CYAN    ""
    #define COLOR_BOLD    ""
#else
    #define COLOR_RESET   "\033[0m"
    #define COLOR_RED     "\033[31m"
    #define COLOR_GREEN   "\033[32m"
    #define COLOR_YELLOW  "\033[33m"
    #define COLOR_CYAN    "\033[36m"
    #define COLOR_BOLD    "\033[1m"
#endif

void printError(const std::string& msg) {
    std::cerr << COLOR_RED << "Error: " << COLOR_RESET << msg << "\n";
}

void printSuccess(const std::string& msg) {
    std::cout << COLOR_GREEN << "Success: " << COLOR_RESET << msg << "\n";
}

void printInfo(const std::string& msg) {
    std::cout << COLOR_CYAN << "Info: " << COLOR_RESET << msg << "\n";
}

void printWarning(const std::string& msg) {
    std::cout << COLOR_YELLOW << "Warning: " << COLOR_RESET << msg << "\n";
}

void printUsage(const char* progName) {
    std::cout << COLOR_BOLD << "Usage: " << COLOR_RESET << progName << " <server_address> <command> [args...]\n\n";
    std::cout << COLOR_BOLD << "Commands:\n" << COLOR_RESET;
    std::cout << "  " << COLOR_GREEN << "enum" << COLOR_RESET << "                                    - List all registered config entries\n";
    std::cout << "  " << COLOR_GREEN << "get" << COLOR_RESET << " <filter_path> [-o <file>]          - Get config (optionally save to file)\n";
    std::cout << "  " << COLOR_GREEN << "set" << COLOR_RESET << " <filter_path> <key> <json|file>    - Set config with key\n";
    std::cout << "  " << COLOR_GREEN << "reset" << COLOR_RESET << " <filter_path>                    - Reset config to default\n";
    std::cout << "  " << COLOR_GREEN << "reset-all" << COLOR_RESET << "                              - Reset all configs to default\n";
    std::cout << "\n" << COLOR_BOLD << "Examples:\n" << COLOR_RESET;
    std::cout << "  " << progName << " 192.168.11.1 enum\n";
    std::cout << "  " << progName << " 192.168.11.1 get recorder.dashcam\n";
    std::cout << "  " << progName << " 192.168.11.1 get recorder.dashcam -o config.json\n";
    std::cout << "  " << progName << " 192.168.11.1 set recorder.dashcam @overwrite '{\"enabled\":true}'\n";
    std::cout << "  " << progName << " 192.168.11.1 set recorder.dashcam @overwrite config.json\n";
    std::cout << "  " << progName << " 192.168.11.1 reset recorder.dashcam\n";
    std::cout << "  " << progName << " 192.168.11.1 reset-all\n";
}

bool fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

bool readFileToString(const std::string& filePath, std::string& outString) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    outString = buffer.str();
    return true;
}

bool writeStringToFile(const std::string& filePath, const std::string& data) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    file << data;
    return true;
}

int main(int argc, const char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    const char* serverAddr = argv[1];
    const char* command = argv[2];

    printInfo(std::string("Connecting to Aurora server at ") + serverAddr + "...");

    // Create SDK session
    auto sdk = std::shared_ptr<RemoteSDK>(RemoteSDK::CreateSession());
    if (!sdk) {
        printError("Failed to create SDK session");
        return 1;
    }

    // Prepare connection descriptor
    SDKServerConnectionDesc srvConn;
    srvConn.push_back(SDKConnectionInfo(serverAddr));

    // Connect to server
    slamtec_aurora_sdk_errorcode_t errcode;
    if (!sdk->connect(srvConn, &errcode)) {
        printError("Failed to connect to server. Error code: " + std::to_string(errcode));
        return 1;
    }

    if (!sdk->isConnected()) {
        printError("Connection failed verification");
        return 1;
    }

    printSuccess("Connected to server");

    try {
        std::string cmd(command);

        if (cmd == "enum") {
            // List all config entries
            printInfo("Enumerating all registered config entries...");

            auto entries = sdk->persistentConfig.enumAllEntries(
                20000, &errcode);

            if (errcode != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                printError("Failed to enumerate entries. Error code: " + std::to_string(errcode));
                return 1;
            }

            if (entries.empty()) {
                printInfo("No config entries registered");
            } else {
                std::cout << "\n" << COLOR_BOLD << "Registered Config Entries (" << entries.size() << "):" << COLOR_RESET << "\n";
                std::cout << "----------------------------------------\n";
                for (size_t i = 0; i < entries.size(); i++) {
                    std::cout << "  " << (i + 1) << ". " << COLOR_CYAN << entries[i] << COLOR_RESET << "\n";
                }
                std::cout << "\n";
            }

        } else if (cmd == "get") {
            if (argc < 4) {
                printError("Missing filter path");
                printUsage(argv[0]);
                return 1;
            }

            std::string filterPath = argv[3];
            std::string outputFile;

            // Check for optional -o flag
            if (argc >= 6 && std::string(argv[4]) == "-o") {
                outputFile = argv[5];
            }

            printInfo("Getting config for: " + filterPath);

            auto configJson = sdk->persistentConfig.getConfig(filterPath,
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode);

            if (errcode != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                printError("Failed to get config. Error code: " + std::to_string(errcode));
                return 1;
            }

            if (configJson.empty()) {
                printWarning("Config is empty");
            } else {
                if (!outputFile.empty()) {
                    if (writeStringToFile(outputFile, configJson)) {
                        printSuccess("Config saved to: " + outputFile);
                    } else {
                        printError("Failed to write to file: " + outputFile);
                        return 1;
                    }
                } else {
                    std::cout << "\n" << COLOR_BOLD << "Config JSON:" << COLOR_RESET << "\n";
                    std::cout << "----------------------------------------\n";
                    std::cout << configJson << "\n";
                    std::cout << "----------------------------------------\n\n";
                }
            }

        } else if (cmd == "set") {
            if (argc < 6) {
                printError("Missing arguments for set command");
                printUsage(argv[0]);
                return 1;
            }

            std::string filterPath = argv[3];
            std::string key = argv[4];
            std::string configInput = argv[5];

            printInfo("Setting config for: " + filterPath);
            printInfo("Key: " + key);

            // Check if input is a file or JSON string
            std::string configJson;
            if (fileExists(configInput)) {
                printInfo("Loading config from file: " + configInput);
                if (!readFileToString(configInput, configJson)) {
                    printError("Failed to read file: " + configInput);
                    return 1;
                }
            } else {
                configJson = configInput;
            }

            if (!sdk->persistentConfig.setConfig(filterPath, key, configJson,
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to set config. Error code: " + std::to_string(errcode));
                return 1;
            }

            printSuccess("Config updated successfully");

        } else if (cmd == "reset") {
            if (argc < 4) {
                printError("Missing filter path");
                printUsage(argv[0]);
                return 1;
            }

            std::string filterPath = argv[3];

            printInfo("Resetting config for: " + filterPath);

            if (!sdk->persistentConfig.resetConfig(filterPath,
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to reset config. Error code: " + std::to_string(errcode));
                return 1;
            }

            printSuccess("Config reset to default successfully");

        } else if (cmd == "reset-all") {
            printWarning("This will reset ALL configs to default. Are you sure? (y/N): ");

            std::string confirm;
            std::getline(std::cin, confirm);

            if (confirm != "y" && confirm != "Y") {
                printInfo("Operation cancelled");
                return 0;
            }

            printInfo("Resetting all configs...");

            if (!sdk->persistentConfig.resetAllConfig(
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to reset all configs. Error code: " + std::to_string(errcode));
                return 1;
            }

            printSuccess("All configs reset to default successfully");

        } else {
            printError("Unknown command: " + cmd);
            printUsage(argv[0]);
            return 1;
        }

        // Cleanup
        printInfo("Disconnecting...");
        sdk->controller.disconnect();

        printSuccess("Done!");
        return 0;

    } catch (const std::exception& e) {
        printError(std::string("Exception: ") + e.what());
    }

    return 1;
}
