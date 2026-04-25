# Aurora Remote SDK - Pose Augmentation Tutorial

## Overview

The Aurora Remote SDK provides a **Pose Augmentation** feature that delivers high-frequency pose output by integrating IMU (Inertial Measurement Unit) measurements between visual tracking updates. This feature enables applications to obtain pose estimates at frequencies up to 200Hz, significantly higher than the typical visual tracking rate of 10-20Hz.

## How It Works

### Basic Principle

Visual-Inertial SLAM systems like Aurora typically track camera poses at the camera frame rate (10-20Hz). Between these visual tracking updates, the Pose Augmentation module uses **IMU pre-integration** to predict intermediate poses:

1. **Visual Tracking Base**: The visual SLAM system provides accurate but lower-frequency pose estimates along with IMU bias estimates and velocity.

2. **IMU Pre-integration**: Between visual updates, the system integrates IMU measurements (acceleration and gyroscope) to predict the device's motion.

3. **High-Frequency Output**: Augmented poses are published at high frequency (50Hz, 100Hz, or 200Hz) combining visual accuracy with IMU responsiveness.

4. **Optional Smoothing**: An exponential moving average filter can be applied to reduce high-frequency noise while maintaining responsiveness.

### Operation Modes

The Pose Augmentation feature supports two modes:

- **VISUAL_ONLY**: Only outputs visual tracking poses (10-20Hz). No IMU augmentation is performed.
- **IMU_VISION_MIXED**: Outputs IMU-augmented poses at high frequency (50-200Hz). This is the recommended mode for most applications.

## Benefits and Drawbacks

### Benefits

✅ **High Frequency Output**: Get pose updates at 50Hz, 100Hz, or 200Hz instead of 10-20Hz

✅ **Low Latency**: IMU measurements are processed immediately, providing minimal delay

✅ **Smooth Motion Tracking**: Ideal for real-time applications like robotics control, AR/VR, or smooth visualization

✅ **Continuous Output**: Maintains pose output even during brief visual tracking interruptions

### Drawbacks

⚠️ **IMU Drift**: IMU-based predictions accumulate errors over time. The system is corrected by visual updates, but between updates, drift can occur.

⚠️ **Noise**: Raw IMU data can be noisy, leading to jitter in the augmented pose. Use the smoothing feature to mitigate this.

⚠️ **Computational Cost**: IMU integration runs in a background thread and consumes additional CPU resources.

⚠️ **Requires IMU**: This feature only works on devices equipped with an IMU sensor.

### When to Use Pose Augmentation

**Use Pose Augmentation when:**
- You need high-frequency pose updates (>30Hz)
- Your application requires low-latency pose estimates
- You're controlling a robot or drone in real-time
- You're implementing AR/VR applications requiring smooth motion

**Don't use Pose Augmentation when:**
- Your application only needs 10-20Hz pose updates
- You want to minimize CPU usage
- You prioritize absolute accuracy over responsiveness
- Your device doesn't have an IMU sensor

## Quick Start Guide

### Step 1: Include the Header

For C++ applications:
```cpp
#include "aurora_pubsdk_inc.h"
using namespace rp::standalone::aurora;
```

For C applications:
```c
#include "aurora_pubsdk_inc.h"
```

### Step 2: Create Session and Connect

#### C++ API (Recommended)

```cpp
// Create a listener to receive pose augmentation callbacks
class MyListener : public RemoteSDKListener {
public:
    void onPoseAugmentationResult(uint64_t timestamp_ns,
                                   slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                                   const slamtec_aurora_sdk_pose_se3_t& pose) override {
        // Handle high-frequency pose updates here
        std::cout << "Pose at " << timestamp_ns << ": ("
                  << pose.translation.x << ", "
                  << pose.translation.y << ", "
                  << pose.translation.z << ")" << std::endl;
    }
};

// Create SDK session
MyListener listener;
auto sdk = RemoteSDK::CreateSession(&listener);

// Connect to Aurora device
slamtec_aurora_sdk_server_connection_info_t info{};
strncpy(info.connection_info[0].address, "192.168.11.1",
        sizeof(info.connection_info[0].address) - 1);
strncpy(info.connection_info[0].protocol_type,
        SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_PROTOCOL,
        sizeof(info.connection_info[0].protocol_type) - 1);
info.connection_info[0].port = SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_PORT;
info.connection_count = 1;

sdk->controller.connect(info);
```

#### C API

```c
// Callback function for pose augmentation results
void on_pose_result(void* user_data, uint64_t timestamp_ns,
                    slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                    const slamtec_aurora_sdk_pose_se3_t* pose) {
    printf("Pose at %llu: (%f, %f, %f)\n",
           timestamp_ns, pose->translation.x,
           pose->translation.y, pose->translation.z);
}

// Setup listener
slamtec_aurora_sdk_listener_t listener = {0};
listener.on_pose_augmentation_result = on_pose_result;

// Create session and connect (same as C++)
slamtec_aurora_sdk_session_handle_t session;
slamtec_aurora_sdk_create_session(&listener, NULL, &session);

slamtec_aurora_sdk_server_connection_info_t info = {0};
// ... (setup connection info same as C++)
slamtec_aurora_sdk_controller_connect(session, &info);
```

### Step 3: Configure and Start Pose Augmentation

#### C++ API

```cpp
// Configure pose augmentation
slamtec_aurora_sdk_pose_augmentation_config_t config{};
config.output_frequency = SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ;
config.enable_smoothing = 0;  // Disable smoothing (default)
config.smoothing_factor = 0.3f;  // Only used if smoothing enabled

// Start pose augmentation
if (!sdk->dataProvider.startPoseAugmentation(
        SLAMTEC_AURORA_SDK_POSE_AUGMENTATION_MODE_IMU_VISION_MIXED,
        config)) {
    std::cerr << "Failed to start pose augmentation!" << std::endl;
}
```

#### C API

```c
slamtec_aurora_sdk_pose_augmentation_config_t config;
config.output_frequency = SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ;
config.enable_smoothing = 0;
config.smoothing_factor = 0.3f;

slamtec_aurora_sdk_errorcode_t result =
    slamtec_aurora_sdk_dataprovider_start_pose_augmentation(
        session,
        SLAMTEC_AURORA_SDK_POSE_AUGMENTATION_MODE_IMU_VISION_MIXED,
        &config);

if (result != SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
    printf("Failed to start pose augmentation: %d\n", result);
}
```

### Step 4: Receive Pose Updates

Pose updates are delivered through the `onPoseAugmentationResult` callback you registered in Step 2. The callback is invoked from a background thread at the configured frequency (50Hz, 100Hz, or 200Hz).

### Step 5: Stop Pose Augmentation

When you're done:

#### C++ API
```cpp
sdk->dataProvider.stopPoseAugmentation();
```

#### C API
```c
slamtec_aurora_sdk_dataprovider_stop_pose_augmentation(session);
```

## Configuration Options

### Output Frequency

Choose the pose output frequency based on your application needs:

| Frequency | Use Case | CPU Usage |
|-----------|----------|-----------|
| `POSE_OUTPUT_FREQ_50HZ` | Basic real-time applications | Low |
| `POSE_OUTPUT_FREQ_100HZ` | Standard robotics control | Medium |
| `POSE_OUTPUT_FREQ_200HZ` | High-performance AR/VR, drones | High |
| `POSE_OUTPUT_FREQ_HIGHEST_POSSIBLE` | Maximum frequency (typically 200Hz+) | Highest |

### Pose Smoothing

Pose smoothing uses an exponential moving average (EMA) to reduce high-frequency noise in the IMU-augmented pose output.

**Enable smoothing when:**
- You observe jitter or noise in the pose output
- Your application prefers smooth motion over instantaneous updates
- You're visualizing the pose trajectory

**Disable smoothing when:**
- You need the most responsive pose updates
- Your application performs its own filtering
- Latency is more critical than smoothness

**Smoothing Factor (alpha):**
- Range: `0.0` to `1.0`
- Higher values (e.g., `0.7` - `1.0`): More responsive, less smooth
- Lower values (e.g., `0.1` - `0.3`): More smooth, less responsive
- Recommended default: `0.3`

**Example with smoothing enabled:**

```cpp
config.enable_smoothing = 1;  // Enable smoothing
config.smoothing_factor = 0.5f;  // Moderate smoothing
```

## Complete Example

Here's a complete working example:

```cpp
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include "aurora_pubsdk_inc.h"

using namespace rp::standalone::aurora;

std::atomic<bool> g_running{true};
std::atomic<int> g_pose_count{0};

class PoseListener : public RemoteSDKListener {
public:
    void onPoseAugmentationResult(uint64_t timestamp_ns,
                                   slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                                   const slamtec_aurora_sdk_pose_se3_t& pose) override {
        int count = ++g_pose_count;

        // Display every 20th pose (200Hz -> 10Hz display)
        if (count % 20 == 0) {
            std::cout << "Pose #" << count << " at " << timestamp_ns << " ns: ("
                      << pose.translation.x << ", "
                      << pose.translation.y << ", "
                      << pose.translation.z << ")" << std::endl;
        }
    }
};

int main(int argc, char** argv) {
    // Create session
    PoseListener listener;
    auto sdk = RemoteSDK::CreateSession(&listener);

    // Connect to device
    std::string address = (argc > 1) ? argv[1] : "192.168.11.1";
    slamtec_aurora_sdk_server_connection_info_t info{};
    strncpy(info.connection_info[0].address, address.c_str(),
            sizeof(info.connection_info[0].address) - 1);
    strncpy(info.connection_info[0].protocol_type,
            SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_PROTOCOL,
            sizeof(info.connection_info[0].protocol_type) - 1);
    info.connection_info[0].port = SLAMTEC_AURORA_SDK_REMOTE_SERVER_DEFAULT_PORT;
    info.connection_count = 1;

    if (!sdk->controller.connect(info)) {
        std::cerr << "Failed to connect!" << std::endl;
        return -1;
    }

    std::cout << "Connected to Aurora device at " << address << std::endl;

    // Configure and start pose augmentation
    slamtec_aurora_sdk_pose_augmentation_config_t config{};
    config.output_frequency = SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ;
    config.enable_smoothing = 1;  // Enable smoothing
    config.smoothing_factor = 0.3f;  // Light smoothing

    if (!sdk->dataProvider.startPoseAugmentation(
            SLAMTEC_AURORA_SDK_POSE_AUGMENTATION_MODE_IMU_VISION_MIXED,
            config)) {
        std::cerr << "Failed to start pose augmentation!" << std::endl;
        return -1;
    }

    std::cout << "Pose augmentation started at 200Hz with smoothing" << std::endl;
    std::cout << "Press Ctrl+C to stop..." << std::endl;

    // Run for 30 seconds
    auto start = std::chrono::steady_clock::now();
    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 30) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Stop and cleanup
    sdk->dataProvider.stopPoseAugmentation();
    std::cout << "\nTotal poses received: " << g_pose_count.load() << std::endl;
    sdk->controller.disconnect();

    return 0;
}
```

## Polling vs Callback

In addition to receiving pose updates via callback, you can also poll for the latest augmented pose:

```cpp
// Poll for current augmented pose
slamtec_aurora_sdk_pose_se3_t pose;
uint64_t timestamp;
if (sdk->dataProvider.getAugmentedPose(pose, &timestamp)) {
    std::cout << "Current pose: (" << pose.translation.x << ", "
              << pose.translation.y << ", " << pose.translation.z << ")"
              << std::endl;
}
```

**Note:** Polling returns the most recently computed pose. For continuous high-frequency updates, use the callback approach.

## Troubleshooting

### No Pose Updates Received

**Problem:** Callback is not being invoked

**Solutions:**
1. Ensure your device has an IMU sensor
2. Verify the connection is established
3. Check that visual tracking is working (device must be initialized)
4. Ensure the callback is registered before starting pose augmentation

### Noisy or Jittery Poses

**Problem:** Pose output has high-frequency jitter

**Solutions:**
1. Enable pose smoothing: `config.enable_smoothing = 1`
2. Adjust smoothing factor (try `0.3` - `0.5`)
3. Reduce output frequency to 100Hz or 50Hz
4. Check for vibrations affecting the IMU sensor

### High CPU Usage

**Problem:** Pose augmentation uses too much CPU

**Solutions:**
1. Reduce output frequency (use 50Hz or 100Hz instead of 200Hz)
2. Use `VISUAL_ONLY` mode if high frequency is not needed
3. Disable smoothing to reduce computational overhead

### Pose Drift

**Problem:** Augmented pose drifts away from actual position

**Solutions:**
1. Ensure visual tracking is functioning properly
2. Check that IMU calibration is correct
3. Verify device firmware is up to date
4. This is expected behavior during brief visual tracking loss; drift is corrected when visual tracking resumes

## API Reference

### Configuration Structure

```c
typedef struct _slamtec_aurora_sdk_pose_augmentation_config_t {
    slamtec_aurora_sdk_pose_output_frequency_t output_frequency;
    int enable_smoothing;      // 0: disabled, non-zero: enabled
    float smoothing_factor;    // 0.0 - 1.0 (default: 0.3)
} slamtec_aurora_sdk_pose_augmentation_config_t;
```

### Functions

**Start Pose Augmentation:**
```c
// C++ API
bool startPoseAugmentation(
    slamtec_aurora_sdk_pose_augmentation_mode_t mode,
    const slamtec_aurora_sdk_pose_augmentation_config_t& config,
    slamtec_aurora_sdk_errorcode_t* errcode = nullptr);

// C API
slamtec_aurora_sdk_errorcode_t slamtec_aurora_sdk_dataprovider_start_pose_augmentation(
    slamtec_aurora_sdk_session_handle_t handle,
    slamtec_aurora_sdk_pose_augmentation_mode_t mode,
    const slamtec_aurora_sdk_pose_augmentation_config_t* config);
```

**Stop Pose Augmentation:**
```c
// C++ API
bool stopPoseAugmentation(slamtec_aurora_sdk_errorcode_t* errcode = nullptr);

// C API
slamtec_aurora_sdk_errorcode_t slamtec_aurora_sdk_dataprovider_stop_pose_augmentation(
    slamtec_aurora_sdk_session_handle_t handle);
```

**Get Augmented Pose (Polling):**
```c
// C++ API
bool getAugmentedPose(slamtec_aurora_sdk_pose_se3_t& poseOut,
                      uint64_t* timestamp_ns = nullptr,
                      slamtec_aurora_sdk_errorcode_t* errcode = nullptr);

// C API
slamtec_aurora_sdk_errorcode_t slamtec_aurora_sdk_dataprovider_get_augmented_pose(
    slamtec_aurora_sdk_session_handle_t handle,
    slamtec_aurora_sdk_pose_se3_t* pose_out,
    uint64_t* timestamp_ns);
```

## Best Practices

1. **Start pose augmentation after the device is initialized** and visual tracking has started
2. **Use callbacks for continuous updates**, polling for occasional queries
3. **Enable smoothing for visualization**, disable for real-time control
4. **Choose frequency based on your needs**: 50Hz is sufficient for most applications
5. **Stop pose augmentation** when not needed to save CPU resources
6. **Handle connection loss**: Stop and restart pose augmentation if the device disconnects

## See Also

- Time Synchronization Tutorial - for accurate timestamp correlation
- Tracking Data API - for accessing visual tracking poses
- IMU Data API - for raw IMU measurements

---

**Copyright © 2013-2025 SLAMTEC Co., Ltd. All Rights Reserved.**
