/*
 * Copyright 2013-2025 SLAMTEC Co., Ltd.
 * Aurora Remote SDK - Pose Covariance Demo
 *
 * This demo shows how to retrieve and interpret pose covariance data from the
 * Aurora device, which provides uncertainty estimates for the device's position
 * and orientation.
 */

#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>

using namespace rp::standalone::aurora;

// Global variables for signal handling
static std::atomic<bool> g_running(true);
static std::atomic<int> g_covariance_count(0);

void signalHandler(int signal) {
    std::cout << "\n\nCtrl-C pressed, stopping..." << std::endl;
    g_running = false;
}

// Helper function to print a horizontal line
void printLine(char c = '=') {
    std::cout << std::string(60, c) << std::endl;
}

// Listener class for SDK callbacks
class DemoListener : public RemoteSDKListener {
public:
    void onPoseCovariance(uint64_t timestamp_ns, const PoseCovariance& covariance) override {
        g_covariance_count++;

        std::cout << "\n";
        printLine('-');
        std::cout << "Pose Covariance Update #" << g_covariance_count.load() << std::endl;
        printLine('-');
        std::cout << "Timestamp: " << timestamp_ns << " ns" << std::endl;

        // Convert to human-readable format
        PoseCovarianceReadable readable;
        if (covariance.toHumanReadable(readable)) {
            std::cout << std::fixed << std::setprecision(4);

            // Position uncertainty (95% confidence ellipsoid)
            auto ellipsoid = readable.getPositionEllipsoid95();
            std::cout << "\nPosition Uncertainty (95% confidence):" << std::endl;
            std::cout << "  Semi-axes (m): [" << ellipsoid[0] << ", "
                     << ellipsoid[1] << ", " << ellipsoid[2] << "]" << std::endl;

            // Position uncertainty in XY plane
            double radius_xy = readable.getPositionRadius95XY();
            std::cout << "  XY Radius (m): " << radius_xy << std::endl;

            // Rotation uncertainty (1-sigma in roll-pitch-yaw)
            auto rpy = readable.getRotation1SigmaRPY();
            std::cout << "\nRotation Uncertainty (1-sigma):" << std::endl;
            std::cout << "  Roll:  " << std::setw(7) << rpy[0] << " deg" << std::endl;
            std::cout << "  Pitch: " << std::setw(7) << rpy[1] << " deg" << std::endl;
            std::cout << "  Yaw:   " << std::setw(7) << rpy[2] << " deg" << std::endl;

            // Quality assessment
            std::cout << "\nQuality Assessment:" << std::endl;
            if (radius_xy < 0.05) {
                std::cout << "  Position: EXCELLENT (< 5 cm)" << std::endl;
            } else if (radius_xy < 0.10) {
                std::cout << "  Position: GOOD (< 10 cm)" << std::endl;
            } else if (radius_xy < 0.30) {
                std::cout << "  Position: FAIR (< 30 cm)" << std::endl;
            } else {
                std::cout << "  Position: POOR (> 30 cm)" << std::endl;
            }

            double max_rot = std::max({std::abs(rpy[0]), std::abs(rpy[1]), std::abs(rpy[2])});
            if (max_rot < 1.0) {
                std::cout << "  Rotation: EXCELLENT (< 1 deg)" << std::endl;
            } else if (max_rot < 3.0) {
                std::cout << "  Rotation: GOOD (< 3 deg)" << std::endl;
            } else if (max_rot < 5.0) {
                std::cout << "  Rotation: FAIR (< 5 deg)" << std::endl;
            } else {
                std::cout << "  Rotation: POOR (> 5 deg)" << std::endl;
            }
        } else {
            std::cout << "  Failed to convert covariance to readable format" << std::endl;
        }
    }

    void onConnectionStatus(slamtec_aurora_sdk_connection_status_t status) override {
        const char* statusStr;
        switch (status) {
            case SLAMTEC_AURORA_SDK_CONNECTION_STATUS_LOST:
                statusStr = "LOST";
                g_running = false;
                break;
            case SLAMTEC_AURORA_SDK_CONNECTION_STATUS_RESTORED:
                statusStr = "RESTORED";
                break;
            case SLAMTEC_AURORA_SDK_CONNECTION_STATUS_DEVICE_CONFIG_CHANGED:
                statusStr = "DEVICE_CONFIG_CHANGED";
                break;
            default:
                statusStr = "UNKNOWN";
                break;
        }
        std::cout << "\n[INFO] Connection status changed: " << statusStr << std::endl;
    }
};

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [server_address]" << std::endl;
    std::cout << "  server_address: Aurora device IP address (optional, auto-discovery if omitted)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << progName << "                    # Auto-discover Aurora device" << std::endl;
    std::cout << "  " << progName << " 192.168.1.100     # Connect to specific IP" << std::endl;
}

int main(int argc, const char* argv[]) {
    signal(SIGINT, signalHandler);

    printLine('=');
    std::cout << "Aurora Pose Covariance Demo" << std::endl;
    printLine('=');

    // Parse command line arguments
    const char* server_address = nullptr;

    if (argc >= 2) {
        if (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        server_address = argv[1];
    }

    // Get SDK version
    slamtec_aurora_sdk_version_info_t versionInfo;
    if (!RemoteSDK::GetSDKInfo(versionInfo)) {
        std::cerr << "Failed to get SDK version info" << std::endl;
        return 1;
    }
    std::cout << "SDK Version: " << versionInfo.sdk_version_string << std::endl;
    std::cout << "Build Date:  " << versionInfo.sdk_build_time << "\n" << std::endl;

    // Create SDK session with listener
    DemoListener listener;
    auto sdk = RemoteSDK::CreateSession(&listener);

    if (!sdk) {
        std::cerr << "Failed to create SDK session" << std::endl;
        return 1;
    }

    // Connect to Aurora device
    std::cout << "Step 1: Connecting to Aurora device..." << std::endl;

    SDKServerConnectionDesc conn;
    if (server_address == nullptr) {
        std::cout << "  Searching for Aurora devices (5 seconds)..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));

        std::vector<SDKServerConnectionDesc> servers;
        size_t count = sdk->controller.getDiscoveredServers(servers);

        if (count == 0) {
            std::cerr << "  ✗ No Aurora devices discovered" << std::endl;
            std::cerr << "  Please specify IP address or check network connection" << std::endl;
            return 1;
        }

        std::cout << "  Found " << count << " device(s), connecting to first one..." << std::endl;
        conn = servers[0];
    } else {
        std::cout << "  Connecting to " << server_address << "..." << std::endl;
        conn.push_back(SDKConnectionInfo(server_address));
    }

    if (!sdk->connect(conn)) {
        std::cerr << "  ✗ Failed to connect to Aurora device" << std::endl;
        return 1;
    }
    std::cout << "  ✓ Connected successfully\n" << std::endl;

    // Get device basic info
    RemoteDeviceBasicInfo devInfo;
    uint64_t timestamp;
    if (sdk->dataProvider.getLastDeviceBasicInfo(devInfo, timestamp)) {
        std::cout << "Device Information:" << std::endl;
        std::cout << "  Name:         " << devInfo.device_name << std::endl;
        std::cout << "  Model:        " << devInfo.getDeviceModelString() << std::endl;
        std::cout << "  Serial:       " << devInfo.getDeviceSerialNumberString() << std::endl;
        std::cout << "  FW Version:   " << devInfo.firmware_version_string << "\n" << std::endl;
    }

    // Wait for some tracking data to accumulate
    std::cout << "Step 2: Waiting for device to start tracking..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Test polling API
    std::cout << "\nStep 3: Testing pose covariance polling API..." << std::endl;
    PoseCovariance covariance;
    uint64_t cov_timestamp = 0;

    if (sdk->dataProvider.getRecentPoseCovariance(covariance, &cov_timestamp)) {
        std::cout << "  ✓ Successfully retrieved pose covariance" << std::endl;
        std::cout << "    Timestamp: " << cov_timestamp << " ns" << std::endl;

        PoseCovarianceReadable readable;
        if (covariance.toHumanReadable(readable)) {
            auto ellipsoid = readable.getPositionEllipsoid95();
            auto radius_xy = readable.getPositionRadius95XY();
            auto rpy = readable.getRotation1SigmaRPY();

            std::cout << std::fixed << std::setprecision(4);
            std::cout << "    Position 95% radius (XY): " << radius_xy << " m" << std::endl;
            std::cout << "    Rotation 1-sigma (RPY): ["
                     << rpy[0] << ", " << rpy[1] << ", " << rpy[2] << "] deg" << std::endl;
        }
    } else {
        std::cout << "  ⚠ Pose covariance not available yet" << std::endl;
        std::cout << "    Note: Requires firmware 2.1.0+ and active tracking" << std::endl;
        std::cout << "    Waiting for covariance data..." << std::endl;
    }

    // Display instructions
    std::cout << "\n";
    printLine('=');
    std::cout << "Monitoring Pose Covariance Updates" << std::endl;
    printLine('=');
    std::cout << "The demo will display covariance updates from the callback." << std::endl;
    std::cout << "Move the Aurora device to observe how uncertainty changes." << std::endl;
    std::cout << "\nPress Ctrl+C to stop\n" << std::endl;

    // Main loop - just wait and let callbacks handle updates
    auto start_time = std::chrono::steady_clock::now();

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Periodically display status if no covariance received
        if (g_covariance_count == 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

            if (elapsed > 10 && elapsed % 5 == 0) {
                std::cout << "\n[" << elapsed << "s] Still waiting for covariance data..." << std::endl;
                std::cout << "  Ensure device has firmware 2.1.0+ and is tracking" << std::endl;
            }
        }
    }

    // Cleanup
    std::cout << "\n\n";
    printLine('=');
    std::cout << "Shutting down..." << std::endl;
    printLine('=');

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

    std::cout << "\nStatistics:" << std::endl;
    std::cout << "  Total covariance updates: " << g_covariance_count.load() << std::endl;
    std::cout << "  Runtime:                  " << elapsed << " seconds" << std::endl;
    if (elapsed > 0 && g_covariance_count > 0) {
        double rate = g_covariance_count.load() / static_cast<double>(elapsed);
        std::cout << "  Average update rate:      " << std::fixed << std::setprecision(2)
                 << rate << " Hz" << std::endl;
    }

    sdk->disconnect();
    std::cout << "  ✓ Disconnected from device" << std::endl;
    std::cout << "\nDemo completed successfully!" << std::endl;

    return 0;
}
