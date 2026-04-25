# 位姿协方差演示

## 概述

本演示程序展示如何从 Aurora 设备检索和解释位姿协方差数据。位姿协方差提供设备位置和方向的不确定性估计，这对于需要评估定位置信度的应用至关重要。

## 功能特性

- **位置不确定性**：位置估计的 95% 置信椭球
- **旋转不确定性**：横滚、俯仰、偏航的 1-sigma 不确定性
- **质量评估**：自动分类定位质量
- **轮询和回调 API**：灵活的数据检索方法
- **人类可读格式**：易于解释的协方差指标

## 先决条件

- Aurora 设备连接到网络
- Aurora 固件版本 2.1.1 或更高（支持协方差）
- 设备必须运行并主动跟踪

## 构建

此演示程序会随其他演示一起自动构建：

```bash
cd build
cmake ..
make pose_covariance
```

## 使用方法

### 自动发现模式

```bash
./pose_covariance
```

这将自动发现并连接到网络上的 Aurora 设备。

### 指定设备地址

```bash
./pose_covariance 192.168.1.100
```

### 帮助

```bash
./pose_covariance --help
```

## 理解位姿协方差

### 什么是位姿协方差？

位姿协方差是一个 6x6 矩阵，表示 6 自由度位姿（3D 位置 + 3D 方向）的不确定性。它捕获：
- 每个自由度的不确定程度
- 不同自由度之间的相关性

SDK 提供便捷函数将此矩阵转换为人类可读的指标。

### 位置不确定性

**95% 置信椭球**：定义一个 3D 椭球，真实位置在其中的概率为 95%。

- **半轴**：椭球的三个主轴（以米为单位）
- **XY 半径**：XY 平面上 95% 置信圆的半径

示例：
```
位置 95% 椭球 (m): [0.0234, 0.0198, 0.0456]
位置 95% XY 半径 (m): 0.0298
```

这意味着：
- 我们有 95% 的信心位置在 XY 平面上 3cm 半径内
- 垂直不确定性稍高，约 4.5cm

### 旋转不确定性

**1-Sigma RPY**：每个旋转轴的一个标准差不确定性（以度为单位）。

示例：
```
旋转 1-sigma (deg): [0.45, 0.52, 1.23]
```

这意味着：
- 约 68% 的可能性实际横滚在估计值 ±0.45° 内
- 约 68% 的可能性实际俯仰在估计值 ±0.52° 内
- 约 68% 的可能性实际偏航在估计值 ±1.23° 内

### 质量指南

#### 位置质量
- **优秀** (< 5 cm)：适合精密对接、操作
- **良好** (< 10 cm)：适合一般导航、过门
- **一般** (< 30 cm)：适合粗略导航
- **较差** (> 30 cm)：考虑重定位

#### 旋转质量
- **优秀** (< 1°)：适合精确的方向相关任务
- **良好** (< 3°)：适合一般导航
- **一般** (< 5°)：可接受的粗略定位
- **较差** (> 5°)：可能需要重定位

## 输出示例

```
============================================================
Aurora 位姿协方差演示
============================================================
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

步骤 2: 等待设备开始跟踪...

步骤 3: 测试位姿协方差轮询 API...
  ✓ 成功检索位姿协方差
    时间戳: 123456789000 ns
    位置 95% XY 半径: 0.0298 m
    旋转 1-sigma (RPY): [0.45, 0.52, 1.23] deg

============================================================
监控位姿协方差更新
============================================================
演示程序将显示来自回调的协方差更新。
移动 Aurora 设备以观察不确定性如何变化。

按 Ctrl+C 停止

------------------------------------------------------------
位姿协方差更新 #1
------------------------------------------------------------
时间戳: 123456789000000 ns

位置不确定性（95% 置信）：
  半轴 (m): [0.0234, 0.0198, 0.0456]
  XY 半径 (m): 0.0298

旋转不确定性（1-sigma）：
  横滚:   0.4512 deg
  俯仰:   0.5234 deg
  偏航:   1.2345 deg

质量评估:
  位置: 优秀 (< 5 cm)
  旋转: 优秀 (< 1 deg)

------------------------------------------------------------
位姿协方差更新 #2
------------------------------------------------------------
时间戳: 123456890000000 ns

位置不确定性（95% 置信）：
  半轴 (m): [0.0245, 0.0209, 0.0467]
  XY 半径 (m): 0.0312

旋转不确定性（1-sigma）：
  横滚:   0.4678 deg
  俯仰:   0.5456 deg
  偏航:   1.2678 deg

质量评估:
  位置: 优秀 (< 5 cm)
  旋转: 优秀 (< 1 deg)

^C
按下 Ctrl-C，正在停止...

============================================================
正在关闭...
============================================================

统计信息:
  协方差更新总数: 150
  运行时间:       15 秒
  平均更新速率:   10.00 Hz

  ✓ 已断开与设备的连接

演示成功完成！
```

## 轮询与回调 API

### 轮询 API

按需使用协方差时使用：

```cpp
PoseCovariance covariance;
uint64_t timestamp = 0;

if (sdk->dataProvider.getRecentPoseCovariance(covariance, &timestamp)) {
    // 转换为可读格式
    PoseCovarianceReadable readable;
    if (covariance.toHumanReadable(readable)) {
        auto radius_xy = readable.getPositionRadius95XY();
        // 使用 radius_xy 评估位置质量
    }
}
```

### 回调 API

用于实时监控：

```cpp
class MyListener : public RemoteSDKListener {
    void onPoseCovariance(uint64_t timestamp_ns,
                         const PoseCovariance& covariance) override {
        // 处理协方差更新
        PoseCovarianceReadable readable;
        covariance.toHumanReadable(readable);
        // ...
    }
};
```

## 使用场景

### 自适应导航

根据定位置信度调整机器人行为：

```cpp
void onPoseCovariance(uint64_t timestamp_ns,
                     const PoseCovariance& covariance) {
    PoseCovarianceReadable readable;
    covariance.toHumanReadable(readable);

    double pos_uncertainty = readable.getPositionRadius95XY();

    if (pos_uncertainty > 0.30) {
        // 高不确定性 - 减速或触发重定位
        robot.reduceSpeed();
        robot.requestRelocalization();
    } else if (pos_uncertainty < 0.10) {
        // 良好定位 - 正常操作
        robot.normalSpeed();
    }
}
```

### 质量监控

记录定位质量以供分析：

```cpp
void logLocalizationQuality(const PoseCovariance& covariance) {
    PoseCovarianceReadable readable;
    covariance.toHumanReadable(readable);

    logger.log("pos_uncertainty_xy", readable.getPositionRadius95XY());
    auto rpy = readable.getRotation1SigmaRPY();
    logger.log("rot_uncertainty", rpy[0], rpy[1], rpy[2]);
}
```

### 传感器融合

在多传感器融合中对 Aurora 位姿加权：

```cpp
void fuseSensors(const PoseCovariance& aurora_cov,
                const Pose& other_sensor_pose) {
    PoseCovarianceReadable readable;
    aurora_cov.toHumanReadable(readable);

    double aurora_weight = 1.0 / readable.getPositionRadius95XY();
    // 在卡尔曼滤波器或其他融合算法中使用权重
}
```

## 影响协方差的因素

### 环境因素
- **丰富的特征**：较低的不确定性
- **无纹理区域**：较高的不确定性
- **动态物体**：可能增加不确定性

### 运动因素
- **缓慢、平稳的运动**：较低的不确定性
- **快速、急促的运动**：较高的不确定性
- **旋转**：可能增加旋转不确定性

### 跟踪状态
- **初始跟踪**：较高的不确定性
- **良好定位**：较低的不确定性
- **最近丢失并恢复**：暂时较高的不确定性

## 故障排除

### 协方差不可用
- 检查固件版本（需要 2.1.1+）
- 确保设备正在主动跟踪（未丢失）
- 连接后等待几秒钟数据累积

### 高不确定性值
- 检查环境是否有足够的视觉特征
- 验证设备是否正确校准
- 如果不确定性持续较高，考虑触发重定位

### 协方差更新不频繁
- 协方差更新遵循视觉跟踪频率（10-15 Hz）
- 这是预期行为（与高频的位姿增强不同）

## API 参考

有关详细的 API 文档，请参阅：
- [位姿协方差教程](../../tutorials/PoseCovariance_Tutorial_CN.md)
- [API 参考](../../doc/html/index.html)

## 相关演示

- [简单位姿](../simple_pose/README.zh-CN.md) - 基本位姿检索
- [重定位](../relocalization/README.zh-CN.md) - 不确定性高时触发重定位
- [位姿增强](../pose_augmentation/README.zh-CN.md) - 高频位姿更新
