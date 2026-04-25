/*
 * Copyright 2013-2025 SLAMTEC Co., Ltd.
 * Aurora Remote SDK - System Power Management Demo
 *
 * This demo shows how to use the requestPowerOperation API to control
 * Aurora device power operations such as reboot and shutdown.
 */

#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <chrono>
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
    std::cout << COLOR_BOLD << "Aurora System Power Management Demo\n" << COLOR_RESET;
    std::cout << "====================================\n\n";
    std::cout << COLOR_BOLD << "Usage: " << COLOR_RESET << progName << " <server_address> <command>\n\n";
    std::cout << COLOR_BOLD << "Commands:\n" << COLOR_RESET;
    std::cout << "  " << COLOR_GREEN << "reboot" << COLOR_RESET << "     - Reboot the Aurora device\n";
    std::cout << "  " << COLOR_GREEN << "shutdown" << COLOR_RESET << "   - Shutdown the Aurora device\n";
    std::cout << "  " << COLOR_GREEN << "status" << COLOR_RESET << "     - Show device status before power operation\n";
    std::cout << "\n" << COLOR_BOLD << "Examples:\n" << COLOR_RESET;
    std::cout << "  " << progName << " 192.168.11.1 status\n";
    std::cout << "  " << progName << " 192.168.11.1 reboot\n";
    std::cout << "  " << progName << " 192.168.11.1 shutdown\n";
    std::cout << "\n" << COLOR_YELLOW << "Warning: " << COLOR_RESET;
    std::cout << "Reboot and shutdown operations will immediately affect the device.\n";
    std::cout << "Make sure to save any important data before executing these commands.\n";
}

void printDeviceInfo(RemoteSDK* sdk) {
    std::cout << "\n" << COLOR_BOLD << "Device Information\n" << COLOR_RESET;
    std::cout << "==================\n";

    // Wait a moment for device info to be retrieved
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Get device basic info
    RemoteDeviceBasicInfo deviceInfo;
    uint64_t timestamp_ns;
    if (sdk->dataProvider.getLastDeviceBasicInfo(deviceInfo, timestamp_ns)) {
        std::cout << "  Device Name:      " << deviceInfo.device_name << "\n";
        std::cout << "  Serial Number:    " << deviceInfo.getDeviceSerialNumberString() << "\n";
        std::cout << "  Device Model:     " << deviceInfo.getDeviceModelString() << "\n";
        std::cout << "  Firmware Version: " << deviceInfo.firmware_version_string << "\n";
        std::cout << "  Build Date:       " << deviceInfo.firmware_build_date << " " << deviceInfo.firmware_build_time << "\n";
    } else {
        printWarning("Could not retrieve device basic info (device may still be initializing)");
    }

    std::cout << "\n";
}

bool confirmOperation(const std::string& operation) {
    std::cout << COLOR_YELLOW << "\nWarning: " << COLOR_RESET;
    std::cout << "You are about to " << COLOR_BOLD << operation << COLOR_RESET << " the Aurora device.\n";
    std::cout << "This operation cannot be undone.\n\n";
    std::cout << "Type '" << COLOR_GREEN << "yes" << COLOR_RESET << "' to confirm: ";

    std::string input;
    std::getline(std::cin, input);

    return (input == "yes" || input == "YES" || input == "Yes");
}

int main(int argc, const char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    const char* server_address = argv[1];
    std::string command = argv[2];

    // Convert command to lowercase
    for (auto& c : command) {
        c = std::tolower(c);
    }

    // Validate command
    if (command != "reboot" && command != "shutdown" && command != "status") {
        printError("Unknown command: " + command);
        printUsage(argv[0]);
        return 1;
    }

    std::cout << COLOR_BOLD << "Aurora System Power Management Demo\n" << COLOR_RESET;
    std::cout << "====================================\n";

    // Get SDK version
    slamtec_aurora_sdk_version_info_t versionInfo;
    if (RemoteSDK::GetSDKInfo(versionInfo)) {
        std::cout << "SDK Version: " << versionInfo.sdk_version_string << "\n";
    }

    // Create SDK session
    printInfo("Creating SDK session...");
    RemoteSDK* sdk = RemoteSDK::CreateSession();
    if (!sdk) {
        printError("Failed to create SDK session");
        return 1;
    }

    // Connect to device
    printInfo("Connecting to Aurora device at " + std::string(server_address) + "...");
    SDKServerConnectionDesc connectionDesc(server_address);

    if (!sdk->connect(connectionDesc)) {
        printError("Failed to connect to Aurora device");
        sdk->release();
        return 1;
    }
    printSuccess("Connected to Aurora device");

    // Show device information
    printDeviceInfo(sdk);

    if (command == "status") {
        // Just show status, no power operation
        printInfo("Status check complete. No power operation performed.");
    }
    else if (command == "reboot") {
        if (!confirmOperation("REBOOT")) {
            printInfo("Operation cancelled by user");
            sdk->disconnect();
            sdk->release();
            return 0;
        }

        printInfo("Sending reboot command...");

        // Note: The device will reboot immediately, so we may not receive a response.
        // We don't check the result since the connection will be lost during reboot.
        sdk->controller.requestPowerOperation(
            SLAMTEC_AURORA_SDK_POWER_OP_REBOOT,
            5000,  // 5 second timeout
            nullptr,
            0,
            nullptr
        );

        std::cout << "\nReboot command sent.\n";
        std::cout << "The device will reboot shortly.\n";
        std::cout << "Please wait for the device to come back online.\n";
        std::cout << "This typically takes 30-60 seconds.\n";
    }
    else if (command == "shutdown") {
        if (!confirmOperation("SHUTDOWN")) {
            printInfo("Operation cancelled by user");
            sdk->disconnect();
            sdk->release();
            return 0;
        }

        printInfo("Sending shutdown command...");

        // Note: The device will shutdown immediately, so we may not receive a response.
        // We don't check the result since the connection will be lost during shutdown.
        sdk->controller.requestPowerOperation(
            SLAMTEC_AURORA_SDK_POWER_OP_SHUTDOWN,
            5000,  // 5 second timeout
            nullptr,
            0,
            nullptr
        );

        std::cout << "\nShutdown command sent.\n";
        std::cout << "The device will shut down shortly.\n";
        std::cout << "You will need to manually power on the device to use it again.\n";
    }

    // Cleanup - connection may already be lost if power operation was executed
    sdk->disconnect();
    sdk->release();

    return 0;
}
