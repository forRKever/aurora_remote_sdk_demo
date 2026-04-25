# Aurora 远程 SDK - 时间同步教程

## 概述

Aurora 远程 SDK 提供了时间同步功能，允许您的应用程序：

1. **稳定时钟同步**：精确地将 Aurora 设备时间域的时间戳转换到本地机器的时间域。对于需要将 Aurora 传感器数据（相机帧、IMU 数据）与本地系统事件或时间戳进行关联的应用程序至关重要。

2. **墙上时钟同步**：将 Aurora 设备的系统时钟与本地机器的时间同步。用于确保设备具有准确的绝对时间，便于日志记录、文件时间戳和数据关联。

## 工作原理

### 基本原理

时间同步采用类似于**网络时间协议 (NTP)** 的方法：

1. **四时间戳交换**：客户端和服务器交换带有四个时间戳（T1, T2, T3, T4）的消息，以测量时钟偏移和网络延迟。

2. **线性回归**：收集多个样本并计算线性回归模型，以建立 Aurora 时间和客户端时间之间的关系：
   ```
   client_time = scale × aurora_time + offset
   ```

3. **离群值剔除**：统计离群值检测确保网络抖动不会影响精度。

4. **持续同步**：客户端在后台持续更新同步，即使存在时钟漂移也能保持精度。

### 主要特性

- **自动配置**：默认设置适用于大多数使用场景
- **亚毫秒级精度**：正常网络条件下典型精度为 0.1-1.0ms
- **后台同步**：初始化后，同步自动持续进行
- **多时间域支持**：支持稳定时钟（单调）和墙上时钟（系统时间）
- **墙上时钟同步**：能够将设备的系统时间与客户端同步

## 快速入门指南

### 步骤 1：包含头文件

C++ 应用程序：
```cpp
#include "aurora_pubsdk_inc.h"
using namespace rp::standalone::aurora;
```

C 应用程序：
```c
#include "aurora_pubsdk_inc.h"
```

### 步骤 2：创建和配置客户端

#### C++ API（推荐）

```cpp
// 创建时间同步客户端（使用稳定时钟）
RemoteTimeSyncClient client(TimeSyncDomain::STEADY_CLOCK);

// 连接到 Aurora 设备
if (!client.connect("192.168.1.100", 9527)) {
    std::cerr << "连接失败" << std::endl;
    return -1;
}

// 获取默认选项（已针对大多数使用场景进行优化）
auto options = TimeSyncOptions::getDefaults();

// 可选：如需要可自定义特定参数
// options.synchronization_interval_ms = 500;  // 采样间隔
// options.min_samples_for_sync = 10;          // 同步前所需样本数

// 应用选项
client.setOptions(options);

// 初始化同步
if (!client.initialize()) {
    std::cerr << "初始化失败" << std::endl;
    return -1;
}

// 等待同步完成
while (!client.isSynchronized()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

std::cout << "时间同步已建立！" << std::endl;
```

#### C API

```c
slamtec_aurora_sdk_errorcode_t err;

// 创建时间同步客户端
slamtec_aurora_sdk_timesync_handle_t client =
    slamtec_aurora_sdk_timesync_create_instance(
        SLAMTEC_AURORA_SDK_TIMESYNC_DOMAIN_STEADY_CLOCK, &err);

// 连接到服务器
err = slamtec_aurora_sdk_timesync_connect(client, "192.168.1.100", 9527);

// 获取默认选项
slamtec_aurora_sdk_timesync_options_t options;
slamtec_aurora_sdk_timesync_get_default_options(&options);

// 应用选项
slamtec_aurora_sdk_timesync_set_options(client, &options);

// 初始化
err = slamtec_aurora_sdk_timesync_initialize(client);

// 等待同步
while (!slamtec_aurora_sdk_timesync_is_synchronized(client)) {
    usleep(100000);  // 100ms
}

printf("时间同步已建立！\n");
```

### 步骤 3：转换时间戳

同步完成后，您可以将 Aurora 时间戳转换到本地时间域：

#### C++ API

```cpp
// 来自传感器数据的 Aurora 时间戳（纳秒）
uint64_t aurora_timestamp_ns = 123456789000000ULL;

// 转换到客户端时间域
uint64_t client_timestamp_ns;
if (client.translateTimestamp(aurora_timestamp_ns, &client_timestamp_ns)) {
    std::cout << "Aurora 时间: " << aurora_timestamp_ns << " ns" << std::endl;
    std::cout << "客户端时间: " << client_timestamp_ns << " ns" << std::endl;
} else {
    std::cerr << "转换失败 - 未同步" << std::endl;
}
```

#### C API

```c
uint64_t aurora_timestamp_ns = 123456789000000ULL;
uint64_t client_timestamp_ns;

err = slamtec_aurora_sdk_timesync_translate_timestamp(
    client, aurora_timestamp_ns, &client_timestamp_ns);

if (err == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
    printf("Aurora 时间: %llu ns\n", aurora_timestamp_ns);
    printf("客户端时间: %llu ns\n", client_timestamp_ns);
}
```

### 步骤 4：监控质量（可选）

您可以检查同步质量：

#### C++ API

```cpp
TimeSyncQuality quality;
if (client.getQuality(&quality)) {
    std::cout << "同步质量:" << std::endl;
    std::cout << "  均方根误差: " << quality.rmse_ms << " ms" << std::endl;
    std::cout << "  最大误差: " << quality.max_error_ms << " ms" << std::endl;
    std::cout << "  样本数: " << quality.sample_count << std::endl;
}
```

#### C API

```c
slamtec_aurora_sdk_timesync_quality_t quality;
err = slamtec_aurora_sdk_timesync_get_quality(client, &quality);
if (err == SLAMTEC_AURORA_SDK_ERRORCODE_OK) {
    printf("均方根误差: %.3f ms\n", quality.rmse_ms);
    printf("最大误差: %.3f ms\n", quality.max_error_ms);
}
```

### 步骤 5：清理

#### C++ API
```cpp
// 停止同步（可选 - 析构函数会处理此操作）
client.stop();
// 客户端在超出作用域时自动销毁
```

#### C API
```c
// 销毁客户端
slamtec_aurora_sdk_timesync_destroy_instance(client);
```

## 完整示例

以下是一个完整的工作示例：

```cpp
#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace rp::standalone::aurora;

int main() {
    try {
        // 创建和连接
        RemoteTimeSyncClient client(TimeSyncDomain::STEADY_CLOCK);

        if (!client.connect("192.168.1.100", 9527)) {
            std::cerr << "连接失败" << std::endl;
            return 1;
        }

        std::cout << "已连接到 Aurora 设备" << std::endl;

        // 使用默认选项（无需手动配置！）
        auto options = TimeSyncOptions::getDefaults();
        client.setOptions(options);

        // 初始化
        if (!client.initialize()) {
            std::cerr << "初始化失败" << std::endl;
            return 1;
        }

        // 等待同步
        std::cout << "等待同步..." << std::endl;
        while (!client.isSynchronized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << "已同步！" << std::endl;

        // 检查质量
        TimeSyncQuality quality;
        if (client.getQuality(&quality)) {
            std::cout << "质量 - 均方根误差: " << quality.rmse_ms
                      << " ms, 样本数: " << quality.sample_count << std::endl;
        }

        // 示例：从 Aurora 传感器数据转换时间戳
        uint64_t aurora_ts = 123456789000000ULL;  // 来自传感器
        uint64_t client_ts;

        if (client.translateTimestamp(aurora_ts, &client_ts)) {
            std::cout << "转换成功:" << std::endl;
            std::cout << "  Aurora: " << aurora_ts << " ns" << std::endl;
            std::cout << "  客户端: " << client_ts << " ns" << std::endl;
        }

        // 客户端自动清理

    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
```

## 墙上时钟同步

除了用于时间戳转换的稳定时钟同步外，您还可以将 Aurora 设备的系统时钟与本地机器的时间同步。

### 何时使用墙上时钟同步

| 使用场景 | 推荐域 |
|----------|--------|
| 关联传感器时间戳 | 稳定时钟 |
| 多传感器融合 | 稳定时钟 |
| 同步设备时间 | 墙上时钟 |
| 设备上准确的文件时间戳 | 墙上时钟 |
| 带绝对时间的数据记录 | 墙上时钟 |

### 墙上时钟同步示例

```cpp
#include "aurora_pubsdk_inc.h"
#include <iostream>
#include <cmath>

using namespace rp::standalone::aurora;

int main() {
    // 使用墙上时钟域创建客户端
    RemoteTimeSyncClient client(TimeSyncDomain::WALL_CLOCK);

    // 连接到 Aurora 设备
    if (!client.connect("192.168.1.100", 9527)) {
        std::cerr << "连接失败" << std::endl;
        return 1;
    }

    // 步骤 1：查询当前墙上时钟偏移
    slamtec_aurora_sdk_wallclock_offset_result_t offset_result;
    if (!client.getWallClockOffset(1000, &offset_result)) {
        std::cerr << "获取墙上时钟偏移失败" << std::endl;
        return 1;
    }

    double offset_ms = offset_result.offset_ns / 1e6;
    std::cout << "当前偏移: " << offset_ms << " ms" << std::endl;

    // 步骤 2：如果偏移较大（> 1ms）则同步
    if (std::abs(offset_ms) > 1.0) {
        slamtec_aurora_sdk_wallclock_sync_result_t sync_result;
        slamtec_aurora_sdk_errorcode_t error;

        if (client.syncServerWallClock(1000, &sync_result, &error)) {
            std::cout << "同步成功！" << std::endl;
            std::cout << "应用的偏移: " << sync_result.applied_offset_ns / 1e6 << " ms" << std::endl;
        } else {
            std::cerr << "同步失败，错误码: " << error << std::endl;
            return 1;
        }
    }

    // 步骤 3：评估同步精度
    slamtec_aurora_sdk_wallclock_accuracy_result_t accuracy;
    if (client.evaluateWallClockSyncAccuracy(1000, &accuracy)) {
        std::cout << "同步精度: " << std::abs(accuracy.offset_error_ns) / 1e6 << " ms" << std::endl;
    }

    return 0;
}
```

### 墙上时钟同步 API 参考

| 函数 | 描述 |
|------|------|
| `getWallClockOffset()` | 查询客户端和设备墙上时钟之间的当前偏移 |
| `syncServerWallClock()` | 将设备的系统时钟与客户端同步 |
| `evaluateWallClockSyncAccuracy()` | 评估墙上时钟同步的精度 |

### 墙上时钟同步注意事项

- **需要权限**：设备必须有足够的权限修改其系统时间
  - 在 Linux 上：使用 `sudo` 运行设备服务
  - 在 Windows 上：以管理员身份运行
- **一次性操作**：与稳定时钟同步不同，墙上时钟同步通常是一次性操作
- **网络延迟**：为获得最佳效果，请在网络延迟稳定时执行墙上时钟同步

## 配置选项

虽然默认选项适用于大多数情况，但如需要您可以自定义：

| 选项 | 默认值 | 描述 |
|------|--------|------|
| `synchronization_interval_ms` | 500 | 同步请求间隔时间（毫秒）|
| `sample_window_size` | 100 | 保留的样本数量 |
| `min_samples_for_sync` | 10 | 同步前所需的样本数 |
| `timeout_ms` | 1000 | 网络超时（毫秒）|
| `max_rtt_ms` | 100.0 | 最大可接受往返时间 |
| `initialize_timeout_ms` | 15000 | 初始化超时时间 |

## 最佳实践

1. **使用默认选项**：默认值针对典型网络条件进行了优化。仅在有特定要求时才自定义。

2. **检查同步状态**：在转换时间戳之前，始终验证 `isSynchronized()` 返回 true。

3. **监控质量**：定期检查同步质量，特别是在网络条件变化的环境中。

4. **处理初始化超时**：初始化可能需要几秒钟。确保您的超时时间足够（默认：15 秒）。

5. **选择合适的时间域**：
   - 对于单调时间使用 `STEADY_CLOCK`（推荐用于大多数情况）
   - 如果需要绝对时间关联，使用 `WALL_CLOCK`

## 故障排除

### 初始化失败
- 验证 Aurora 设备可在指定的 IP 和端口访问
- 检查时间同步服务器是否在 Aurora 设备上运行（默认端口：9527）
- 确保网络延迟合理（建议 RTT < 100ms）

### 同步质量差
- 检查网络稳定性 - 高抖动会影响精度
- 验证设备和客户端时钟没有过度漂移
- 考虑增加 `sample_window_size` 以获得更稳定的估计

### 转换返回错误
- 确保在转换之前 `isSynchronized()` 返回 true
- 验证 Aurora 时间戳有效且在预期范围内

## 支持

有关更多信息，请参阅 Aurora 远程 SDK 文档或联系 SLAMTEC 技术支持。
