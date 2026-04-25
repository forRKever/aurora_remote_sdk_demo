/*
 * Copyright 2013-2025 SLAMTEC Co., Ltd.
 * Aurora Remote SDK - Time Synchronization Demo
 *
 * This demo shows how to use the time synchronization feature to translate
 * timestamps between Aurora device time domain and local system time domain.
 * It demonstrates:
 * - Steady clock synchronization for timestamp translation
 * - Wall clock synchronization to sync device system time
 * - Practical usage by translating timestamps from pose and IMU data
 */

#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <csignal>
#include <atomic>
#include <algorithm>

using namespace rp::standalone::aurora;

static std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    std::cout << "\n\nCtrl-C pressed, stopping..." << std::endl;
    g_running = false;
}

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " <command> [server_address] [port]" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  steady    - Steady clock sync with timestamp translation demo" << std::endl;
    std::cout << "  wallclock - Wall clock sync to synchronize device system time" << std::endl;
    std::cout << "  all       - Run both demos (default)" << std::endl;
    std::cout << std::endl;
    std::cout << "  server_address: Aurora device IP address (default: auto-discover)" << std::endl;
    std::cout << "  port:           Time sync service port (default: 9527)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  " << progName << " steady 192.168.1.100" << std::endl;
    std::cout << "  " << progName << " wallclock 192.168.1.100 9527" << std::endl;
    std::cout << "  " << progName << " all" << std::endl;
}

bool discoverAuroraDevice(RemoteSDK* sdk, SDKServerConnectionDesc& selectedDeviceDesc) {
    std::vector<SDKServerConnectionDesc> serverList;
    size_t count = sdk->getDiscoveredServers(serverList, 32);
    if (count == 0) {
        std::cerr << "No Aurora devices found" << std::endl;
        return false;
    }

    std::cout << "Found " << count << " Aurora device(s)" << std::endl;
    for (size_t i = 0; i < count; i++) {
        std::cout << "  Device " << i << ": ";
        for (size_t j = 0; j < serverList[i].size(); ++j) {
            if (j > 0) std::cout << ", ";
            std::cout << serverList[i][j].toLocatorString();
        }
        std::cout << std::endl;
    }

    selectedDeviceDesc = serverList[0];
    std::cout << "Selected first device" << std::endl;
    return true;
}

// ============================================================================
// Wall Clock Sync Demo
// ============================================================================
// This demonstrates how to synchronize the Aurora device's wall clock (system time)
// with the client's system time. This is useful when you need the device to have
// accurate absolute time for logging, timestamping files, etc.

bool runWallClockSyncDemo(const std::string& server_address, uint16_t port) {
    std::cout << "\n=============================================" << std::endl;
    std::cout << "Wall Clock Synchronization Demo" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "This demo synchronizes the Aurora device's system clock" << std::endl;
    std::cout << "with your local machine's time.\n" << std::endl;

    try {
        // Create client with wall clock domain
        RemoteTimeSyncClient client(TimeSyncDomain::WALL_CLOCK);
        std::cout << "Step 1: Created wall clock sync client" << std::endl;

        // Connect
        if (!client.connect(server_address.c_str(), port)) {
            std::cerr << "  ✗ Failed to connect to " << server_address << ":" << port << std::endl;
            return false;
        }
        std::cout << "  ✓ Connected to " << server_address << ":" << port << std::endl;

        // Step 1: Get wall clock offset
        std::cout << "\nStep 2: Querying wall clock offset..." << std::endl;
        slamtec_aurora_sdk_wallclock_offset_result_t offset_result;
        if (!client.getWallClockOffset(1000, &offset_result)) {
            std::cerr << "  ✗ Failed to get wall clock offset" << std::endl;
            return false;
        }

        double offset_ms = offset_result.offset_ns / 1e6;
        double rtt_ms = offset_result.rtt_ns / 1e6;

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "  Offset:     " << offset_ms << " ms" << std::endl;
        std::cout << "  RTT:        " << rtt_ms << " ms" << std::endl;

        // Determine if sync is needed
        double abs_offset_ms = std::abs(offset_ms);
        if (abs_offset_ms > 10.0) {
            std::cout << "  Status:     Large offset detected - sync recommended" << std::endl;
        } else if (abs_offset_ms > 1.0) {
            std::cout << "  Status:     Small offset detected - sync optional" << std::endl;
        } else {
            std::cout << "  Status:     Clocks already well synchronized" << std::endl;
        }

        // Step 2: Sync server wall clock if offset is significant
        if (abs_offset_ms > 1.0) {
            std::cout << "\nStep 3: Synchronizing device wall clock..." << std::endl;

            slamtec_aurora_sdk_wallclock_sync_result_t sync_result = {};
            slamtec_aurora_sdk_errorcode_t sync_error = SLAMTEC_AURORA_SDK_ERRORCODE_OK;

            if (client.syncServerWallClock(1000, &sync_result, &sync_error)) {
                std::cout << "  ✓ Wall clock synchronized successfully!" << std::endl;
                std::cout << "  Applied offset: " << sync_result.applied_offset_ns / 1e6 << " ms" << std::endl;

                // Step 3: Evaluate sync accuracy
                std::cout << "\nStep 4: Evaluating sync accuracy..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                std::vector<double> accuracy_errors;
                for (int i = 0; i < 10; ++i) {
                    slamtec_aurora_sdk_wallclock_accuracy_result_t accuracy_result;
                    if (client.evaluateWallClockSyncAccuracy(1000, &accuracy_result)) {
                        double error_ms = std::abs(accuracy_result.offset_error_ns / 1e6);
                        accuracy_errors.push_back(error_ms);
                        std::cout << "  Sample " << (i + 1) << ": Error = " << error_ms << " ms" << std::endl;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                // Calculate accuracy statistics
                if (!accuracy_errors.empty()) {
                    double mean = 0.0;
                    for (double err : accuracy_errors) {
                        mean += err;
                    }
                    mean /= accuracy_errors.size();

                    double max_err = *std::max_element(accuracy_errors.begin(), accuracy_errors.end());

                    std::cout << "\nSync Accuracy Statistics:" << std::endl;
                    std::cout << "  Mean Error: " << mean << " ms" << std::endl;
                    std::cout << "  Max Error:  " << max_err << " ms" << std::endl;

                    if (max_err < 1.0) {
                        std::cout << "  Verdict:    EXCELLENT (< 1ms)" << std::endl;
                    } else if (max_err < 5.0) {
                        std::cout << "  Verdict:    GOOD (< 5ms)" << std::endl;
                    } else if (max_err < 10.0) {
                        std::cout << "  Verdict:    ACCEPTABLE (< 10ms)" << std::endl;
                    } else {
                        std::cout << "  Verdict:    POOR (> 10ms)" << std::endl;
                    }
                }
            } else {
                std::cerr << "  ✗ Failed to sync wall clock (error code: " << sync_error << ")" << std::endl;
                std::cerr << "\n  This usually means:" << std::endl;
                std::cerr << "  1. Device is not running with sufficient privileges" << std::endl;
                std::cerr << "     - On Linux: run device service with 'sudo'" << std::endl;
                std::cerr << "     - On Windows: run as Administrator" << std::endl;
                std::cerr << "  2. Device may not support system clock modification" << std::endl;
                return false;
            }
        } else {
            std::cout << "\nSkipping sync - offset is already small enough" << std::endl;
        }

        std::cout << "\n  ✓ Wall clock sync demo completed!" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

// ============================================================================
// Steady Clock Sync Demo
// ============================================================================
// This demonstrates timestamp translation using steady clock synchronization.

bool runSteadyClockSyncDemo(const std::string& server_address, uint16_t port) {
    std::cout << "\n=============================================" << std::endl;
    std::cout << "Steady Clock Synchronization Demo" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << "This demo establishes time synchronization and translates" << std::endl;
    std::cout << "timestamps from pose and IMU data.\n" << std::endl;

    try {
        // Step 1: Create time sync client with steady clock domain
        std::cout << "Step 1: Creating time sync client..." << std::endl;
        RemoteTimeSyncClient timeSyncClient(TimeSyncDomain::STEADY_CLOCK);
        std::cout << "  ✓ Client created" << std::endl;

        // Step 2: Connect to Aurora device time sync service
        std::cout << "\nStep 2: Connecting to time sync service..." << std::endl;
        std::cout << "  Address: " << server_address << ":" << port << std::endl;

        if (!timeSyncClient.connect(server_address.c_str(), port)) {
            std::cerr << "  ✗ Failed to connect to time sync service" << std::endl;
            return false;
        }
        std::cout << "  ✓ Connected successfully" << std::endl;

        // Step 3: Configure synchronization options
        std::cout << "\nStep 3: Configuring synchronization options..." << std::endl;
        auto options = TimeSyncOptions::getDefaults();

        if (!timeSyncClient.setOptions(options)) {
            std::cerr << "  ✗ Failed to set options" << std::endl;
            return false;
        }
        std::cout << "  ✓ Options configured" << std::endl;

        // Step 4: Initialize time synchronization
        std::cout << "\nStep 4: Initializing time synchronization..." << std::endl;
        if (!timeSyncClient.initialize()) {
            std::cerr << "  ✗ Failed to initialize" << std::endl;
            return false;
        }
        std::cout << "  ✓ Initialization successful" << std::endl;

        // Step 5: Wait for synchronization to complete
        std::cout << "\nStep 5: Waiting for synchronization..." << std::endl;
        int wait_count = 0;
        const int max_wait = 100;
        while (!timeSyncClient.isSynchronized() && wait_count < max_wait) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
            if (wait_count % 10 == 0) {
                std::cout << "  Still waiting... (" << wait_count / 10 << "s)" << std::endl;
            }
        }

        if (!timeSyncClient.isSynchronized()) {
            std::cerr << "  ✗ Failed to synchronize within timeout" << std::endl;
            return false;
        }
        std::cout << "  ✓ Synchronized!" << std::endl;

        // Display synchronization quality metrics
        std::cout << "\n========================================" << std::endl;
        std::cout << "Synchronization Quality Metrics" << std::endl;
        std::cout << "========================================" << std::endl;

        TimeSyncQuality quality;
        if (timeSyncClient.getQuality(&quality)) {
            std::cout << std::fixed << std::setprecision(3);
            std::cout << "  RMSE:              " << quality.rmse_ms << " ms" << std::endl;
            std::cout << "  Max Error:         " << quality.max_error_ms << " ms" << std::endl;
            std::cout << "  Scale Factor:      " << std::setprecision(9) << quality.scale << std::endl;
            std::cout << "  Offset:            " << std::setprecision(3) << quality.offset_ns / 1e6 << " ms" << std::endl;
            std::cout << "  Samples Used:      " << quality.sample_count << "/" << quality.total_sync_count << std::endl;

            if (quality.rmse_ms < 1.0) {
                std::cout << "\n  Assessment: EXCELLENT (< 1ms)" << std::endl;
            } else if (quality.rmse_ms < 5.0) {
                std::cout << "\n  Assessment: GOOD (< 5ms)" << std::endl;
            } else {
                std::cout << "\n  Assessment: FAIR (> 5ms)" << std::endl;
            }
        }

        // Connect to Aurora Device and Retrieve Data
        std::cout << "\n\n=============================================" << std::endl;
        std::cout << "Retrieving and Translating Data" << std::endl;
        std::cout << "=============================================" << std::endl;

        // Create SDK session
        std::cout << "\nConnecting to Aurora device for data retrieval..." << std::endl;
        RemoteSDK* sdk = RemoteSDK::CreateSession();
        if (!sdk) {
            std::cerr << "  ✗ Failed to create SDK session" << std::endl;
            return false;
        }

        // Connect to device
        SDKServerConnectionDesc deviceDesc(server_address.c_str());

        if (!sdk->connect(deviceDesc)) {
            std::cerr << "  ✗ Failed to connect to Aurora device" << std::endl;
            sdk->release();
            return false;
        }
        std::cout << "  ✓ Connected to Aurora device\n" << std::endl;

        // Display instructions
        std::cout << "========================================" << std::endl;
        std::cout << "Timestamp Translation in Action" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Displaying pose and IMU data with timestamp translation." << std::endl;
        std::cout << "Press Ctrl+C to stop\n" << std::endl;

        // Main loop: retrieve and translate timestamps
        uint64_t lastIMUTimestamp = 0;
        int pose_count = 0;
        int imu_count = 0;

        auto start_time = std::chrono::steady_clock::now();

        while (g_running) {
            // Get pose data with timestamp
            slamtec_aurora_sdk_pose_se3_t pose;
            uint64_t aurora_timestamp = 0;

            if (sdk->dataProvider.getCurrentPoseSE3WithTimestamp(pose, aurora_timestamp)) {
                uint64_t local_timestamp = 0;

                // Translate timestamp
                if (timeSyncClient.translateTimestamp(aurora_timestamp, &local_timestamp)) {
                    pose_count++;

                    // Calculate time difference
                    int64_t diff_ns = static_cast<int64_t>(local_timestamp) - static_cast<int64_t>(aurora_timestamp);
                    double diff_ms = diff_ns / 1e6;

                    // Print every 10th pose to avoid flooding
                    if (pose_count % 10 == 0) {
                        std::cout << std::fixed << std::setprecision(3);
                        std::cout << "\n[POSE #" << pose_count << "]" << std::endl;
                        std::cout << "  Position:        (" << pose.translation.x << ", "
                                 << pose.translation.y << ", "
                                 << pose.translation.z << ") m" << std::endl;
                        std::cout << "  Aurora Time:     " << aurora_timestamp << " ns" << std::endl;
                        std::cout << "  Local Time:      " << local_timestamp << " ns" << std::endl;
                        std::cout << "  Time Offset:     " << std::setprecision(3) << diff_ms << " ms" << std::endl;
                    }
                }
            }

            // Get IMU data with timestamps
            std::vector<slamtec_aurora_sdk_imu_data_t> imuData;
            if (sdk->dataProvider.peekIMUData(imuData)) {
                for (auto& imu : imuData) {
                    if (imu.timestamp_ns <= lastIMUTimestamp) {
                        continue;  // Skip old data
                    }
                    lastIMUTimestamp = imu.timestamp_ns;

                    uint64_t imu_aurora_timestamp = imu.timestamp_ns;
                    uint64_t local_timestamp = 0;

                    // Translate timestamp
                    if (timeSyncClient.translateTimestamp(imu_aurora_timestamp, &local_timestamp)) {
                        imu_count++;

                        // Calculate time difference
                        int64_t diff_ns = static_cast<int64_t>(local_timestamp) - static_cast<int64_t>(imu_aurora_timestamp);
                        double diff_ms = diff_ns / 1e6;

                        // Print every 100th IMU sample to avoid flooding
                        if (imu_count % 100 == 0) {
                            std::cout << std::fixed << std::setprecision(4);
                            std::cout << "\n[IMU #" << imu_count << "]" << std::endl;
                            std::cout << "  Accel (g):       (" << imu.acc[0] << ", " << imu.acc[1] << ", " << imu.acc[2] << ")" << std::endl;
                            std::cout << "  Gyro (dps):      (" << imu.gyro[0] << ", " << imu.gyro[1] << ", " << imu.gyro[2] << ")" << std::endl;
                            std::cout << "  Aurora Time:     " << imu_aurora_timestamp << " ns" << std::endl;
                            std::cout << "  Local Time:      " << local_timestamp << " ns" << std::endl;
                            std::cout << "  Time Offset:     " << std::setprecision(3) << diff_ms << " ms" << std::endl;
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            // Print periodic summary
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            if (elapsed > 0 && elapsed % 10 == 0) {
                static int last_report = 0;
                if (elapsed != last_report) {
                    last_report = elapsed;
                    std::cout << "\n[STATISTICS after " << elapsed << "s]" << std::endl;
                    std::cout << "  Total poses translated:  " << pose_count << std::endl;
                    std::cout << "  Total IMU samples:       " << imu_count << std::endl;

                    // Check sync quality
                    TimeSyncQuality current_quality;
                    if (timeSyncClient.getQuality(&current_quality)) {
                        std::cout << "  Current RMSE:            " << std::fixed << std::setprecision(3)
                                 << current_quality.rmse_ms << " ms" << std::endl;
                    }
                }
            }
        }

        // Cleanup
        std::cout << "\n\n=============================================" << std::endl;
        std::cout << "Shutting down..." << std::endl;
        std::cout << "=============================================" << std::endl;

        auto end_time = std::chrono::steady_clock::now();
        auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();

        std::cout << "\nFinal Statistics:" << std::endl;
        std::cout << "  Total poses translated:  " << pose_count << std::endl;
        std::cout << "  Total IMU samples:       " << imu_count << std::endl;
        std::cout << "  Runtime:                 " << total_elapsed << " seconds" << std::endl;

        sdk->disconnect();
        sdk->release();
        std::cout << "  ✓ Disconnected from device" << std::endl;

        timeSyncClient.stop();
        std::cout << "  ✓ Time synchronization stopped" << std::endl;

        std::cout << "\n  ✓ Steady clock sync demo completed!" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "\nException: " << e.what() << std::endl;
        return false;
    }
}

int main(int argc, const char* argv[]) {
    signal(SIGINT, signalHandler);

    std::cout << "=============================================" << std::endl;
    std::cout << "Aurora Time Synchronization Demo" << std::endl;
    std::cout << "=============================================" << std::endl;

    // Parse command line arguments
    std::string command = "all";
    const char* server_address = nullptr;
    uint16_t port = 9527;

    int arg_offset = 1;
    if (argc >= 2) {
        std::string arg1 = argv[1];
        if (arg1 == "-h" || arg1 == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        if (arg1 == "steady" || arg1 == "wallclock" || arg1 == "all") {
            command = arg1;
            arg_offset = 2;
        }
    }

    if (argc > arg_offset) {
        server_address = argv[arg_offset];
    }

    if (argc > arg_offset + 1) {
        port = static_cast<uint16_t>(std::atoi(argv[arg_offset + 1]));
    }

    // Get SDK version
    slamtec_aurora_sdk_version_info_t versionInfo;
    if (!RemoteSDK::GetSDKInfo(versionInfo)) {
        std::cerr << "Failed to get SDK version info" << std::endl;
        return 1;
    }
    std::cout << "SDK Version: " << versionInfo.sdk_version_string << std::endl;
    std::cout << "Command: " << command << "\n" << std::endl;

    // Determine server address if not provided
    std::string sync_address;
    if (server_address == nullptr) {
        std::cout << "Discovering Aurora devices..." << std::endl;

        // Create a temporary SDK session for discovery
        RemoteSDK* tempSdk = RemoteSDK::CreateSession();
        if (!tempSdk) {
            std::cerr << "Failed to create SDK session for discovery" << std::endl;
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
        SDKServerConnectionDesc deviceDesc;
        if (!discoverAuroraDevice(tempSdk, deviceDesc)) {
            std::cerr << "No devices discovered" << std::endl;
            tempSdk->release();
            return 1;
        }

        // Extract IP address from the connection descriptor
        sync_address = deviceDesc[0].address;
        std::cout << "Will use address: " << sync_address << std::endl;
        tempSdk->release();
    } else {
        sync_address = server_address;
        std::cout << "Using specified address: " << sync_address << std::endl;
    }

    bool success = true;

    // Run the selected demo(s)
    if (command == "steady" || command == "all") {
        success = runSteadyClockSyncDemo(sync_address, port) && success;
        g_running = true;  // Reset for next demo if running all
    }

    if (command == "wallclock" || command == "all") {
        success = runWallClockSyncDemo(sync_address, port) && success;
    }

    std::cout << "\n=============================================" << std::endl;
    if (success) {
        std::cout << "Demo completed successfully!" << std::endl;
    } else {
        std::cout << "Demo completed with errors." << std::endl;
    }
    std::cout << "=============================================" << std::endl;

    return success ? 0 : 1;
}
