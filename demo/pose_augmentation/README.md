# Pose Augmentation Demo

## Overview

This demo demonstrates how to use the Aurora Remote SDK's pose augmentation feature to achieve high-frequency pose output by integrating IMU data with visual tracking. This increases the pose update rate from the typical 10-15 Hz (visual-only) to 200+ Hz.

## Features

- **High-frequency pose output**: Up to 200 Hz pose updates
- **IMU-Vision fusion**: Combines visual SLAM with IMU pre-integration
- **Multiple output frequencies**: 50Hz, 100Hz, 200Hz, or highest possible
- **Optional smoothing**: Configurable pose smoothing for applications requiring it
- **Automatic fallback**: Falls back to visual-only mode when IMU data unavailable

## Prerequisites

- Aurora device connected to network
- Aurora firmware version 2.1.1 or later
- Device must be running and tracking successfully

## Building

This demo is built automatically with the other demos:

```bash
cd build
cmake ..
make pose_augmentation
```

## Usage

### Auto-Discovery Mode

```bash
./pose_augmentation
```

This will automatically discover and connect to an Aurora device on the network.

### Specify Device Address

```bash
./pose_augmentation 192.168.1.100
```

### Help

```bash
./pose_augmentation --help
```

## How It Works

### Pose Augmentation Modes

#### Visual Only Mode
- Uses only VSLAM tracking results
- Typical update rate: 10-15 Hz
- Most accurate but lowest frequency

#### IMU-Vision Mixed Mode
- Combines VSLAM poses with IMU pre-integration
- Update rate: Up to 200+ Hz
- Provides smooth, high-frequency pose estimates between visual updates

### Process Flow

1. **Connect to Device**: Establishes connection with Aurora device
2. **Enable IMU Subscription**: Disables low-rate mode to receive IMU data
3. **Configure Augmentation**: Sets desired output frequency and options
4. **Start Augmentation**: Begins background thread for IMU integration
5. **Receive Poses**: Callback receives high-frequency pose updates
6. **Monitor Quality**: Track visual vs. mixed pose statistics

### Configuration Options

```cpp
slamtec_aurora_sdk_pose_augmentation_config_t config;

// Output frequency options:
// - SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_50HZ: 50 Hz output
// - SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_100HZ: 100 Hz output
// - SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ: 200 Hz output
// - SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_HIGHEST_POSSIBLE: Match IMU sample rate
config.output_frequency = SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ;

// Smoothing (optional):
config.enable_smoothing = 0;         // Disable for lowest latency (0=false, 1=true)
config.smoothing_factor = 0.3f;      // 0.0-1.0, only used if enabled
```

## Output Example

```
=============================================
Aurora Pose Augmentation Demo
=============================================
SDK Version: 2.1.0-rtm
Build Date:  2025-01-15 14:23:45

Step 1: Connecting to Aurora device...
  Searching for Aurora devices (5 seconds)...
  Found 1 device(s), connecting to first one...
  ✓ Connected successfully

Device Information:
  Name:         Aurora-12345
  Model:        AURORA_A1
  Serial:       A1-2024-12345
  FW Version:   2.1.0

Step 2: Enabling IMU data subscription...
  ✓ IMU subscription enabled

Step 3: Waiting for IMU data...
  ✓ Received 456 IMU samples

Step 4: Configuring pose augmentation...
  Configuration:
    - Target frequency: 200 Hz
    - Smoothing: Disabled (for lowest latency)
    - Mode: IMU-Vision Mixed

Step 5: Starting pose augmentation...
  ✓ Pose augmentation started

=============================================
Receiving High-Frequency Pose Updates
=============================================
The demo is now receiving pose updates at high frequency.
Move the Aurora device to see pose changes.
(Displaying every 50th pose to avoid flooding console)

Press Ctrl+C to stop

[IMU_MIXED] Pose #50 - Position: (1.234, 0.567, 0.089) m | Timestamp: 123456789000 ns
[IMU_MIXED] Pose #100 - Position: (1.245, 0.578, 0.091) m | Timestamp: 123456789250 ns
[IMU_MIXED] Pose #150 - Position: (1.256, 0.589, 0.093) m | Timestamp: 123456789500 ns

[Statistics after 5s]
  Total poses:    1000
  Visual poses:   75
  Mixed poses:    925
  IMU samples:    2500
  Avg pose rate:  200.0 Hz (last 5s)

[Statistics after 10s]
  Total poses:    2000
  Visual poses:   150
  Mixed poses:    1850
  IMU samples:    5000
  Avg pose rate:  200.0 Hz (last 5s)

^C
Ctrl-C pressed, stopping...

=============================================
Shutting down...
=============================================
  ✓ Stopped pose augmentation

Final Statistics:
  Total poses received: 2000
  Visual poses:         150
  IMU-mixed poses:      1850
  Total IMU samples:    5000
  Runtime:              10 seconds
  Overall pose rate:    200.0 Hz

  ✓ Disconnected from device

Demo completed successfully!
```

## Use Cases

### Robotics Navigation
High-frequency pose updates provide smooth motion control for mobile robots:

```cpp
void onPoseAugmentationResult(uint64_t timestamp_ns,
                              slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                              const slamtec_aurora_sdk_pose_se3_t& pose) {
    // Use high-frequency pose for motion control
    updateRobotController(pose);
}
```

### AR/VR Applications
Low-latency pose updates reduce motion-to-photon delay:

```cpp
// Get latest augmented pose
slamtec_aurora_sdk_pose_se3_t pose;
uint64_t timestamp;
sdk->dataProvider.getAugmentedPose(pose, timestamp);

// Render at display refresh rate (90Hz, 120Hz, etc.)
renderARContent(pose);
```

### Data Logging
Capture high-frequency trajectory data:

```cpp
// Log all pose updates
void onPoseAugmentationResult(uint64_t timestamp_ns,
                              slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                              const slamtec_aurora_sdk_pose_se3_t& pose) {
    logPose(timestamp_ns, mode, pose);
}
```

## Performance Considerations

### IMU Sample Rate
- Typical IMU rate: 200-500 Hz depending on device model
- Pose output frequency cannot exceed IMU rate
- Use `HIGHEST_POSSIBLE` frequency to match IMU rate

### Network Bandwidth
- IMU data subscription increases network traffic
- For bandwidth-constrained networks, consider lower output frequencies
- Visual-only mode uses minimal bandwidth

### CPU Usage
- IMU integration runs on client side
- Higher output frequencies increase CPU usage
- Background thread performs integration, minimal impact on main thread

## Troubleshooting

### No IMU Data Received
- Check firmware version (requires 2.1.1+)
- Ensure low-rate mode is disabled
- Some Aurora models may not support IMU streaming

### Low Pose Update Rate
- Check IMU data is being received
- Verify configuration (use `HIGHEST_POSSIBLE` for maximum rate)
- Network latency can affect IMU data delivery

### Pose Jumps or Discontinuities
- Enable smoothing to reduce jitter
- Check IMU calibration quality
- Verify device is properly mounted and not experiencing excessive vibration

## API Reference

For detailed API documentation, refer to:
- [Pose Augmentation Tutorial](../../doc/PoseAugmentation_Tutorial_EN.md)
- [API Reference](../../doc/html/index.html)

## Related Demos

- [Time Sync](../time_sync/README.md) - Synchronize timestamps
- [Simple Pose](../simple_pose/README.md) - Basic pose retrieval
- [IMU Fetcher](../imu_fetcher/README.md) - Raw IMU data access
