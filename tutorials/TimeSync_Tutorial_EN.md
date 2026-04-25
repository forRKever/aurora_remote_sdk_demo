# Aurora Remote SDK - Time Synchronization Tutorial

## Overview

The Aurora Remote SDK provides time synchronization features that allow your application to:

1. **Steady Clock Sync**: Accurately translate timestamps from the Aurora device's time domain to your local machine's time domain. Essential for correlating Aurora sensor data (camera frames, IMU data) with local system events or timestamps.

2. **Wall Clock Sync**: Synchronize the Aurora device's system clock with your local machine's time. Useful for ensuring the device has accurate absolute time for logging, file timestamps, and data correlation.

## How It Works

### Basic Principle

The time synchronization uses a **Network Time Protocol (NTP)**-like approach:

1. **Four-Timestamp Exchange**: The client and server exchange messages with four timestamps (T1, T2, T3, T4) to measure both the clock offset and network delay.

2. **Linear Regression**: Multiple samples are collected and a linear regression model is computed to establish the relationship between Aurora time and client time:
   ```
   client_time = scale × aurora_time + offset
   ```

3. **Outlier Rejection**: Statistical outlier detection ensures that network jitter doesn't affect accuracy.

4. **Continuous Synchronization**: The client continuously updates the synchronization in the background to maintain accuracy even with clock drift.

### Key Features

- **Automatic Configuration**: Default settings work well for most use cases
- **Sub-millisecond Accuracy**: Typical accuracy is 0.1-1.0ms under normal network conditions
- **Background Synchronization**: Once initialized, synchronization continues automatically
- **Multiple Time Domains**: Supports both steady clock (monotonic) and wall clock (system time)
- **Wall Clock Sync**: Ability to synchronize the device's system time with the client

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

### Step 2: Create and Configure Client

#### C++ API (Recommended)

```cpp
// Create time sync client (using steady clock)
RemoteTimeSyncClient client(TimeSyncDomain::STEADY_CLOCK);

// Connect to Aurora device
if (!client.connect("192.168.1.100", 9527)) {
    std::cerr << "Failed to connect" << std::endl;
    return -1;
}

// Get default options (already optimized for most use cases)
auto options = TimeSyncOptions::getDefaults();

// Optional: Customize specific parameters if needed
// options.synchronization_interval_ms = 500;  // Sample interval
// options.min_samples_for_sync = 10;          // Samples before sync

// Apply options
client.setOptions(options);

// Initialize synchronization
if (!client.initialize()) {
    std::cerr << "Failed to initialize" << std::endl;
    return -1;
}

// Wait for synchronization to complete
while (!client.isSynchronized()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

std::cout << "Time synchronization established!" << std::endl;
```

#### C API

```c
slamtec_aurora_sdk_errorcode_t err;

// Create time sync client
slamtec_aurora_sdk_timesync_handle_t client =
    slamtec_aurora_sdk_timesync_create_instance(
        SLAMTEC_AURORA_SDK_TIMESYNC_DOMAIN_STEADY_CLOCK, &err);

// Connect to server
err = slamtec_aurora_sdk_timesync_connect(client, "192.168.1.100", 9527);

// Get default options
slamtec_aurora_sdk_timesync_options_t options;
slamtec_aurora_sdk_timesync_get_default_options(&options);

// Apply options
slamtec_aurora_sdk_timesync_set_options(client, &options);

// Initialize
err = slamtec_aurora_sdk_timesync_initialize(client);

// Wait for synchronization
while (!slamtec_aurora_sdk_timesync_is_synchronized(client)) {
    usleep(100000);  // 100ms
}

printf("Time synchronization established!\n");
```

### Step 3: Translate Timestamps

Once synchronized, you can translate Aurora timestamps to your local time domain:

#### C++ API

```cpp
// Aurora timestamp from sensor data (in nanoseconds)
uint64_t aurora_timestamp_ns = 123456789000000ULL;

// Translate to client time domain
uint64_t client_timestamp_ns;
if (client.translateTimestamp(aurora_timestamp_ns, &client_timestamp_ns)) {
    std::cout << "Aurora time: " << aurora_timestamp_ns << " ns" << std::endl;
    std::cout << "Client time: " << client_timestamp_ns << " ns" << std::endl;
} else {
    std::cerr << "Translation failed - not synchronized" << std::endl;
}
```

#### C API

```c
uint64_t aurora_timestamp_ns = 123456789000000ULL;
uint64_t client_timestamp_ns;

err = slamtec_aurora_sdk_timesync_translate_timestamp(
    client, aurora_timestamp_ns, &client_timestamp_ns);

if (err == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
    printf("Aurora time: %llu ns\n", aurora_timestamp_ns);
    printf("Client time: %llu ns\n", client_timestamp_ns);
}
```

### Step 4: Monitor Quality (Optional)

You can check the synchronization quality:

#### C++ API

```cpp
TimeSyncQuality quality;
if (client.getQuality(&quality)) {
    std::cout << "Synchronization Quality:" << std::endl;
    std::cout << "  RMSE: " << quality.rmse_ms << " ms" << std::endl;
    std::cout << "  Max Error: " << quality.max_error_ms << " ms" << std::endl;
    std::cout << "  Samples: " << quality.sample_count << std::endl;
}
```

#### C API

```c
slamtec_aurora_sdk_timesync_quality_t quality;
err = slamtec_aurora_sdk_timesync_get_quality(client, &quality);
if (err == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
    printf("RMSE: %.3f ms\n", quality.rmse_ms);
    printf("Max Error: %.3f ms\n", quality.max_error_ms);
}
```

### Step 5: Cleanup

#### C++ API
```cpp
// Stop synchronization (optional - destructor handles this)
client.stop();
// Client automatically destroyed when going out of scope
```

#### C API
```c
// Destroy client
slamtec_aurora_sdk_timesync_destroy_instance(client);
```

## Complete Example

Here's a complete working example:

```cpp
#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace rp::standalone::aurora;

int main() {
    try {
        // Create and connect
        RemoteTimeSyncClient client(TimeSyncDomain::STEADY_CLOCK);

        if (!client.connect("192.168.1.100", 9527)) {
            std::cerr << "Connection failed" << std::endl;
            return 1;
        }

        std::cout << "Connected to Aurora device" << std::endl;

        // Use default options (no manual configuration needed!)
        auto options = TimeSyncOptions::getDefaults();
        client.setOptions(options);

        // Initialize
        if (!client.initialize()) {
            std::cerr << "Initialization failed" << std::endl;
            return 1;
        }

        // Wait for synchronization
        std::cout << "Waiting for synchronization..." << std::endl;
        while (!client.isSynchronized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "Synchronized!" << std::endl;

        // Check quality
        TimeSyncQuality quality;
        if (client.getQuality(&quality)) {
            std::cout << "Quality - RMSE: " << quality.rmse_ms
                      << " ms, Samples: " << quality.sample_count << std::endl;
        }

        // Example: Translate timestamp from Aurora sensor data
        uint64_t aurora_ts = 123456789000000ULL;  // From sensor
        uint64_t client_ts;

        if (client.translateTimestamp(aurora_ts, &client_ts)) {
            std::cout << "Translation successful:" << std::endl;
            std::cout << "  Aurora: " << aurora_ts << " ns" << std::endl;
            std::cout << "  Client: " << client_ts << " ns" << std::endl;
        }

        // Client automatically cleaned up

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

## Wall Clock Synchronization

In addition to steady clock sync for timestamp translation, you can also synchronize the Aurora device's system clock with your local machine's time.

### When to Use Wall Clock Sync

| Use Case | Recommended Domain |
|----------|-------------------|
| Correlating sensor timestamps | Steady Clock |
| Multi-sensor fusion | Steady Clock |
| Synchronizing device time | Wall Clock |
| Accurate file timestamps on device | Wall Clock |
| Data logging with absolute time | Wall Clock |

### Wall Clock Sync Example

```cpp
#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <cmath>

using namespace rp::standalone::aurora;

int main() {
    // Create client with wall clock domain
    RemoteTimeSyncClient client(TimeSyncDomain::WALL_CLOCK);

    // Connect to Aurora device
    if (!client.connect("192.168.1.100", 9527)) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    // Step 1: Query current wall clock offset
    slamtec_aurora_sdk_wallclock_offset_result_t offset_result;
    if (!client.getWallClockOffset(1000, &offset_result)) {
        std::cerr << "Failed to get wall clock offset" << std::endl;
        return 1;
    }

    double offset_ms = offset_result.offset_ns / 1e6;
    std::cout << "Current offset: " << offset_ms << " ms" << std::endl;

    // Step 2: Sync if offset is significant (> 1ms)
    if (std::abs(offset_ms) > 1.0) {
        slamtec_aurora_sdk_wallclock_sync_result_t sync_result;
        slamtec_aurora_sdk_errorcode_t error;

        if (client.syncServerWallClock(1000, &sync_result, &error)) {
            std::cout << "Sync successful!" << std::endl;
            std::cout << "Applied offset: " << sync_result.applied_offset_ns / 1e6 << " ms" << std::endl;
        } else {
            std::cerr << "Sync failed with error: " << error << std::endl;
            return 1;
        }
    }

    // Step 3: Evaluate sync accuracy
    slamtec_aurora_sdk_wallclock_accuracy_result_t accuracy;
    if (client.evaluateWallClockSyncAccuracy(1000, &accuracy)) {
        std::cout << "Sync accuracy: " << std::abs(accuracy.offset_error_ns) / 1e6 << " ms" << std::endl;
    }

    return 0;
}
```

### Wall Clock Sync API Reference

| Function | Description |
|----------|-------------|
| `getWallClockOffset()` | Query the current offset between client and device wall clocks |
| `syncServerWallClock()` | Synchronize the device's system clock with the client |
| `evaluateWallClockSyncAccuracy()` | Evaluate the accuracy of the wall clock synchronization |

### Wall Clock Sync Notes

- **Privileges Required**: The device must have sufficient privileges to modify its system time
  - On Linux: run device service with `sudo`
  - On Windows: run as Administrator
- **One-time Operation**: Unlike steady clock sync, wall clock sync is typically a one-time operation
- **Network Latency**: For best results, perform wall clock sync when network latency is stable

## Configuration Options

While the default options work well for most cases, you can customize if needed:

| Option | Default | Description |
|--------|---------|-------------|
| `synchronization_interval_ms` | 500 | Time between sync requests (ms) |
| `sample_window_size` | 100 | Number of samples to keep |
| `min_samples_for_sync` | 10 | Samples needed before synchronized |
| `timeout_ms` | 1000 | Network timeout (ms) |
| `max_rtt_ms` | 100.0 | Maximum acceptable round-trip time |
| `initialize_timeout_ms` | 15000 | Timeout for initialization |

## Best Practices

1. **Use Default Options**: The defaults are optimized for typical network conditions. Only customize if you have specific requirements.

2. **Check Synchronization Status**: Always verify `isSynchronized()` returns true before translating timestamps.

3. **Monitor Quality**: Periodically check synchronization quality, especially in environments with variable network conditions.

4. **Handle Initialization Timeout**: The initialization may take several seconds. Ensure your timeout is adequate (default: 15 seconds).

5. **Choose Appropriate Time Domain**:
   - Use `STEADY_CLOCK` for monotonic time (recommended for most cases)
   - Use `WALL_CLOCK` if you need absolute time correlation

## Troubleshooting

### Initialization Fails
- Verify the Aurora device is reachable at the specified IP and port
- Check that the time sync server is running on the Aurora device (default port: 9527)
- Ensure network latency is reasonable (< 100ms RTT recommended)

### Poor Synchronization Quality
- Check network stability - high jitter affects accuracy
- Verify the device and client clocks are not drifting excessively
- Consider increasing `sample_window_size` for more stable estimates

### Translation Returns Error
- Ensure `isSynchronized()` returns true before translating
- Verify the Aurora timestamp is valid and in the expected range

## Support

For more information, refer to the Aurora Remote SDK documentation or contact SLAMTEC technical support.
