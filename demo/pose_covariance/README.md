# Pose Covariance Demo

## Overview

This demo demonstrates how to retrieve and interpret pose covariance data from the Aurora device. Pose covariance provides uncertainty estimates for the device's position and orientation, which is crucial for applications that need to assess localization confidence.

## Features

- **Position uncertainty**: 95% confidence ellipsoid for position estimates
- **Rotation uncertainty**: 1-sigma uncertainty in roll, pitch, and yaw
- **Quality assessment**: Automatic classification of localization quality
- **Both polling and callback APIs**: Flexible data retrieval methods
- **Human-readable format**: Easy-to-interpret covariance metrics

## Prerequisites

- Aurora device connected to network
- Aurora firmware version 2.1.1 or later (with covariance support)
- Device must be running and actively tracking

## Building

This demo is built automatically with the other demos:

```bash
cd build
cmake ..
make pose_covariance
```

## Usage

### Auto-Discovery Mode

```bash
./pose_covariance
```

This will automatically discover and connect to an Aurora device on the network.

### Specify Device Address

```bash
./pose_covariance 192.168.1.100
```

### Help

```bash
./pose_covariance --help
```

## Understanding Pose Covariance

### What is Pose Covariance?

Pose covariance is a 6x6 matrix that represents the uncertainty in the 6-DOF pose (3D position + 3D orientation). It captures:
- How uncertain each degree of freedom is
- Correlations between different degrees of freedom

The SDK provides convenient functions to convert this matrix into human-readable metrics.

### Position Uncertainty

**95% Confidence Ellipsoid**: Defines a 3D ellipsoid where the true position is expected to be 95% of the time.

- **Semi-axes**: The three principal axes of the ellipsoid (in meters)
- **XY Radius**: The radius of the 95% confidence circle in the XY plane

Example:
```
Position 95% Ellipsoid (m): [0.0234, 0.0198, 0.0456]
Position 95% Radius XY (m): 0.0298
```

This means:
- We're 95% confident the position is within a 3cm radius in the XY plane
- Vertical uncertainty is slightly higher at ~4.5cm

### Rotation Uncertainty

**1-Sigma RPY**: One standard deviation of uncertainty for each rotation axis (in degrees).

Example:
```
Rotation 1-sigma (deg): [0.45, 0.52, 1.23]
```

This means:
- ~68% chance the actual roll is within ±0.45° of the estimate
- ~68% chance the actual pitch is within ±0.52° of the estimate
- ~68% chance the actual yaw is within ±1.23° of the estimate

### Quality Guidelines

#### Position Quality
- **EXCELLENT** (< 5 cm): Suitable for precision navigation and manipulation
- **GOOD** (< 10 cm): Suitable for general robotics navigation
- **FAIR** (< 30 cm): Suitable for coarse positioning
- **POOR** (> 30 cm): Localization may be unreliable

#### Rotation Quality
- **EXCELLENT** (< 1°): Suitable for precise orientation-dependent tasks
- **GOOD** (< 3°): Suitable for general navigation
- **FAIR** (< 5°): Acceptable for coarse orientation
- **POOR** (> 5°): Orientation may be unreliable

## Output Example

```
============================================================
Aurora Pose Covariance Demo
============================================================
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

Step 2: Waiting for device to start tracking...

Step 3: Testing pose covariance polling API...
  ✓ Successfully retrieved pose covariance
    Timestamp: 123456789000 ns
    Position 95% radius (XY): 0.0298 m
    Rotation 1-sigma (RPY): [0.45, 0.52, 1.23] deg

============================================================
Monitoring Pose Covariance Updates
============================================================
The demo will display covariance updates from the callback.
Move the Aurora device to observe how uncertainty changes.

Press Ctrl+C to stop

------------------------------------------------------------
Pose Covariance Update #1
------------------------------------------------------------
Timestamp: 123456789000000 ns

Position Uncertainty (95% confidence):
  Semi-axes (m): [0.0234, 0.0198, 0.0456]
  XY Radius (m): 0.0298

Rotation Uncertainty (1-sigma):
  Roll:   0.4512 deg
  Pitch:  0.5234 deg
  Yaw:    1.2345 deg

Quality Assessment:
  Position: EXCELLENT (< 5 cm)
  Rotation: EXCELLENT (< 1 deg)

------------------------------------------------------------
Pose Covariance Update #2
------------------------------------------------------------
Timestamp: 123456890000000 ns

Position Uncertainty (95% confidence):
  Semi-axes (m): [0.0245, 0.0209, 0.0467]
  XY Radius (m): 0.0312

Rotation Uncertainty (1-sigma):
  Roll:   0.4678 deg
  Pitch:  0.5456 deg
  Yaw:    1.2678 deg

Quality Assessment:
  Position: EXCELLENT (< 5 cm)
  Rotation: EXCELLENT (< 1 deg)

^C
Ctrl-C pressed, stopping...

============================================================
Shutting down...
============================================================

Statistics:
  Total covariance updates: 150
  Runtime:                  15 seconds
  Average update rate:      10.00 Hz

  ✓ Disconnected from device

Demo completed successfully!
```

## Polling vs. Callback API

### Polling API

Use when you need covariance on-demand:

```cpp
PoseCovariance covariance;
uint64_t timestamp = 0;

if (sdk->dataProvider.getRecentPoseCovariance(covariance, &timestamp)) {
    // Convert to readable format
    PoseCovarianceReadable readable;
    if (covariance.toHumanReadable(readable)) {
        auto radius_xy = readable.getPositionRadius95XY();
        // Use radius_xy to assess position quality
    }
}
```

### Callback API

Use for real-time monitoring:

```cpp
class MyListener : public RemoteSDKListener {
    void onPoseCovariance(uint64_t timestamp_ns,
                         const PoseCovariance& covariance) override {
        // Process covariance update
        PoseCovarianceReadable readable;
        covariance.toHumanReadable(readable);
        // ...
    }
};
```

## Use Cases

### Adaptive Navigation

Adjust robot behavior based on localization confidence:

```cpp
void onPoseCovariance(uint64_t timestamp_ns,
                     const PoseCovariance& covariance) {
    PoseCovarianceReadable readable;
    covariance.toHumanReadable(readable);

    double pos_uncertainty = readable.getPositionRadius95XY();

    if (pos_uncertainty > 0.30) {
        // High uncertainty - slow down or trigger relocalization
        robot.reduceSpeed();
        robot.requestRelocalization();
    } else if (pos_uncertainty < 0.10) {
        // Good localization - normal operation
        robot.normalSpeed();
    }
}
```

### Quality Monitoring

Log localization quality for analysis:

```cpp
void logLocalizationQuality(const PoseCovariance& covariance) {
    PoseCovarianceReadable readable;
    covariance.toHumanReadable(readable);

    logger.log("pos_uncertainty_xy", readable.getPositionRadius95XY());
    auto rpy = readable.getRotation1SigmaRPY();
    logger.log("rot_uncertainty", rpy[0], rpy[1], rpy[2]);
}
```

### Sensor Fusion

Weight Aurora pose in multi-sensor fusion:

```cpp
void fuseSensors(const PoseCovariance& aurora_cov,
                const Pose& other_sensor_pose) {
    PoseCovarianceReadable readable;
    aurora_cov.toHumanReadable(readable);

    double aurora_weight = 1.0 / readable.getPositionRadius95XY();
    // Use weight in Kalman filter or other fusion algorithm
}
```

## Factors Affecting Covariance

### Environment
- **Rich features**: Lower uncertainty
- **Texture-less areas**: Higher uncertainty
- **Dynamic objects**: May increase uncertainty

### Motion
- **Slow, smooth motion**: Lower uncertainty
- **Fast, jerky motion**: Higher uncertainty
- **Rotation**: Can increase rotational uncertainty

### Tracking State
- **Initial tracking**: Higher uncertainty
- **Well-localized**: Lower uncertainty
- **Recently lost and recovered**: Temporarily higher uncertainty

## Troubleshooting

### Covariance Not Available
- Check firmware version (requires 2.1.1+)
- Ensure device is actively tracking (not lost)
- Wait a few seconds after connection for data to accumulate

### High Uncertainty Values
- Check environment has sufficient visual features
- Verify device is properly calibrated
- Consider triggering relocalization if uncertainty remains high

### Covariance Updates Infrequent
- Covariance updates follow visual tracking frequency (10-15 Hz)
- This is expected behavior (unlike pose augmentation which is higher frequency)

## API Reference

For detailed API documentation, refer to:
- [Pose Covariance Tutorial](../../doc/PoseCovariance_Tutorial_EN.md)
- [API Reference](../../doc/html/index.html)

## Related Demos

- [Simple Pose](../simple_pose/README.md) - Basic pose retrieval
- [Relocalization](../relocalization/README.md) - Trigger relocalization when uncertainty is high
- [Pose Augmentation](../pose_augmentation/README.md) - High-frequency pose updates
