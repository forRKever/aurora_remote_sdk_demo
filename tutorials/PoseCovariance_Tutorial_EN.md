# Aurora Remote SDK - Pose Covariance Tutorial

## Overview

The Aurora Remote SDK provides pose covariance data that represents the uncertainty of the device's localization. This information is crucial for applications that need to assess the reliability of pose estimates and make decisions based on localization confidence.

## What is Pose Covariance?

### Mathematical Definition

Pose covariance is a 6×6 symmetric positive semi-definite matrix that describes the uncertainty and correlations in the 6-DOF pose estimate:
- 3 degrees of freedom for position (x, y, z)
- 3 degrees of freedom for orientation (typically represented as axis-angle or quaternion internally)

The covariance matrix captures:
1. **Variance**: Diagonal elements represent uncertainty in each dimension
2. **Correlation**: Off-diagonal elements show how errors in different dimensions are related

### Practical Interpretation

The SDK provides convenient functions to convert the raw covariance matrix into human-readable metrics:

#### Position Uncertainty
- **95% Confidence Ellipsoid**: A 3D ellipsoid where the true position is expected to lie 95% of the time
- **XY Plane Radius**: The radius of the 95% confidence circle projected onto the horizontal plane
- **Semi-axes**: The three principal axes of the uncertainty ellipsoid

#### Rotation Uncertainty
- **1-Sigma RPY**: One standard deviation (68% confidence) for Roll, Pitch, and Yaw angles

## Quick Start Guide

### Step 1: Include Headers

For C++ applications:
```cpp
#include "aurora_pubsdk_inc.h"
using namespace rp::standalone::aurora;
```

For C applications:
```c
#include "aurora_pubsdk_inc.h"
```

### Step 2: Retrieve Pose Covariance

#### Using Polling API (C++)

```cpp
// Get the most recent pose covariance
PoseCovariance covariance;
uint64_t timestamp = 0;

if (sdk->dataProvider.getRecentPoseCovariance(covariance, &timestamp)) {
    std::cout << "Retrieved covariance at timestamp: " << timestamp << " ns" << std::endl;
    // Process covariance...
} else {
    std::cerr << "Covariance not available" << std::endl;
}
```

#### Using Callback API (C++)

```cpp
class MyListener : public RemoteSDKListener {
public:
    void onPoseCovariance(uint64_t timestamp_ns,
                         const PoseCovariance& covariance) override {
        // This callback is called whenever new covariance data arrives
        std::cout << "New covariance at " << timestamp_ns << " ns" << std::endl;
        processCovariance(covariance);
    }
};

// Create SDK session with listener
MyListener listener;
auto sdk = RemoteSDK::CreateSession(&listener);
```

#### Using C API

```c
slamtec_aurora_sdk_pose_covariance_t covariance;
uint64_t timestamp = 0;

slamtec_aurora_sdk_errorcode_t result =
    slamtec_aurora_sdk_get_recent_pose_covariance(
        sdkSession,
        &covariance,
        &timestamp
    );

if (result == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
    printf("Retrieved covariance at timestamp: %llu ns\n", timestamp);
} else {
    printf("Covariance not available\n");
}
```

### Step 3: Convert to Human-Readable Format

#### C++ API

```cpp
PoseCovariance covariance;
uint64_t timestamp;

if (sdk->dataProvider.getRecentPoseCovariance(covariance, &timestamp)) {
    // Convert to readable format
    PoseCovarianceReadable readable;
    if (covariance.toHumanReadable(readable)) {
        // Get position uncertainty (95% confidence)
        auto ellipsoid = readable.getPositionEllipsoid95();
        std::cout << "Position 95% ellipsoid (m): ["
                  << ellipsoid[0] << ", "
                  << ellipsoid[1] << ", "
                  << ellipsoid[2] << "]" << std::endl;

        // Get XY plane radius
        double radius_xy = readable.getPositionRadius95XY();
        std::cout << "XY radius (m): " << radius_xy << std::endl;

        // Get rotation uncertainty (1-sigma)
        auto rpy = readable.getRotation1SigmaRPY();
        std::cout << "Rotation 1-sigma RPY (deg): ["
                  << rpy[0] << ", "
                  << rpy[1] << ", "
                  << rpy[2] << "]" << std::endl;
    }
}
```

#### C API

```c
slamtec_aurora_sdk_pose_covariance_t covariance;
slamtec_aurora_sdk_pose_covariance_readable_t readable;
uint64_t timestamp;

if (slamtec_aurora_sdk_get_recent_pose_covariance(session, &covariance, &timestamp)
    == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {

    // Convert to readable format
    if (slamtec_aurora_sdk_pose_covariance_to_readable(&covariance, &readable)
        == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {

        printf("Position 95%% ellipsoid (m): [%.4f, %.4f, %.4f]\n",
               readable.position_ellipsoid_95[0],
               readable.position_ellipsoid_95[1],
               readable.position_ellipsoid_95[2]);

        printf("XY radius (m): %.4f\n", readable.position_radius_95_xy);

        printf("Rotation 1-sigma RPY (deg): [%.2f, %.2f, %.2f]\n",
               readable.rotation_1sigma_rpy[0],
               readable.rotation_1sigma_rpy[1],
               readable.rotation_1sigma_rpy[2]);
    }
}
```

## Understanding the Metrics

### Position Metrics

#### 95% Confidence Ellipsoid
The three semi-axes define an ellipsoid in 3D space:

```
Example: [0.0234, 0.0198, 0.0456] meters

Interpretation:
- Semi-axis 1: ±2.34 cm (95% confidence)
- Semi-axis 2: ±1.98 cm (95% confidence)
- Semi-axis 3: ±4.56 cm (95% confidence)
```

The ellipsoid is oriented along the principal axes of uncertainty, which may not align with the X, Y, Z axes.

#### XY Plane Radius (95%)
For many robotics applications, horizontal positioning is most critical:

```
Example: 0.0298 meters

Interpretation:
- 95% confident the true position is within a 3cm radius circle
- Useful threshold: < 5cm is excellent, < 10cm is good, > 30cm is poor
```

### Rotation Metrics

#### 1-Sigma RPY (Roll-Pitch-Yaw)
One standard deviation for each rotation axis:

```
Example: [0.45, 0.52, 1.23] degrees

Interpretation:
- ~68% confident roll error is within ±0.45°
- ~68% confident pitch error is within ±0.52°
- ~68% confident yaw error is within ±1.23°
```

Note: Yaw (heading) uncertainty is often higher than roll/pitch.

## Common Use Cases

### Use Case 1: Adaptive Robot Behavior

Adjust robot speed and behavior based on localization confidence:

```cpp
void adaptRobotBehavior(const PoseCovariance& covariance) {
    PoseCovarianceReadable readable;
    covariance.toHumanReadable(readable);

    double pos_uncertainty = readable.getPositionRadius95XY();

    if (pos_uncertainty > 0.30) {
        // High uncertainty - take corrective action
        robot.slowDown();
        robot.increaseMapObservation();

        // Consider relocalization if uncertainty persists
        if (consecutiveHighUncertainty > 50) {
            robot.triggerRelocalization();
        }
    } else if (pos_uncertainty > 0.10) {
        // Moderate uncertainty - proceed with caution
        robot.normalSpeed();
    } else {
        // Low uncertainty - full speed ahead
        robot.fullSpeed();
    }
}
```

### Use Case 2: Goal Reaching Threshold

Determine when robot has reached a navigation goal:

```cpp
bool hasReachedGoal(const RemotePoseSE3& current_pose,
                   const RemotePoseSE3& goal_pose,
                   const PoseCovariance& covariance) {
    // Calculate distance to goal
    double distance = calculateDistance(current_pose, goal_pose);

    // Get position uncertainty
    PoseCovarianceReadable readable;
    covariance.toHumanReadable(readable);
    double uncertainty = readable.getPositionRadius95XY();

    // Goal threshold should account for uncertainty
    double threshold = 0.10 + 2.0 * uncertainty;  // 10cm + 2x uncertainty

    return distance < threshold;
}
```

### Use Case 3: Data Quality Monitoring

Log and monitor localization quality over time:

```cpp
class QualityMonitor : public RemoteSDKListener {
private:
    std::ofstream log_file;

public:
    void onPoseCovariance(uint64_t timestamp_ns,
                         const PoseCovariance& covariance) override {
        PoseCovarianceReadable readable;
        covariance.toHumanReadable(readable);

        // Log metrics for analysis
        log_file << timestamp_ns << ","
                << readable.getPositionRadius95XY() << ","
                << readable.getRotation1SigmaRPY()[2] << std::endl;  // Yaw

        // Alert if quality degrades
        if (readable.getPositionRadius95XY() > 0.50) {
            alertOperator("Warning: High position uncertainty detected!");
        }
    }
};
```

### Use Case 4: Multi-Sensor Fusion

Weight Aurora pose in sensor fusion based on covariance:

```cpp
void fuseWithOtherSensors(const RemotePoseSE3& aurora_pose,
                         const PoseCovariance& aurora_cov,
                         const Pose& wheel_odom_pose,
                         double wheel_odom_variance) {
    PoseCovarianceReadable readable;
    aurora_cov.toHumanReadable(readable);

    // Calculate weights based on uncertainties
    double aurora_uncertainty = readable.getPositionRadius95XY();
    double aurora_weight = 1.0 / (aurora_uncertainty * aurora_uncertainty);
    double wheel_weight = 1.0 / wheel_odom_variance;

    // Weighted average (simplified)
    double total_weight = aurora_weight + wheel_weight;
    Pose fused_pose;
    fused_pose.x = (aurora_pose.translation.x * aurora_weight +
                    wheel_odom_pose.x * wheel_weight) / total_weight;
    // ... similar for other dimensions
}
```

## Factors Affecting Covariance

### Environmental Factors

1. **Visual Features**
   - Rich texture → Lower uncertainty
   - Texture-less walls → Higher uncertainty
   - Repetitive patterns → May increase uncertainty

2. **Lighting Conditions**
   - Good lighting → Lower uncertainty
   - Low light → Higher uncertainty
   - Changing lighting → May temporarily increase uncertainty

3. **Scene Geometry**
   - 3D structure → Lower uncertainty
   - Planar surfaces → Higher uncertainty in perpendicular direction

### Motion Factors

1. **Speed**
   - Slow motion → Lower uncertainty
   - Fast motion → Higher uncertainty
   - Sudden movements → Temporarily increased uncertainty

2. **Rotation**
   - Pure translation → Good position, may have rotation uncertainty
   - Pure rotation → May temporarily increase position uncertainty
   - Combined motion → Balanced uncertainty

### System State

1. **Tracking Duration**
   - Recently started → Higher uncertainty
   - Long stable tracking → Lower uncertainty
   - After relocalization → Initially higher, then improves

2. **Map Quality**
   - Well-mapped area → Lower uncertainty
   - Unexplored area → Higher uncertainty
   - Recently mapped → Moderate uncertainty

## Quality Guidelines

### Position Quality Classification

| XY Radius (95%) | Quality | Suitable For |
|----------------|---------|--------------|
| < 0.05 m (5 cm) | Excellent | Precision docking, manipulation |
| 0.05 - 0.10 m | Good | General navigation, door passing |
| 0.10 - 0.30 m | Fair | Coarse navigation |
| > 0.30 m (30 cm) | Poor | Consider relocalization |

### Rotation Quality Classification

| Max RPY (1σ) | Quality | Suitable For |
|--------------|---------|--------------|
| < 1° | Excellent | Precise orientation tasks |
| 1° - 3° | Good | General navigation |
| 3° - 5° | Fair | Coarse positioning |
| > 5° | Poor | May need relocalization |

## Advanced Topics

### Converting to Eigen Matrix (C++ with Eigen)

If your application uses Eigen:

```cpp
#ifdef EIGEN_WORLD_VERSION
PoseCovariance covariance;
Eigen::Matrix<float, 6, 6> eigen_cov;

if (covariance.toEigenMatrix(eigen_cov)) {
    // Now you can use Eigen operations
    std::cout << "Covariance determinant: " << eigen_cov.determinant() << std::endl;
    std::cout << "Covariance trace: " << eigen_cov.trace() << std::endl;

    // Can be used in Kalman filter or other estimation algorithms
}
#endif
```

### Covariance Update Frequency

- Covariance updates follow visual tracking frequency (~10-15 Hz)
- Each visual frame produces an updated covariance estimate
- Unlike pose augmentation, covariance is not interpolated at high frequency

### Thread Safety

The polling API (`getRecentPoseCovariance`) is thread-safe and can be called from any thread. The callback (`onPoseCovariance`) is called from an internal SDK thread.

## Troubleshooting

### Covariance Not Available

**Problem**: `getRecentPoseCovariance()` returns error or false

**Solutions**:
1. Check firmware version (requires 2.1.1 or later)
2. Ensure device is actively tracking (not in lost state)
3. Wait a few seconds after connection for data to accumulate
4. Verify device supports covariance (older firmware may not)

### High Uncertainty Values

**Problem**: Position uncertainty consistently > 0.30m

**Solutions**:
1. Improve environment:
   - Add visual features/markers
   - Improve lighting conditions
   - Avoid highly reflective surfaces
2. Trigger relocalization: `sdk->controller.requireRelocalization()`
3. Check device calibration
4. Verify map quality

### Covariance Spikes

**Problem**: Occasional high uncertainty values

**Solutions**:
1. Expected during:
   - Fast motion
   - Rapid rotation
   - Tracking recovery
2. Filter covariance over time window if needed
3. Consider motion planning to avoid aggressive movements

### Inconsistent with Observed Error

**Problem**: Reported uncertainty doesn't match actual positioning error

**Possible Causes**:
1. Ground truth measurement error
2. Coordinate system mismatch
3. Timestamp synchronization issues
4. Map-to-world transformation not accounted for

## API Reference Summary

### C++ API

```cpp
// Polling API
bool getRecentPoseCovariance(PoseCovariance& covariance, uint64_t* timestamp);

// Callback API
virtual void onPoseCovariance(uint64_t timestamp_ns, const PoseCovariance& covariance);

// Conversion functions
bool toHumanReadable(PoseCovarianceReadable& readable) const;
bool toEigenMatrix(Eigen::Matrix<float, 6, 6>& matrix) const;  // If Eigen available

// Readable metrics
std::array<double, 3> getPositionEllipsoid95() const;
double getPositionRadius95XY() const;
std::array<double, 3> getRotation1SigmaRPY() const;
```

### C API

```c
// Retrieve covariance
slamtec_aurora_sdk_errorcode_t slamtec_aurora_sdk_get_recent_pose_covariance(
    slamtec_aurora_sdk_session_handle_t session,
    slamtec_aurora_sdk_pose_covariance_t* covariance,
    uint64_t* timestamp
);

// Convert to readable format
slamtec_aurora_sdk_errorcode_t slamtec_aurora_sdk_pose_covariance_to_readable(
    const slamtec_aurora_sdk_pose_covariance_t* covariance,
    slamtec_aurora_sdk_pose_covariance_readable_t* readable
);

// Callback setup (in listener struct)
void (*on_pose_covariance)(
    void* user_data,
    uint64_t timestamp_ns,
    const slamtec_aurora_sdk_pose_covariance_t* covariance
);
```

## Related Features

- **Pose Retrieval**: Get the actual pose estimate along with its covariance
- **Relocalization**: Trigger when uncertainty is too high
- **Time Synchronization**: Ensure covariance timestamps are correctly interpreted
- **Pose Augmentation**: High-frequency pose updates (though covariance is at visual rate)

## Additional Resources

- [Demo Code](../demo/pose_covariance/README.md)
- [API Reference](../doc/html/index.html)
- [Simple Pose Demo](../demo/simple_pose/README.md)
- [Relocalization Demo](../demo/relocalization/README.md)
