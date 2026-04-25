# 位姿增强演示

## 概述

本演示程序展示如何使用 Aurora Remote SDK 的位姿增强功能，通过集成 IMU 数据来实现高频位姿输出。这将位姿更新速率从典型的 10-15 Hz（纯视觉）提高到 200+ Hz。

## 功能特性

- **高频位姿输出**：高达 200 Hz 的位姿更新
- **IMU-视觉融合**：结合视觉 SLAM 与 IMU 预积分
- **多种输出频率**：50Hz、100Hz、200Hz 或最高可能频率
- **可选平滑**：可配置的位姿平滑（适用于需要的应用）
- **自动回退**：当 IMU 数据不可用时回退到纯视觉模式

## 先决条件

- Aurora 设备连接到网络
- Aurora 固件版本 2.1.1 或更高
- 设备必须运行并成功跟踪

## 构建

此演示程序会随其他演示一起自动构建：

```bash
cd build
cmake ..
make pose_augmentation
```

## 使用方法

### 自动发现模式

```bash
./pose_augmentation
```

这将自动发现并连接到网络上的 Aurora 设备。

### 指定设备地址

```bash
./pose_augmentation 192.168.1.100
```

### 帮助

```bash
./pose_augmentation --help
```

## 工作原理

### 位姿增强模式

#### 纯视觉模式
- 仅使用 VSLAM 跟踪结果
- 典型更新速率：10-15 Hz
- 最准确但频率最低

#### IMU-视觉混合模式
- 将 VSLAM 位姿与 IMU 预积分相结合
- 更新速率：高达 200+ Hz
- 在视觉更新之间提供平滑的高频位姿估计

### 处理流程

1. **连接到设备**：与 Aurora 设备建立连接
2. **配置增强**：设置所需的输出频率和选项
3. **启动增强**：开始 IMU 集成的后台线程
4. **接收位姿**：回调接收高频位姿更新
5. **监控质量**：跟踪视觉与混合位姿统计信息

### 配置选项

```cpp
slamtec_aurora_sdk_pose_augmentation_config_t config;

// 输出频率选项：
// - SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_50HZ: 50 Hz 输出
// - SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_100HZ: 100 Hz 输出
// - SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ: 200 Hz 输出
// - SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_HIGHEST_POSSIBLE: 匹配 IMU 采样率
config.output_frequency = SLAMTEC_AURORA_SDK_POSE_OUTPUT_FREQ_200HZ;

// 平滑（可选）：
config.enable_smoothing = 0;         // 禁用以获得最低延迟（0=false，1=true）
config.smoothing_factor = 0.3f;      // 0.0-1.0，仅在启用时使用
```

## 输出示例

```
=============================================
Aurora 位姿增强演示
=============================================
SDK 版本: 2.1.0-rtm
构建日期: 2025-01-15 14:23:45

步骤 1: 连接到 Aurora 设备...
  正在搜索 Aurora 设备（5 秒）...
  找到 1 个设备，连接到第一个...
  ✓ 成功连接

设备信息:
  名称:         Aurora-12345
  型号:         AURORA_A1
  序列号:       A1-2024-12345
  固件版本:     2.1.0

步骤 2: 配置位姿增强...
  配置:
    - 目标频率: 200 Hz
    - 平滑: 禁用（最低延迟）
    - 模式: IMU-视觉混合

步骤 3: 启动位姿增强...
  ✓ 位姿增强已启动

=============================================
接收高频位姿更新
=============================================
演示程序现在正在接收高频位姿更新。
移动 Aurora 设备以查看位姿变化。
（显示每第 50 个位姿以避免控制台刷屏）

按 Ctrl+C 停止

[IMU_MIXED] 位姿 #50 - 位置: (1.234, 0.567, 0.089) m | 时间戳: 123456789000 ns
[IMU_MIXED] 位姿 #100 - 位置: (1.245, 0.578, 0.091) m | 时间戳: 123456789250 ns
[IMU_MIXED] 位姿 #150 - 位置: (1.256, 0.589, 0.093) m | 时间戳: 123456789500 ns

[统计信息 5秒后]
  位姿总数:    1000
  视觉位姿:    75
  混合位姿:    925
  平均位姿速率: 200.0 Hz（最近 5 秒）

[统计信息 10秒后]
  位姿总数:    2000
  视觉位姿:    150
  混合位姿:    1850
  平均位姿速率: 200.0 Hz（最近 5 秒）

^C
按下 Ctrl-C，正在停止...

=============================================
正在关闭...
=============================================
  ✓ 已停止位姿增强

最终统计信息:
  接收的位姿总数: 2000
  视觉位姿:       150
  IMU 混合位姿:   1850
  运行时间:       10 秒
  总体位姿速率:   200.0 Hz

  ✓ 已断开与设备的连接

演示成功完成！
```

## 使用场景

### 机器人导航
高频位姿更新为移动机器人提供平滑的运动控制：

```cpp
void onPoseAugmentationResult(uint64_t timestamp_ns,
                              slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                              const slamtec_aurora_sdk_pose_se3_t& pose) {
    // 使用高频位姿进行运动控制
    updateRobotController(pose);
}
```

### AR/VR 应用
低延迟位姿更新减少运动到光子延迟：

```cpp
// 获取最新增强位姿
slamtec_aurora_sdk_pose_se3_t pose;
uint64_t timestamp;
sdk->dataProvider.getAugmentedPose(pose, &timestamp);

// 以显示刷新率渲染（90Hz、120Hz 等）
renderARContent(pose);
```

### 数据记录
捕获高频轨迹数据：

```cpp
// 记录所有位姿更新
void onPoseAugmentationResult(uint64_t timestamp_ns,
                              slamtec_aurora_sdk_pose_augmentation_mode_t mode,
                              const slamtec_aurora_sdk_pose_se3_t& pose) {
    logPose(timestamp_ns, mode, pose);
}
```

## 性能考虑

### IMU 采样率
- 典型 IMU 速率：200-500 Hz，取决于设备型号
- 位姿输出频率不能超过 IMU 速率
- 使用 `HIGHEST_POSSIBLE` 频率以匹配 IMU 速率

### 网络带宽
- 位姿增强在客户端侧运行，网络流量最小
- 对于带宽受限的网络，考虑较低的输出频率

### CPU 使用
- IMU 集成在客户端侧运行
- 较高的输出频率会增加 CPU 使用
- 后台线程执行集成，对主线程影响最小

## 故障排除

### 低位姿更新速率
- 检查配置（使用 `HIGHEST_POSSIBLE` 以获得最大速率）
- 网络延迟可能影响数据传输

### 位姿跳变或不连续
- 启用平滑以减少抖动
- 检查 IMU 校准质量
- 验证设备是否正确安装且未经历过度振动

## API 参考

有关详细的 API 文档，请参阅：
- [位姿增强教程](../../tutorials/PoseAugmentation_Tutorial_CN.md)
- [API 参考](../../doc/html/index.html)

## 相关演示

- [时间同步](../time_sync/README.zh-CN.md) - 同步时间戳
- [简单位姿](../simple_pose/README.zh-CN.md) - 基本位姿检索
- [IMU 获取器](../imu_fetcher/README.zh-CN.md) - 原始 IMU 数据访问
