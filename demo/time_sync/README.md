# Time Synchronization Demo

## Overview

This demo demonstrates how to use the Aurora Remote SDK's time synchronization features:

1. **Steady Clock Sync**: Translate timestamps from Aurora device time domain to your local machine's time domain. Essential for correlating Aurora sensor data with external sensors or events.

2. **Wall Clock Sync**: Synchronize the Aurora device's system clock with your local machine's time. Useful for ensuring the device has accurate absolute time for logging, file timestamps, etc.

## Features

- **Steady Clock Synchronization**: Translate Aurora timestamps to local time domain
- **Wall Clock Synchronization**: Sync device system time with client time
- **Sub-millisecond accuracy** under normal network conditions
- **Background synchronization** maintains accuracy over time
- **Quality metrics** to monitor synchronization performance
- **Practical demonstration** - translates real pose and IMU timestamps
- **Live data monitoring** - displays both Aurora and local timestamps side-by-side

## Prerequisites

- Aurora device connected to network
- Aurora firmware version 2.1.1 or later
- Aurora device must be running and accessible

## Building

This demo is built automatically with the other demos:

```bash
cd build
cmake ..
make time_sync
```

## Usage

### Commands

```bash
./time_sync <command> [server_address] [port]
```

| Command | Description |
|---------|-------------|
| `steady` | Run steady clock sync demo (timestamp translation) |
| `wallclock` | Run wall clock sync demo (device time synchronization) |
| `all` | Run both demos (default) |

### Examples

```bash
# Run both demos with auto-discovery
./time_sync

# Run only steady clock sync demo
./time_sync steady

# Run only wall clock sync demo
./time_sync wallclock 192.168.1.100

# Run both demos with specific address and port
./time_sync all 192.168.1.100 9527
```

### Help

```bash
./time_sync --help
```

## How It Works

### Steady Clock Sync Demo

The steady clock demo establishes time synchronization and translates timestamps from pose and IMU data:

1. **Create Client**: Creates a time sync client with `TimeSyncDomain::STEADY_CLOCK`
2. **Connect**: Connects to Aurora device time sync service
3. **Configure**: Sets synchronization options (interval, sample size, etc.)
4. **Initialize**: Performs initial synchronization handshake
5. **Wait for Sync**: Waits until sufficient samples are collected
6. **Display Quality**: Shows synchronization quality metrics (RMSE, max error, etc.)
7. **Translate Data**: Retrieves pose and IMU data and translates timestamps in real-time

### Wall Clock Sync Demo

The wall clock demo synchronizes the Aurora device's system clock with the client's time:

1. **Create Client**: Creates a time sync client with `TimeSyncDomain::WALL_CLOCK`
2. **Connect**: Connects to Aurora device time sync service
3. **Query Offset**: Gets the current wall clock offset between client and device
4. **Sync Clock**: If offset > 1ms, synchronizes the device's system clock
5. **Evaluate Accuracy**: Measures sync accuracy with multiple samples
6. **Display Results**: Shows mean/max error and quality verdict

### Key Concepts

#### Time Domains

- **Steady Clock**: Monotonic clock that never goes backwards (recommended for timestamp translation)
- **Wall Clock**: System wall time (use for synchronizing device system time)

#### When to Use Each

| Use Case | Recommended Domain |
|----------|-------------------|
| Correlating sensor timestamps | Steady Clock |
| Multi-sensor fusion | Steady Clock |
| Synchronizing device time | Wall Clock |
| Accurate file timestamps on device | Wall Clock |

#### Quality Metrics (Steady Clock)

- **RMSE**: Root mean square error of the synchronization
- **Max Error**: Maximum error observed in the sample window
- **Scale Factor**: Clock speed ratio between client and server
- **Offset**: Time offset between the two clocks

#### Timestamp Translation (Steady Clock)

Once synchronized, you can translate any Aurora timestamp to your local time domain:

```cpp
uint64_t aurora_timestamp = ...; // From tracking frame, IMU data, etc.
uint64_t local_timestamp;
if (client.translateTimestamp(aurora_timestamp, &local_timestamp)) {
    // Use local_timestamp
}
```

#### Wall Clock Synchronization

Synchronize the device's system clock:

```cpp
// Query current offset
slamtec_aurora_sdk_wallclock_offset_result_t offset_result;
client.getWallClockOffset(1000, &offset_result);

// Sync if needed
slamtec_aurora_sdk_wallclock_sync_result_t sync_result;
slamtec_aurora_sdk_errorcode_t error;
client.syncServerWallClock(1000, &sync_result, &error);

// Evaluate accuracy
slamtec_aurora_sdk_wallclock_accuracy_result_t accuracy;
client.evaluateWallClockSyncAccuracy(1000, &accuracy);
```

## Output Examples

### Steady Clock Sync Output

```
=============================================
Steady Clock Synchronization Demo
=============================================
This demo establishes time synchronization and translates
timestamps from pose and IMU data.

Step 1: Creating time sync client...
  ✓ Client created

Step 2: Connecting to time sync service...
  Address: 192.168.1.100:9527
  ✓ Connected successfully

Step 3: Configuring synchronization options...
  ✓ Options configured

Step 4: Initializing time synchronization...
  ✓ Initialization successful

Step 5: Waiting for synchronization...
  ✓ Synchronized!

========================================
Synchronization Quality Metrics
========================================
  RMSE:              0.523 ms
  Max Error:         1.234 ms
  Scale Factor:      1.000000123
  Offset:            -125.456 ms
  Samples Used:      10/12

  Assessment: EXCELLENT (< 1ms)

[POSE #10]
  Position:        (1.234, 0.567, 0.089) m
  Aurora Time:     123456789000000 ns
  Local Time:      123456663544000 ns
  Time Offset:     -125.456 ms

[IMU #100]
  Accel (g):       (0.0123, -0.0045, 0.9876)
  Gyro (dps):      (0.234, -0.123, 0.567)
  Aurora Time:     123456790000000 ns
  Local Time:      123456664544000 ns
  Time Offset:     -125.456 ms
```

### Wall Clock Sync Output

```
=============================================
Wall Clock Synchronization Demo
=============================================
This demo synchronizes the Aurora device's system clock
with your local machine's time.

Step 1: Created wall clock sync client
  ✓ Connected to 192.168.1.100:9527

Step 2: Querying wall clock offset...
  Offset:     125.456 ms
  RTT:        2.345 ms
  Status:     Large offset detected - sync recommended

Step 3: Synchronizing device wall clock...
  ✓ Wall clock synchronized successfully!
  Applied offset: 125.456 ms

Step 4: Evaluating sync accuracy...
  Sample 1: Error = 0.234 ms
  Sample 2: Error = 0.312 ms
  Sample 3: Error = 0.287 ms
  ...

Sync Accuracy Statistics:
  Mean Error: 0.278 ms
  Max Error:  0.456 ms
  Verdict:    EXCELLENT (< 1ms)

  ✓ Wall clock sync demo completed!
```

## Integration with Other Features

Time synchronization is essential when correlating Aurora data with external sensors or events:

### Example 1: Translating Pose Timestamps

```cpp
// Get pose with Aurora timestamp
slamtec_aurora_sdk_pose_se3_t pose;
uint64_t aurora_timestamp;
sdk->dataProvider.getCurrentPoseSE3WithTimestamp(pose, aurora_timestamp);

// Translate to local time
uint64_t local_timestamp;
if (timeSync.translateTimestamp(aurora_timestamp, &local_timestamp)) {
    // Now you can correlate with local events/sensors
    correlateWithExternalSensor(local_timestamp, pose);
}
```

### Example 2: Translating IMU Timestamps

```cpp
// Get IMU data with Aurora timestamps
std::vector<slamtec_aurora_sdk_imu_data_t> imuData;
if (sdk->dataProvider.peekIMUData(imuData)) {
    for (auto& imu : imuData) {
        uint64_t local_timestamp;
        if (timeSync.translateTimestamp(imu.timestamp_ns, &local_timestamp)) {
            // Use synchronized timestamp for sensor fusion
            fuseIMUData(local_timestamp, imu.acc, imu.gyro);
        }
    }
}
```

### Example 3: Multi-Sensor Fusion

```cpp
// Aurora pose with translated timestamp
slamtec_aurora_sdk_pose_se3_t aurora_pose;
uint64_t aurora_time, aurora_local_time;
sdk->dataProvider.getCurrentPoseSE3WithTimestamp(aurora_pose, aurora_time);
timeSync.translateTimestamp(aurora_time, &aurora_local_time);

// External sensor reading with local timestamp
uint64_t lidar_timestamp = getCurrentLocalTime();
LidarScan lidar_scan = getLidarScan();

// Now both are in the same time domain - can be fused
if (abs(aurora_local_time - lidar_timestamp) < 50000000) {  // Within 50ms
    fuseSensorData(aurora_pose, lidar_scan);
}
```

## Troubleshooting

### Synchronization Fails

- Check network connectivity to Aurora device
- Verify the port (default: 9527) is not blocked
- Check if Aurora firmware supports time sync (required: v2.1.1+)

### Poor Accuracy (> 5ms)

- Check network latency and jitter
- Increase sample window size
- Reduce synchronization interval for more frequent updates

### Connection Refused

- Ensure Aurora device is powered on and connected
- Verify IP address is correct
- Check firewall settings

### Wall Clock Sync Fails

- Device must have sufficient privileges to modify system time
- On Linux: run device service with `sudo`
- On Windows: run as Administrator
- Some devices may not support system clock modification

## API Reference

For detailed API documentation, refer to:
- [Time Sync Tutorial](../../doc/TimeSync_Tutorial_EN.md)
- [API Reference](../../doc/html/index.html)

## Related Demos

- [Pose Augmentation](../pose_augmentation/README.md) - Uses time sync for high-frequency pose output
- [Simple Pose](../simple_pose/README.md) - Basic pose retrieval
