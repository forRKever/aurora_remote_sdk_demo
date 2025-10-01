/* 
 *  SLAMTEC AURORA DEMO - COLMAP DATASET RECORDER
 *  This demo shows how to use the DataRecorder mechanism to record colmap dataset
 *  from a remote aurora device
 */

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <thread>
#include <csignal>
#include <cstring>
#include "aurora_pubsdk_inc.h"

using namespace rp::standalone::aurora;

static int isCtrlC = 0;
static int timeout_seconds = 0;

static void onCtrlC(int) {
    std::cout << "Ctrl-C pressed, stopping recording..." << std::endl;
    isCtrlC = 1;
}

bool discoverAndSelectAuroraDevice(RemoteSDK * sdk, SDKServerConnectionDesc & selectedDeviceDesc)
{
    std::vector<SDKServerConnectionDesc> serverList;
    size_t count = sdk->getDiscoveredServers(serverList, 32);
    if (count == 0) {
        std::cerr << "No aurora devices found" << std::endl;
        return false;
    }

    // print the server list
    std::cout << "Found " << count << " aurora devices" << std::endl;
    for (size_t i = 0; i < count; i++) {
        std::cout << "Device " << i << std::endl;
        for (size_t j = 0; j < serverList[i].size(); ++j)
        {
            auto & connectionOption = serverList[i][j];
            std::cout << "  option " << j << ": " << connectionOption.toLocatorString() << std::endl;
        }
    }

    // select the first device
    selectedDeviceDesc = serverList[0];
    std::cout << "Selected first device" << std::endl;
    return true;
}

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " --output <folder> [OPTIONS]" << std::endl;
    std::cout << "Required arguments:" << std::endl;
    std::cout << "  --output <folder>       Folder path to store the recorded dataset" << std::endl;
    std::cout << std::endl;
    std::cout << "Optional arguments:" << std::endl;
    std::cout << "  --device <ip>           Device IP address (auto-discover if not specified)" << std::endl;
    std::cout << "  --timeout <seconds>     Timeout in seconds (0 = no timeout, default)" << std::endl;
    std::cout << std::endl;
    std::cout << "Common Options:" << std::endl;
    std::cout << "  --image-quality <type>  Image stream type: raw (default), preview" << std::endl;
    std::cout << std::endl;
    std::cout << "Colmap Recorder Options:" << std::endl;
    std::cout << "  --stereo-recording      Enable stereo image recording (default: false)" << std::endl;
    std::cout << "  --undistort             Enable image undistortion (default: true)" << std::endl;
    std::cout << "  --no-undistort          Disable image undistortion" << std::endl;
    std::cout << "  --force-focal-center    Force focal center to image center (default: true)" << std::endl;
    std::cout << "  --no-force-focal-center Do not force focal center to image center" << std::endl;
    std::cout << "  --keep-unused-points    Keep unused map points (default: false)" << std::endl;
    std::cout << "  --multi-mapper          Store data in sparse/n/ folder (default: false)" << std::endl;
    std::cout << "  --file-format <format>  Output format: binary (default), text, all" << std::endl;
    std::cout << std::endl;
    std::cout << "  --help                  Show this help message" << std::endl;
}

int main(int argc, char** argv) {
    const char* outputFolder = nullptr;
    const char* connectionString = nullptr;
    
    // Recording options with defaults
    const char* imageQuality = "raw";
    bool stereoRecording = false;
    bool undistort = true;
    bool forceFocalCenter = true;
    bool keepUnusedPoints = false;
    bool multiMapper = false;
    const char* fileFormat = "binary";
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                outputFolder = argv[++i];
            } else {
                std::cerr << "Error: --output requires a value" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "--device") == 0) {
            if (i + 1 < argc) {
                connectionString = argv[++i];
            } else {
                std::cerr << "Error: --device requires a value" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "--timeout") == 0) {
            if (i + 1 < argc) {
                timeout_seconds = std::atoi(argv[++i]);
                if (timeout_seconds < 0) timeout_seconds = 0;
            } else {
                std::cerr << "Error: --timeout requires a value" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "--image-quality") == 0) {
            if (i + 1 < argc) {
                const char* quality = argv[++i];
                if (strcmp(quality, "raw") == 0 || strcmp(quality, "preview") == 0) {
                    imageQuality = quality;
                } else {
                    std::cerr << "Error: --image-quality must be 'raw' or 'preview'" << std::endl;
                    printUsage(argv[0]);
                    return 1;
                }
            } else {
                std::cerr << "Error: --image-quality requires a value" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "--stereo-recording") == 0) {
            stereoRecording = true;
        } else if (strcmp(argv[i], "--undistort") == 0) {
            undistort = true;
        } else if (strcmp(argv[i], "--no-undistort") == 0) {
            undistort = false;
        } else if (strcmp(argv[i], "--force-focal-center") == 0) {
            forceFocalCenter = true;
        } else if (strcmp(argv[i], "--no-force-focal-center") == 0) {
            forceFocalCenter = false;
        } else if (strcmp(argv[i], "--keep-unused-points") == 0) {
            keepUnusedPoints = true;
        } else if (strcmp(argv[i], "--multi-mapper") == 0) {
            multiMapper = true;
        } else if (strcmp(argv[i], "--file-format") == 0) {
            if (i + 1 < argc) {
                const char* format = argv[++i];
                if (strcmp(format, "binary") == 0 || strcmp(format, "text") == 0 || strcmp(format, "all") == 0) {
                    fileFormat = format;
                } else {
                    std::cerr << "Error: --file-format must be 'binary', 'text', or 'all'" << std::endl;
                    printUsage(argv[0]);
                    return 1;
                }
            } else {
                std::cerr << "Error: --file-format requires a value" << std::endl;
                printUsage(argv[0]);
                return 1;
            }
        } else {
            std::cerr << "Error: Unknown argument '" << argv[i] << "'" << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }

    // Check required arguments
    if (outputFolder == nullptr) {
        std::cerr << "Error: --output argument is required" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    // Simple validation of output folder path
    std::cout << "Dataset will be stored in: " << outputFolder << std::endl;

    // Print current recording options
    std::cout << std::endl << "Recording options:" << std::endl;
    std::cout << "  Image quality: " << imageQuality;
    if (strcmp(imageQuality, "preview") == 0) {
        std::cout << " (WARNING: Preview image will reduce image quality)";
    }
    std::cout << std::endl;
    std::cout << "  Stereo recording: " << (stereoRecording ? "enabled" : "disabled") << std::endl;
    std::cout << "  Undistort images: " << (undistort ? "enabled" : "disabled") << std::endl;
    std::cout << "  Force focal center: " << (forceFocalCenter ? "enabled" : "disabled") << std::endl;
    std::cout << "  Keep unused map points: " << (keepUnusedPoints ? "enabled" : "disabled") << std::endl;
    std::cout << "  Multi-mapper: " << (multiMapper ? "enabled" : "disabled") << std::endl;
    std::cout << "  File format: " << fileFormat << std::endl;
    std::cout << std::endl;

    // register the ctrl-c signal handler
    signal(SIGINT, onCtrlC);

    // print the version info
    slamtec_aurora_sdk_version_info_t versionInfo;
    RemoteSDK::GetSDKInfo(versionInfo);
    std::cout << "Aurora SDK Version: " << versionInfo.sdk_version_string << std::endl;

    RemoteSDK * sdk = RemoteSDK::CreateSession();
    if (sdk == nullptr) {
        std::cerr << "Failed to create session" << std::endl;
        return 1;
    }

    // wait for the sdk to detect aurora devices
    SDKServerConnectionDesc selectedDeviceDesc;
    if (connectionString == nullptr) {
        std::cout << "Device connection string not provided, trying to discover aurora devices..." << std::endl;
        std::cout << "Waiting for aurora devices..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        if (!discoverAndSelectAuroraDevice(sdk, selectedDeviceDesc)) {
            std::cerr << "Failed to discover aurora devices" << std::endl;
            sdk->release();
            return 1;
        }
    } else {
        selectedDeviceDesc = SDKServerConnectionDesc(connectionString);
        std::cout << "Selected device: " << selectedDeviceDesc[0].toLocatorString() << std::endl;
    }

    // connect to the selected device
    std::cout << "Connecting to the selected device..." << std::endl;
    if (!sdk->connect(selectedDeviceDesc)) {
        std::cerr << "Failed to connect to the selected device" << std::endl;
        sdk->release();
        return 1;
    }
    std::cout << "Connected to the selected device" << std::endl;

    // Enable background map data syncing (required for recording)
    std::cout << "Starting background map data syncing..." << std::endl;
    sdk->startBackgroundMapDataSyncing();

    // Configure recorder options before starting recording
    std::cout << "Configuring recording options..." << std::endl;
    
    // Common options
    if (!sdk->colmapDataRecorder.setOptionString("image_quality", imageQuality)) {
        std::cerr << "Warning: Failed to set image_quality option" << std::endl;
    }
    
    // Colmap specific options
    if (!sdk->colmapDataRecorder.setOptionBool("stereo_recording", stereoRecording)) {
        std::cerr << "Warning: Failed to set stereo_recording option" << std::endl;
    }
    
    if (!sdk->colmapDataRecorder.setOptionBool("undistort", undistort)) {
        std::cerr << "Warning: Failed to set undistort option" << std::endl;
    }
    
    if (!sdk->colmapDataRecorder.setOptionBool("undistort_force_focal_center", forceFocalCenter)) {
        std::cerr << "Warning: Failed to set undistort_force_focal_center option" << std::endl;
    }
    
    if (!sdk->colmapDataRecorder.setOptionBool("keep_unused_map_points", keepUnusedPoints)) {
        std::cerr << "Warning: Failed to set keep_unused_map_points option" << std::endl;
    }
    
    if (!sdk->colmapDataRecorder.setOptionBool("multi_mapper", multiMapper)) {
        std::cerr << "Warning: Failed to set multi_mapper option" << std::endl;
    }
    
    if (!sdk->colmapDataRecorder.setOptionString("file_format", fileFormat)) {
        std::cerr << "Warning: Failed to set file_format option" << std::endl;
    }

    // Start recording
    std::cout << "Starting colmap dataset recording..." << std::endl;
    if (!sdk->colmapDataRecorder.startRecording(outputFolder)) {
        std::cerr << "Failed to start colmap dataset recording" << std::endl;
        sdk->stopBackgroundMapDataSyncing();
        sdk->disconnect();
        sdk->release();
        return 1;
    }

    // Check if recording is actually running
    if (!sdk->colmapDataRecorder.isRecording()) {
        std::cerr << "Recording failed to start properly" << std::endl;
        sdk->stopBackgroundMapDataSyncing();
        sdk->disconnect();
        sdk->release();
        return 1;
    }

    std::cout << "Recording started successfully" << std::endl;
    if (timeout_seconds > 0) {
        std::cout << "Recording will stop automatically after " << timeout_seconds << " seconds" << std::endl;
    }
    std::cout << "Press Ctrl+C to stop recording" << std::endl;

    // Recording loop
    auto startTime = std::chrono::steady_clock::now();
    while (!isCtrlC && sdk->colmapDataRecorder.isRecording()) {
        // Check timeout
        if (timeout_seconds > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - startTime).count();
            if (elapsed >= timeout_seconds) {
                std::cout << "Timeout reached, stopping recording..." << std::endl;
                break;
            }
        }

        // Query and print current status (keyframe count)
        int64_t keyframeCount = 0;
        if (sdk->colmapDataRecorder.queryStatusInt64("kf_count", &keyframeCount)) {
            std::cout << "Current keyframe count: " << keyframeCount << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Stop recording
    std::cout << "Stopping recording..." << std::endl;
    sdk->colmapDataRecorder.stopRecording();


    std::cout << "Recording stopped successfully" << std::endl;

    // Cleanup
    sdk->disconnect();
    sdk->release();
    
    std::cout << "Dataset saved to: " << outputFolder << std::endl;
    return 0;
}