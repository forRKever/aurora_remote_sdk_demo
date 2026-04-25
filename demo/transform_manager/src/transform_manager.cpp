/*
 * Copyright 2013-2025 SLAMTEC Co., Ltd.
 * Aurora Remote SDK - Transform Manager Demo
 *
 * This demo shows how to use the TransformManager to manage coordinate
 * frame transforms on the Aurora device.
 */

#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
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

void printPoseSE3(const std::string& name, const slamtec_aurora_sdk_pose_se3_t& pose) {
    std::cout << COLOR_BOLD << name << ":" << COLOR_RESET << "\n";
    std::cout << "  Translation: [" << std::fixed << std::setprecision(4)
              << pose.translation.x << ", " << pose.translation.y << ", " << pose.translation.z << "]\n";
    std::cout << "  Quaternion:  [" << std::fixed << std::setprecision(4)
              << pose.quaternion.x << ", " << pose.quaternion.y << ", "
              << pose.quaternion.z << ", " << pose.quaternion.w << "]\n";
}

void printUsage(const char* progName) {
    std::cout << COLOR_BOLD << "Usage: " << COLOR_RESET << progName << " <server_address> <command> [args...]\n\n";
    std::cout << COLOR_BOLD << "Commands:\n" << COLOR_RESET;
    std::cout << "  " << COLOR_GREEN << "list" << COLOR_RESET << "                                  - List all transforms\n";
    std::cout << "  " << COLOR_GREEN << "get" << COLOR_RESET << " <name>                           - Get a specific transform\n";
    std::cout << "  " << COLOR_GREEN << "set" << COLOR_RESET << " <name> <tx> <ty> <tz> <qx> <qy> <qz> <qw> - Set a transform\n";
    std::cout << "  " << COLOR_GREEN << "reset" << COLOR_RESET << " <name>                         - Reset a transform to identity\n";
    std::cout << "  " << COLOR_GREEN << "refresh" << COLOR_RESET << "                              - Refresh config from device\n";
    std::cout << "\n" << COLOR_BOLD << "Examples:\n" << COLOR_RESET;
    std::cout << "  " << progName << " 192.168.11.1 list\n";
    std::cout << "  " << progName << " 192.168.11.1 get T_cam_imu\n";
    std::cout << "  " << progName << " 192.168.11.1 set T_custom 0.1 0.2 0.3 0 0 0 1\n";
    std::cout << "  " << progName << " 192.168.11.1 reset T_custom\n";
    std::cout << "  " << progName << " 192.168.11.1 refresh\n";
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
    auto sdk = RemoteSDK::CreateSession();
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

    // Create TransformManager instance
    auto transformManager = sdk->createTransformManager();
    if (!transformManager) {
        printError("Failed to create TransformManager");
        return 1;
    }

    printSuccess("TransformManager created");

    try {
        std::string cmd(command);

        if (cmd == "list") {
            // List all transforms
            printInfo("Listing all transforms...");

            auto names = transformManager->getAllTransformNames(SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode);
            if (errcode != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                printError("Failed to get transform list. Error code: " + std::to_string(errcode));
                delete transformManager;
                return 1;
            }

            if (names.empty()) {
                printInfo("No transforms found");
            } else {
                std::cout << "\n" << COLOR_BOLD << "Found " << names.size() << " transform(s):" << COLOR_RESET << "\n";
                std::cout << "----------------------------------------\n";

                for (size_t i = 0; i < names.size(); i++) {
                    std::cout << "  " << (i + 1) << ". " << COLOR_CYAN << names[i] << COLOR_RESET << "\n";

                    slamtec_aurora_sdk_pose_se3_t pose;
                    if (transformManager->getTransform(names[i].c_str(), pose, SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                        std::cout << "     Translation: [" << std::fixed << std::setprecision(4)
                                  << pose.translation.x << ", " << pose.translation.y << ", " << pose.translation.z << "]\n";
                        std::cout << "     Quaternion:  [" << std::fixed << std::setprecision(4)
                                  << pose.quaternion.x << ", " << pose.quaternion.y << ", "
                                  << pose.quaternion.z << ", " << pose.quaternion.w << "]\n";
                    }
                }
                std::cout << "\n";
            }

        } else if (cmd == "get") {
            if (argc < 4) {
                printError("Missing transform name");
                printUsage(argv[0]);
                delete transformManager;
                return 1;
            }

            const char* name = argv[3];
            printInfo(std::string("Getting transform: ") + name);

            slamtec_aurora_sdk_pose_se3_t pose;
            if (!transformManager->getTransform(name, pose, SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to get transform. Error code: " + std::to_string(errcode));
                delete transformManager;
                return 1;
            }

            std::cout << "\n";
            printPoseSE3(name, pose);
            std::cout << "\n";

        } else if (cmd == "set") {
            if (argc < 11) {
                printError("Missing arguments for set command");
                printUsage(argv[0]);
                delete transformManager;
                return 1;
            }

            const char* name = argv[3];

            slamtec_aurora_sdk_pose_se3_t pose;
            pose.translation.x = std::stof(argv[4]);
            pose.translation.y = std::stof(argv[5]);
            pose.translation.z = std::stof(argv[6]);
            pose.quaternion.x = std::stof(argv[7]);
            pose.quaternion.y = std::stof(argv[8]);
            pose.quaternion.z = std::stof(argv[9]);
            pose.quaternion.w = std::stof(argv[10]);

            printInfo(std::string("Setting transform: ") + name);
            printPoseSE3("New transform", pose);

            if (!transformManager->setTransform(name, pose,
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to set transform. Error code: " + std::to_string(errcode));
                delete transformManager;
                return 1;
            }

            printSuccess("Transform updated successfully");

        } else if (cmd == "reset") {
            if (argc < 4) {
                printError("Missing transform name");
                printUsage(argv[0]);
                delete transformManager;
                return 1;
            }

            const char* name = argv[3];
            printInfo(std::string("Resetting transform: ") + name);

            // Reset to identity transform
            slamtec_aurora_sdk_pose_se3_t identity;
            identity.translation.x = 0.0f;
            identity.translation.y = 0.0f;
            identity.translation.z = 0.0f;
            identity.quaternion.x = 0.0f;
            identity.quaternion.y = 0.0f;
            identity.quaternion.z = 0.0f;
            identity.quaternion.w = 1.0f;

            if (!transformManager->setTransform(name, identity,
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to reset transform. Error code: " + std::to_string(errcode));
                delete transformManager;
                return 1;
            }

            printSuccess("Transform reset to identity");

        } else if (cmd == "refresh") {
            printInfo("Refreshing config from device...");

            bool success = transformManager->refresh(
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode);

            if (!success || errcode != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                printError("Failed to refresh config. Error code: " + std::to_string(errcode));
                delete transformManager;
                return 1;
            }

            printSuccess("Config refreshed successfully");

        } else {
            printError("Unknown command: " + cmd);
            printUsage(argv[0]);
            delete transformManager;
            return 1;
        }

    } catch (const std::exception& e) {
        printError(std::string("Exception: ") + e.what());
        delete transformManager;
        return 1;
    }

    // Cleanup
    delete transformManager;
    printInfo("Disconnecting...");
    sdk->controller.disconnect();

    // Explicitly release SDK session
    RemoteSDK::DestroySession(sdk);
    sdk = nullptr;

    printSuccess("Done!");

    return 0;
}
