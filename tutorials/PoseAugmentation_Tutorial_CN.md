# Aurora Remote SDK - 位姿增强教程

## 概述

Aurora Remote SDK 提供了**位姿增强（Pose Augmentation）**功能，通过在视觉跟踪更新之间集成 IMU（惯性测量单元）测量值来提供高频位姿输出。该功能使应用程序能够获得高达 200Hz 的位姿估计频率，远高于典型的 10-20Hz 视觉跟踪频率。

## 工作原理

### 基本原理

像 Aurora 这样的视觉惯性 SLAM 系统通常以相机帧率（10-20Hz）跟踪相机位姿。在这些视觉跟踪更新之间，位姿增强模块使用 **IMU 预积分**来预测中间位姿：

1. **视觉跟踪基准**：视觉 SLAM 系统提供准确但频率较低的位姿估计，以及 IMU 偏置估计和速度。

2. **IMU 预积分**：在视觉更新之间，系统积分 IMU 测量值（加速度和陀螺仪）来预测设备的运动。

3. **高频输出**：以高频率（50Hz、100Hz 或 200Hz）发布增强位姿，结合视觉精度和 IMU 响应性。

4. **可选平滑**：可以应用指数移动平均滤波器来减少高频噪声，同时保持响应性。

### 运行模式

位姿增强功能支持两种模式：

- **VISUAL_ONLY（仅视觉）**：仅输出视觉跟踪位姿（10-20Hz）。不执行 IMU 增强。
- **IMU_VISION_MIXED（IMU-视觉混合）**：以高频率（50-200Hz）输出 IMU 增强位姿。这是大多数应用的推荐模式。

## 优点和缺点

### 优点

✅ **高频输出**：以 50Hz、100Hz 或 200Hz 获取位姿更新，而不是 10-20Hz

✅ **低延迟**：IMU 测量值立即处理，提供最小延迟

✅ **平滑运动跟踪**：非常适合机器人控制、AR/VR 或平滑可视化等实时应用

✅ **连续输出**：即使在短暂的视觉跟踪中断期间也能保持位姿输出

### 缺点

⚠️ **IMU 漂移**：基于 IMU 的预测会随时间累积误差。系统通过视觉更新进行校正，但在更新之间可能会发生漂移。

⚠️ **噪声**：原始 IMU 数据可能有噪声，导致增强位姿抖动。使用平滑功能可以缓解这个问题。

⚠️ **计算成本**：IMU 积分在后台线程中运行，消耗额外的 CPU 资源。

⚠️ **需要 IMU**：此功能仅适用于配备 IMU 传感器的设备。

### 何时使用位姿增强

**在以下情况下使用位姿增强：**
- 需要高频位姿更新（>30Hz）
- 应用程序需要低延迟位姿估计
- 实时控制机器人或无人机
- 实现需要平滑运动的 AR/VR 应用

**在以下情况下不使用位姿增强：**
- 应用程序只需要 10-20Hz 位姿更新
- 希望最小化 CPU 使用
- 优先考虑绝对精度而非响应性
- 设备没有 IMU 传感器

## 快速入门指南

### 步骤 1：包含头文件

对于 C++ 应用：
```cpp
#include "aurora_pubsdk_inc.h"
using namespace rp::standalone::aurora;
```

对于 C 应用：
```c
#include "aurora_pubsdk_inc.h"
```

### 步骤 2：创建会话并连接

#### C++ API（推荐）

```cpp
// 创建监听器以接收位姿增强回调
class MyListener : public RemoteSDKListener {
public:
    void onPoseAugmentationResult(uint64_t timestamp_ns,
                                   slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                                   const slamtec_aurora_sdk_pose_se3_t& pose) override {
        // 在这里处理高频位姿更新
        std::cout << "位姿时间戳 " << timestamp_ns << ": ("
                  << pose.translation.x << ", "
                  << pose.translation.y << ", "
                  << pose.translation.z << ")" << std::endl;
    }
};

// 创建 SDK 会话
MyListener listener;
auto sdk = RemoteSDK::CreateSession(&listener);

// 连接到 Aurora 设备
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
// 位姿增强结果的回调函数
void on_pose_result(void* user_data, uint64_t timestamp_ns,
                    slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                    const slamtec_aurora_sdk_pose_se3_t* pose) {
    printf("位姿时间戳 %llu: (%f, %f, %f)\n",
           timestamp_ns, pose->translation.x,
           pose->translation.y, pose->translation.z);
}

// 设置监听器
slamtec_aurora_sdk_listener_t listener = {0};
listener.on_pose_augmentation_result = on_pose_result;

// 创建会话并连接（与 C++ 相同）
slamtec_aurora_sdk_session_handle_t session;
slamtec_aurora_sdk_create_session(&listener, NULL, &session);

slamtec_aurora_sdk_server_connection_info_t info = {0};
// ... (设置连接信息与 C++ 相同)
slamtec_aurora_sdk_controller_connect(session, &info);
```

### 步骤 3：配置并启动位姿增强

#### C++ API

```cpp
// 配置位姿增强
slamtec_aurora_sdk_pose_augmentation_config_t config{};
config.output_frequency = SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ;
config.enable_smoothing = 0;  // 禁用平滑（默认）
config.smoothing_factor = 0.3f;  // 仅在启用平滑时使用

// 启动位姿增强
if (!sdk->dataProvider.startPoseAugmentation(
        SLAMTEC_AURORA_SDK_POSE_AUGMENTATION_MODE_IMU_VISION_MIXED,
        config)) {
    std::cerr << "启动位姿增强失败！" << std::endl;
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
    printf("启动位姿增强失败：%d\n", result);
}
```

### 步骤 4：接收位姿更新

位姿更新通过您在步骤 2 中注册的 `onPoseAugmentationResult` 回调传递。回调从后台线程以配置的频率（50Hz、100Hz 或 200Hz）调用。

### 步骤 5：停止位姿增强

完成后：

#### C++ API
```cpp
sdk->dataProvider.stopPoseAugmentation();
```

#### C API
```c
slamtec_aurora_sdk_dataprovider_stop_pose_augmentation(session);
```

## 配置选项

### 输出频率

根据应用需求选择位姿输出频率：

| 频率 | 使用场景 | CPU 使用 |
|------|---------|---------|
| `POSE_OUTPUT_FREQ_50HZ` | 基础实时应用 | 低 |
| `POSE_OUTPUT_FREQ_100HZ` | 标准机器人控制 | 中等 |
| `POSE_OUTPUT_FREQ_200HZ` | 高性能 AR/VR、无人机 | 高 |
| `POSE_OUTPUT_FREQ_HIGHEST_POSSIBLE` | 最大频率（通常 200Hz+）| 最高 |

### 位姿平滑

位姿平滑使用指数移动平均（EMA）来减少 IMU 增强位姿输出中的高频噪声。

**在以下情况下启用平滑：**
- 观察到位姿输出中的抖动或噪声
- 应用程序偏好平滑运动而非即时更新
- 可视化位姿轨迹

**在以下情况下禁用平滑：**
- 需要最具响应性的位姿更新
- 应用程序执行自己的滤波
- 延迟比平滑度更重要

**平滑因子（alpha）：**
- 范围：`0.0` 到 `1.0`
- 较高值（例如 `0.7` - `1.0`）：更具响应性，不太平滑
- 较低值（例如 `0.1` - `0.3`）：更平滑，响应性较低
- 推荐默认值：`0.3`

**启用平滑的示例：**

```cpp
config.enable_smoothing = 1;  // 启用平滑
config.smoothing_factor = 0.5f;  // 中等平滑
```

## 完整示例

这是一个完整的工作示例：

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

        // 每 20 个位姿显示一次（200Hz -> 10Hz 显示）
        if (count % 20 == 0) {
            std::cout << "位姿 #" << count << " 时间戳 " << timestamp_ns << " ns: ("
                      << pose.translation.x << ", "
                      << pose.translation.y << ", "
                      << pose.translation.z << ")" << std::endl;
        }
    }
};

int main(int argc, char** argv) {
    // 创建会话
    PoseListener listener;
    auto sdk = RemoteSDK::CreateSession(&listener);

    // 连接到设备
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
        std::cerr << "连接失败！" << std::endl;
        return -1;
    }

    std::cout << "已连接到 Aurora 设备：" << address << std::endl;

    // 配置并启动位姿增强
    slamtec_aurora_sdk_pose_augmentation_config_t config{};
    config.output_frequency = SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ;
    config.enable_smoothing = 1;  // 启用平滑
    config.smoothing_factor = 0.3f;  // 轻度平滑

    if (!sdk->dataProvider.startPoseAugmentation(
            SLAMTEC_AURORA_SDK_POSE_AUGMENTATION_MODE_IMU_VISION_MIXED,
            config)) {
        std::cerr << "启动位姿增强失败！" << std::endl;
        return -1;
    }

    std::cout << "位姿增强已在 200Hz 启动，带平滑功能" << std::endl;
    std::cout << "按 Ctrl+C 停止..." << std::endl;

    // 运行 30 秒
    auto start = std::chrono::steady_clock::now();
    while (g_running) {
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() > 30) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 停止并清理
    sdk->dataProvider.stopPoseAugmentation();
    std::cout << "\n接收的位姿总数：" << g_pose_count.load() << std::endl;
    sdk->controller.disconnect();

    return 0;
}
```

## 轮询 vs 回调

除了通过回调接收位姿更新外，您还可以轮询最新的增强位姿：

```cpp
// 轮询当前增强位姿
slamtec_aurora_sdk_pose_se3_t pose;
uint64_t timestamp;
if (sdk->dataProvider.getAugmentedPose(pose, &timestamp)) {
    std::cout << "当前位姿：(" << pose.translation.x << ", "
              << pose.translation.y << ", " << pose.translation.z << ")"
              << std::endl;
}
```

**注意：** 轮询返回最近计算的位姿。对于连续的高频更新，请使用回调方法。

## 故障排除

### 没有收到位姿更新

**问题：** 回调未被调用

**解决方案：**
1. 确保设备有 IMU 传感器
2. 验证连接已建立
3. 检查视觉跟踪是否正常工作（设备必须已初始化）
4. 确保在启动位姿增强之前注册了回调

### 位姿有噪声或抖动

**问题：** 位姿输出有高频抖动

**解决方案：**
1. 启用位姿平滑：`config.enable_smoothing = 1`
2. 调整平滑因子（尝试 `0.3` - `0.5`）
3. 将输出频率降低到 100Hz 或 50Hz
4. 检查影响 IMU 传感器的振动

### CPU 使用率高

**问题：** 位姿增强使用过多 CPU

**解决方案：**
1. 降低输出频率（使用 50Hz 或 100Hz 而不是 200Hz）
2. 如果不需要高频率，使用 `VISUAL_ONLY` 模式
3. 禁用平滑以减少计算开销

### 位姿漂移

**问题：** 增强位姿偏离实际位置

**解决方案：**
1. 确保视觉跟踪正常工作
2. 检查 IMU 校准是否正确
3. 验证设备固件是最新版本
4. 在短暂的视觉跟踪丢失期间，这是预期行为；当视觉跟踪恢复时会纠正漂移

## API 参考

### 配置结构

```c
typedef struct _slamtec_aurora_sdk_pose_augmentation_config_t {
    slamtec_aurora_sdk_pose_output_frequency_t output_frequency;
    int enable_smoothing;      // 0: 禁用，非零：启用
    float smoothing_factor;    // 0.0 - 1.0（默认：0.3）
} slamtec_aurora_sdk_pose_augmentation_config_t;
```

### 函数

**启动位姿增强：**
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

**停止位姿增强：**
```c
// C++ API
bool stopPoseAugmentation(slamtec_aurora_sdk_errorcode_t* errcode = nullptr);

// C API
slamtec_aurora_sdk_errorcode_t slamtec_aurora_sdk_dataprovider_stop_pose_augmentation(
    slamtec_aurora_sdk_session_handle_t handle);
```

**获取增强位姿（轮询）：**
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

## 最佳实践

1. **在设备初始化后启动位姿增强**并且视觉跟踪已开始
2. **使用回调进行连续更新**，轮询用于偶尔查询
3. **可视化时启用平滑**，实时控制时禁用
4. **根据需求选择频率**：50Hz 对大多数应用已足够
5. **不需要时停止位姿增强**以节省 CPU 资源
6. **处理连接丢失**：如果设备断开连接，停止并重新启动位姿增强

## 另请参阅

- 时间同步教程 - 用于准确的时间戳关联
- 跟踪数据 API - 用于访问视觉跟踪位姿
- IMU 数据 API - 用于原始 IMU 测量

---

**版权所有 © 2013-2025 思岚科技有限公司。保留所有权利。**
