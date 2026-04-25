/*
 * Copyright 2013-2025 SLAMTEC Co., Ltd.
 * Aurora Remote SDK - Pose Augmentation Demo
 *
 * This demo shows how to use the pose augmentation feature to get high-frequency
 * pose output using IMU data integration, increasing pose update rate from typical
 * 10-15 Hz to 200+ Hz.
 */

#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>

using namespace rp::standalone::aurora;

// Global variables for signal handling and statistics
static std::atomic<bool> g_running(true);
static std::atomic<int> g_pose_count(0);
static std::atomic<int> g_visual_pose_count(0);
static std::atomic<int> g_mixed_pose_count(0);

void signalHandler(int signal) {
    std::cout << "\n\nCtrl-C pressed, stopping..." << std::endl;
    g_running = false;
}

// Listener class for SDK callbacks
class DemoListener : public RemoteSDKListener {
public:
    void onPoseAugmentationResult(uint64_t timestamp_ns,
                                  slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                                  const slamtec_aurora_sdk_pose_se3_t& pose) override {
        g_pose_count++;

        if (mode == SLAMTEC_AURORA_SDK_POSE_AUGMENTATION_MODE_VISUAL_ONLY) {
            g_visual_pose_count++;
        } else {
            g_mixed_pose_count++;
        }

        // Print pose at reduced rate to avoid flooding console
        if (g_pose_count % 50 == 0) {
            const char* mode_str = (mode == SLAMTEC_AURORA_SDK_POSE_AUGMENTATION_MODE_VISUAL_ONLY) ? "VISUAL" : "IMU_MIXED";

            std::cout << std::fixed << std::setprecision(3);
            std::cout << "[" << mode_str << "] Pose #" << g_pose_count.load()
                     << " - Position: (" << pose.translation.x << ", "
                     << pose.translation.y << ", " << pose.translation.z << ") m"
                     << " | Timestamp: " << timestamp_ns << " ns" << std::endl;
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
        std::cout << "[INFO] Connection status changed: " << statusStr << std::endl;
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

    std::cout << "=============================================" << std::endl;
    std::cout << "Aurora Pose Augmentation Demo" << std::endl;
    std::cout << "=============================================" << std::endl;

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

    // Configure pose augmentation
    std::cout << "Step 2: Configuring pose augmentation..." << std::endl;
    slamtec_aurora_sdk_pose_augmentation_config_t config{};  // Zero-initialize
    config.output_frequency = SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ;
    config.enable_smoothing = 0;  // Disable smoothing for lowest latency
    config.smoothing_factor = 0.3f;   // Only used if smoothing enabled

    std::cout << "  Configuration:" << std::endl;
    std::cout << "    - Target frequency: 200 Hz" << std::endl;
    std::cout << "    - Smoothing: Disabled (for lowest latency)" << std::endl;
    std::cout << "    - Mode: IMU-Vision Mixed" << std::endl;

    // Start pose augmentation
    std::cout << "\nStep 3: Starting pose augmentation..." << std::endl;
    slamtec_aurora_sdk_errorcode_t errcode;
    if (!sdk->dataProvider.startPoseAugmentation(SLAMTEC_AURORA_SDK_POSE_AUGMENTATION_MODE_IMU_VISION_MIXED, config, &errcode)) {
        std::cerr << "  ✗ Failed to start pose augmentation, error code: " << errcode << std::endl;
        sdk->disconnect();
        return 1;
    }
    std::cout << "  ✓ Pose augmentation started\n" << std::endl;

    // Display instructions
    std::cout << "=============================================" << std::endl;
    std::cout << "Receiving High-Frequency Pose Updates" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "The demo is now receiving pose updates at high frequency." << std::endl;
    std::cout << "Move the Aurora device to see pose changes." << std::endl;
    std::cout << "(Displaying every 50th pose to avoid flooding console)" << std::endl;
    std::cout << "\nPress Ctrl+C to stop\n" << std::endl;

    // Main loop - monitor and display statistics
    auto start_time = std::chrono::steady_clock::now();
    int last_pose_count = 0;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

        int current_poses = g_pose_count.load();
        int poses_this_interval = current_poses - last_pose_count;
        double avg_rate = poses_this_interval / 5.0;

        std::cout << "\n[Statistics after " << elapsed << "s]" << std::endl;
        std::cout << "  Total poses:    " << current_poses << std::endl;
        std::cout << "  Visual poses:   " << g_visual_pose_count.load() << std::endl;
        std::cout << "  Mixed poses:    " << g_mixed_pose_count.load() << std::endl;
        std::cout << "  Avg pose rate:  " << std::fixed << std::setprecision(1)
                 << avg_rate << " Hz (last 5s)" << std::endl;

        last_pose_count = current_poses;
    }

    // Cleanup
    std::cout << "\n\n=============================================" << std::endl;
    std::cout << "Shutting down..." << std::endl;
    std::cout << "=============================================" << std::endl;

    sdk->dataProvider.stopPoseAugmentation();
    std::cout << "  ✓ Stopped pose augmentation" << std::endl;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    double overall_rate = elapsed > 0 ? g_pose_count.load() / static_cast<double>(elapsed) : 0;

    std::cout << "\nFinal Statistics:" << std::endl;
    std::cout << "  Total poses received: " << g_pose_count.load() << std::endl;
    std::cout << "  Visual poses:         " << g_visual_pose_count.load() << std::endl;
    std::cout << "  IMU-mixed poses:      " << g_mixed_pose_count.load() << std::endl;
    std::cout << "  Runtime:              " << elapsed << " seconds" << std::endl;
    std::cout << "  Overall pose rate:    " << std::fixed << std::setprecision(1)
             << overall_rate << " Hz" << std::endl;

    sdk->disconnect();
    std::cout << "  ✓ Disconnected from device" << std::endl;
    std::cout << "\nDemo completed successfully!" << std::endl;

    return 0;
}
