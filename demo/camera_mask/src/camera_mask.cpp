/*
 * Copyright 2013-2025 SLAMTEC Co., Ltd.
 * Aurora Remote SDK - Camera Mask Manager Demo
 *
 * This demo shows how to use the CameraMaskManager to manage static camera
 * masks on the Aurora device. Camera masks can be used to exclude specific
 * regions of the camera image from visual SLAM processing.
 *
 * Note: This demo requires OpenCV for image I/O operations.
 */

#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstring>

#ifdef AURORA_SDK_HAS_OPENCV
#include <opencv2/opencv.hpp>
#endif

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
    std::cout << "  " << COLOR_GREEN << "status" << COLOR_RESET << "                             - Get mask enable status\n";
    std::cout << "  " << COLOR_GREEN << "enable" << COLOR_RESET << " <0|1>                        - Enable/disable static mask\n";
    std::cout << "  " << COLOR_GREEN << "list" << COLOR_RESET << "                               - List camera indices with masks\n";
#ifdef AURORA_SDK_HAS_OPENCV
    std::cout << "  " << COLOR_GREEN << "get-mask" << COLOR_RESET << " <camera_index> <output>   - Get static mask image and save to PNG file\n";
    std::cout << "  " << COLOR_GREEN << "set-mask" << COLOR_RESET << " <camera_index> <input>    - Set static mask image from PNG file\n";
#endif
    std::cout << "  " << COLOR_GREEN << "remove-mask" << COLOR_RESET << " <camera_index>         - Remove static mask for camera\n";
    std::cout << "  " << COLOR_GREEN << "refresh" << COLOR_RESET << "                            - Refresh config from device\n";
    std::cout << "\n" << COLOR_BOLD << "Examples:\n" << COLOR_RESET;
    std::cout << "  " << progName << " 192.168.11.1 status\n";
    std::cout << "  " << progName << " 192.168.11.1 enable 1\n";
    std::cout << "  " << progName << " 192.168.11.1 list\n";
#ifdef AURORA_SDK_HAS_OPENCV
    std::cout << "  " << progName << " 192.168.11.1 get-mask 0 mask_cam0.png\n";
    std::cout << "  " << progName << " 192.168.11.1 set-mask 0 mask_cam0.png\n";
#endif
    std::cout << "  " << progName << " 192.168.11.1 remove-mask 0\n";
    std::cout << "  " << progName << " 192.168.11.1 refresh\n";
}

#ifdef AURORA_SDK_HAS_OPENCV
bool savePNGImage(const std::string& filename, const RemoteCameraMaskImage& maskImage) {
    if (!maskImage.isValid()) {
        printError("Invalid mask image");
        return false;
    }

    const auto& desc = maskImage.desc;

    // Create OpenCV Mat from the image data - assuming grayscale
    cv::Mat image(desc.height, desc.width, CV_8UC1, (void*)maskImage.image._data);

    // Save as PNG
    try {
        if (!cv::imwrite(filename, image)) {
            printError("Failed to write PNG file");
            return false;
        }
    } catch (const cv::Exception& e) {
        printError(std::string("OpenCV error: ") + e.what());
        return false;
    }

    return true;
}

bool loadPNGImage(const std::string& filename, std::vector<uint8_t>& imageData, int& width, int& height) {
    try {
        // Load image as grayscale
        cv::Mat image = cv::imread(filename, cv::IMREAD_GRAYSCALE);

        if (image.empty()) {
            printError("Failed to load PNG image");
            return false;
        }

        // Check if it's 8-bit grayscale
        if (image.type() != CV_8UC1) {
            printError("Image must be 8-bit grayscale");
            return false;
        }

        width = image.cols;
        height = image.rows;

        // Copy image data
        size_t dataSize = width * height;
        imageData.resize(dataSize);
        memcpy(imageData.data(), image.data, dataSize);

        return true;
    } catch (const cv::Exception& e) {
        printError(std::string("OpenCV error: ") + e.what());
        return false;
    }
}
#endif

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

    // Create CameraMaskManager instance
    auto cameraMaskManager = sdk->createCameraMaskManager();
    if (!cameraMaskManager) {
        printError("Failed to create CameraMaskManager");
        return 1;
    }

    printSuccess("CameraMaskManager created");

    try {
        std::string cmd(command);

        if (cmd == "status") {
            // Get mask enable status
            printInfo("Getting static mask status...");

            bool enabled = cameraMaskManager->isStaticMaskEnabled(SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode);
            if (errcode != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                printError("Failed to get status. Error code: " + std::to_string(errcode));
                delete cameraMaskManager;
                return 1;
            }

            std::cout << "\n" << COLOR_BOLD << "Static Camera Mask Status:" << COLOR_RESET << "\n";
            std::cout << "  Enabled: " << (enabled ? COLOR_GREEN "Yes" : COLOR_RED "No") << COLOR_RESET << "\n\n";

        } else if (cmd == "enable") {
            if (argc < 4) {
                printError("Missing enable flag (0 or 1)");
                printUsage(argv[0]);
                delete cameraMaskManager;
                return 1;
            }

            bool enable = std::stoi(argv[3]) != 0;

            printInfo(std::string(enable ? "Enabling" : "Disabling") + " static mask...");

            if (!cameraMaskManager->setStaticMaskEnable(enable,
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to set enable status. Error code: " + std::to_string(errcode));
                delete cameraMaskManager;
                return 1;
            }

            printSuccess(std::string("Static mask ") + (enable ? "enabled" : "disabled") + " successfully");

        } else if (cmd == "list") {
            // List camera indices with masks
            printInfo("Listing camera indices with static masks...");

            auto indices = cameraMaskManager->getStaticCameraMaskImageIdList(
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode);

            if (errcode != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                printError("Failed to get mask list. Error code: " + std::to_string(errcode));
                delete cameraMaskManager;
                return 1;
            }

            if (indices.empty()) {
                printInfo("No camera masks configured");
            } else {
                std::cout << "\n" << COLOR_BOLD << "Found " << indices.size() << " camera mask(s):" << COLOR_RESET << "\n";
                std::cout << "----------------------------------------\n";
                for (size_t i = 0; i < indices.size(); i++) {
                    std::cout << "  " << (i + 1) << ". Camera index " << COLOR_CYAN << indices[i] << COLOR_RESET << "\n";
                }
                std::cout << "\n";
            }

#ifdef AURORA_SDK_HAS_OPENCV
        } else if (cmd == "get-mask") {
            if (argc < 5) {
                printError("Missing arguments for get-mask command");
                printUsage(argv[0]);
                delete cameraMaskManager;
                return 1;
            }

            int cameraIndex = std::stoi(argv[3]);
            const char* outputFile = argv[4];

            printInfo(std::string("Getting static mask image for camera ") + std::to_string(cameraIndex) + "...");

            auto maskImage = cameraMaskManager->getStaticCameraMaskImage(cameraIndex,
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode);

            if (errcode != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
                printError("Failed to get mask image. Error code: " + std::to_string(errcode));
                delete cameraMaskManager;
                return 1;
            }

            if (!maskImage.isValid()) {
                printError("Retrieved mask image is invalid");
                delete cameraMaskManager;
                return 1;
            }

            std::cout << "  Image size: " << maskImage.desc.width << "x" << maskImage.desc.height << "\n";
            std::cout << "  Data size: " << maskImage.desc.data_size << " bytes\n";

            if (savePNGImage(outputFile, maskImage)) {
                printSuccess(std::string("Mask image saved to: ") + outputFile);
            } else {
                printError("Failed to save mask image");
                delete cameraMaskManager;
                return 1;
            }

        } else if (cmd == "set-mask") {
            if (argc < 5) {
                printError("Missing arguments for set-mask command");
                printUsage(argv[0]);
                delete cameraMaskManager;
                return 1;
            }

            int cameraIndex = std::stoi(argv[3]);
            const char* inputFile = argv[4];

            printInfo(std::string("Loading mask image from: ") + inputFile);

            std::vector<uint8_t> imageData;
            int width, height;

            if (!loadPNGImage(inputFile, imageData, width, height)) {
                printError("Failed to load mask image");
                delete cameraMaskManager;
                return 1;
            }

            std::cout << "  Image size: " << width << "x" << height << "\n";
            std::cout << "  Data size: " << imageData.size() << " bytes\n";

            printInfo(std::string("Setting static mask image for camera ") + std::to_string(cameraIndex) + "...");

            if (!cameraMaskManager->setStaticCameraMaskImage(cameraIndex, width, height, imageData.data(), imageData.size(),
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to set mask image. Error code: " + std::to_string(errcode));
                delete cameraMaskManager;
                return 1;
            }

            printSuccess("Mask image uploaded successfully");
#endif

        } else if (cmd == "remove-mask") {
            if (argc < 4) {
                printError("Missing camera index");
                printUsage(argv[0]);
                delete cameraMaskManager;
                return 1;
            }

            int cameraIndex = std::stoi(argv[3]);

            printInfo(std::string("Removing static mask for camera ") + std::to_string(cameraIndex) + "...");

            if (!cameraMaskManager->removeStaticCameraMaskImage(cameraIndex,
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to remove mask. Error code: " + std::to_string(errcode));
                delete cameraMaskManager;
                return 1;
            }

            printSuccess("Mask removed successfully");

        } else if (cmd == "refresh") {
            printInfo("Refreshing config from device...");

            if (!cameraMaskManager->refresh(
                SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_TIMEOUT, &errcode)) {
                printError("Failed to refresh config. Error code: " + std::to_string(errcode));
                delete cameraMaskManager;
                return 1;
            }

            printSuccess("Config refreshed successfully");

        } else {
            printError("Unknown command: " + cmd);
            printUsage(argv[0]);
            delete cameraMaskManager;
            return 1;
        }

    } catch (const std::exception& e) {
        printError(std::string("Exception: ") + e.what());
        delete cameraMaskManager;
        return 1;
    }

    // Cleanup
    delete cameraMaskManager;
    printInfo("Disconnecting...");
    sdk->controller.disconnect();

    // Explicitly release SDK session
    RemoteSDK::DestroySession(sdk);
    sdk = nullptr;

    printSuccess("Done!");

    return 0;
}
