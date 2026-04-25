# 时间同步演示

## 概述

本演示程序展示如何使用 Aurora Remote SDK 的时间同步功能：

1. **稳定时钟同步**：将 Aurora 设备时间域的时间戳转换为本地计算机时间域。对于将 Aurora 传感器数据与外部传感器或事件关联至关重要。

2. **墙上时钟同步**：将 Aurora 设备的系统时钟与本地计算机时间同步。用于确保设备具有准确的绝对时间，便于日志记录、文件时间戳等。

## 功能特性

- **稳定时钟同步** - 将 Aurora 时间戳转换为本地时间域
- **墙上时钟同步** - 将设备系统时间与客户端时间同步
- **亚毫秒级精度** 在正常网络条件下
- **后台同步** 持续保持精度
- **质量指标** 监控同步性能
- **实用演示** - 转换真实位姿和 IMU 时间戳
- **实时数据监控** - 并排显示 Aurora 时间和本地时间

## 先决条件

- Aurora 设备连接到网络
- Aurora 固件版本 2.1.1 或更高
- Aurora 设备必须运行并可访问

## 构建

此演示程序会随其他演示一起自动构建：

```bash
cd build
cmake ..
make time_sync
```

## 使用方法

### 命令

```bash
./time_sync <命令> [服务器地址] [端口]
```

| 命令 | 描述 |
|------|------|
| `steady` | 运行稳定时钟同步演示（时间戳转换）|
| `wallclock` | 运行墙上时钟同步演示（设备时间同步）|
| `all` | 运行两个演示（默认）|

### 示例

```bash
# 使用自动发现运行两个演示
./time_sync

# 仅运行稳定时钟同步演示
./time_sync steady

# 仅运行墙上时钟同步演示
./time_sync wallclock 192.168.1.100

# 使用指定地址和端口运行两个演示
./time_sync all 192.168.1.100 9527
```

### 帮助

```bash
./time_sync --help
```

## 工作原理

### 稳定时钟同步演示

稳定时钟演示建立时间同步并转换位姿和 IMU 数据的时间戳：

1. **创建客户端**：使用 `TimeSyncDomain::STEADY_CLOCK` 创建时间同步客户端
2. **连接**：连接到 Aurora 设备时间同步服务
3. **配置**：设置同步选项（间隔、采样大小等）
4. **初始化**：执行初始同步握手
5. **等待同步**：等待直到收集到足够的样本
6. **显示质量**：显示同步质量指标（RMSE、最大误差等）
7. **转换数据**：实时检索位姿和 IMU 数据并转换时间戳

### 墙上时钟同步演示

墙上时钟演示将 Aurora 设备的系统时钟与客户端时间同步：

1. **创建客户端**：使用 `TimeSyncDomain::WALL_CLOCK` 创建时间同步客户端
2. **连接**：连接到 Aurora 设备时间同步服务
3. **查询偏移**：获取客户端和设备之间当前的墙上时钟偏移
4. **同步时钟**：如果偏移 > 1ms，同步设备的系统时钟
5. **评估精度**：使用多个样本测量同步精度
6. **显示结果**：显示平均/最大误差和质量评估

### 关键概念

#### 时间域

- **稳定时钟**：单调时钟，永不倒退（推荐用于时间戳转换）
- **墙上时钟**：系统时钟（用于同步设备系统时间）

#### 何时使用

| 使用场景 | 推荐域 |
|----------|--------|
| 关联传感器时间戳 | 稳定时钟 |
| 多传感器融合 | 稳定时钟 |
| 同步设备时间 | 墙上时钟 |
| 设备上准确的文件时间戳 | 墙上时钟 |

#### 质量指标（稳定时钟）

- **RMSE**：同步的均方根误差
- **最大误差**：采样窗口中观察到的最大误差
- **缩放因子**：客户端和服务器之间的时钟速度比
- **偏移量**：两个时钟之间的时间偏移

#### 时间戳转换（稳定时钟）

一旦同步，您可以将任何 Aurora 时间戳转换为本地时间域：

```cpp
uint64_t aurora_timestamp = ...; // 来自跟踪帧、IMU 数据等
uint64_t local_timestamp;
if (client.translateTimestamp(aurora_timestamp, &local_timestamp)) {
    // 使用 local_timestamp
}
```

#### 墙上时钟同步

同步设备的系统时钟：

```cpp
// 查询当前偏移
slamtec_aurora_sdk_wallclock_offset_result_t offset_result;
client.getWallClockOffset(1000, &offset_result);

// 如需要则同步
slamtec_aurora_sdk_wallclock_sync_result_t sync_result;
slamtec_aurora_sdk_errorcode_t error;
client.syncServerWallClock(1000, &sync_result, &error);

// 评估精度
slamtec_aurora_sdk_wallclock_accuracy_result_t accuracy;
client.evaluateWallClockSyncAccuracy(1000, &accuracy);
```

## 输出示例

### 稳定时钟同步输出

```
=============================================
Steady Clock Synchronization Demo
=============================================
This demo establishes time synchronization and translates
timestamps from pose and IMU data.

步骤 1: 创建时间同步客户端...
  ✓ 客户端已创建

步骤 2: 连接到时间同步服务...
  地址: 192.168.1.100:9527
  ✓ 成功连接

步骤 3: 配置同步选项...
  ✓ 选项已配置

步骤 4: 初始化时间同步...
  ✓ 初始化成功

步骤 5: 等待同步...
  ✓ 同步完成！

========================================
同步质量指标
========================================
  RMSE:              0.523 ms
  最大误差:          1.234 ms
  缩放因子:          1.000000123
  偏移量:            -125.456 ms
  使用的样本:        10/12

  评估: 优秀 (< 1ms)

[位姿 #10]
  位置:            (1.234, 0.567, 0.089) m
  Aurora 时间:     123456789000000 ns
  本地时间:        123456663544000 ns
  时间偏移:        -125.456 ms

[IMU #100]
  加速度 (g):      (0.0123, -0.0045, 0.9876)
  陀螺仪 (dps):    (0.234, -0.123, 0.567)
  Aurora 时间:     123456790000000 ns
  本地时间:        123456664544000 ns
  时间偏移:        -125.456 ms
```

### 墙上时钟同步输出

```
=============================================
Wall Clock Synchronization Demo
=============================================
This demo synchronizes the Aurora device's system clock
with your local machine's time.

步骤 1: 创建墙上时钟同步客户端
  ✓ 已连接到 192.168.1.100:9527

步骤 2: 查询墙上时钟偏移...
  偏移:     125.456 ms
  RTT:      2.345 ms
  状态:     检测到较大偏移 - 建议同步

步骤 3: 同步设备墙上时钟...
  ✓ 墙上时钟同步成功！
  应用的偏移: 125.456 ms

步骤 4: 评估同步精度...
  样本 1: 误差 = 0.234 ms
  样本 2: 误差 = 0.312 ms
  样本 3: 误差 = 0.287 ms
  ...

同步精度统计:
  平均误差: 0.278 ms
  最大误差: 0.456 ms
  评估:     优秀 (< 1ms)

  ✓ 墙上时钟同步演示完成！
```

## 与其他功能集成

时间同步对于将 Aurora 数据与外部传感器或事件关联至关重要：

### 示例 1：转换位姿时间戳

```cpp
// 获取带有 Aurora 时间戳的位姿
slamtec_aurora_sdk_pose_se3_t pose;
uint64_t aurora_timestamp;
sdk->dataProvider.getCurrentPoseSE3WithTimestamp(pose, aurora_timestamp);

// 转换为本地时间
uint64_t local_timestamp;
if (timeSync.translateTimestamp(aurora_timestamp, &local_timestamp)) {
    // 现在可以与本地事件/传感器关联
    correlateWithExternalSensor(local_timestamp, pose);
}
```

### 示例 2：转换 IMU 时间戳

```cpp
// 获取带有 Aurora 时间戳的 IMU 数据
std::vector<slamtec_aurora_sdk_imu_data_t> imuData;
if (sdk->dataProvider.peekIMUData(imuData)) {
    for (auto& imu : imuData) {
        uint64_t local_timestamp;
        if (timeSync.translateTimestamp(imu.timestamp_ns, &local_timestamp)) {
            // 使用同步的时间戳进行传感器融合
            fuseIMUData(local_timestamp, imu.acc, imu.gyro);
        }
    }
}
```

### 示例 3：多传感器融合

```cpp
// 带有转换时间戳的 Aurora 位姿
slamtec_aurora_sdk_pose_se3_t aurora_pose;
uint64_t aurora_time, aurora_local_time;
sdk->dataProvider.getCurrentPoseSE3WithTimestamp(aurora_pose, aurora_time);
timeSync.translateTimestamp(aurora_time, &aurora_local_time);

// 带有本地时间戳的外部传感器读数
uint64_t lidar_timestamp = getCurrentLocalTime();
LidarScan lidar_scan = getLidarScan();

// 现在两者都在相同的时间域 - 可以融合
if (abs(aurora_local_time - lidar_timestamp) < 50000000) {  // 50ms 内
    fuseSensorData(aurora_pose, lidar_scan);
}
```

## 故障排除

### 同步失败

- 检查与 Aurora 设备的网络连接
- 验证端口（默认：9527）未被阻止
- 检查 Aurora 固件是否支持时间同步（需要：v2.1.1+）

### 精度差（> 5ms）

- 检查网络延迟和抖动
- 增加采样窗口大小
- 减少同步间隔以进行更频繁的更新

### 连接被拒绝

- 确保 Aurora 设备已开机并已连接
- 验证 IP 地址是否正确
- 检查防火墙设置

### 墙上时钟同步失败

- 设备必须有足够的权限修改系统时间
- 在 Linux 上：使用 `sudo` 运行设备服务
- 在 Windows 上：以管理员身份运行
- 某些设备可能不支持系统时钟修改

## API 参考

有关详细的 API 文档，请参阅：
- [时间同步教程](../../tutorials/TimeSync_Tutorial_CN.md)
- [API 参考](../../doc/html/index.html)

## 相关演示

- [位姿增强](../pose_augmentation/README.zh-CN.md) - 使用时间同步实现高频位姿输出
- [简单位姿](../simple_pose/README.zh-CN.md) - 基本位姿检索
