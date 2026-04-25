# 变换管理器演示

本演示程序展示如何使用 Aurora SDK 的 TransformManager 来管理设备上的坐标系变换。

## 功能特性

- **列表**: 显示所有可用的变换及其值
- **获取**: 按名称检索特定变换
- **设置**: 使用自定义平移和旋转值更新变换
- **重置**: 将变换重置为单位变换（零平移，无旋转）
- **刷新**: 从设备重新加载变换

## 先决条件

- Aurora 设备固件版本 >= 2.1.1
- Aurora Remote SDK 2.1.1 或更高版本

## 使用方法

```bash
transform_manager <服务器地址> <命令> [参数...]
```

### 命令

| 命令 | 描述 |
|------|------|
| `list` | 列出所有变换及其值 |
| `get <名称>` | 获取特定变换 |
| `set <名称> <tx> <ty> <tz> <qx> <qy> <qz> <qw>` | 设置变换 |
| `reset <名称>` | 将变换重置为单位变换 |
| `refresh` | 从设备刷新配置 |

### 变换格式

变换以 SE3 位姿表示：
- **平移**: [tx, ty, tz] 单位为米
- **四元数**: [qx, qy, qz, qw]（Hamilton 约定，w 是标量部分）

### 示例

列出所有变换：
```bash
./transform_manager 192.168.11.1 list
```

获取特定变换：
```bash
./transform_manager 192.168.11.1 get T_cam_imu
```

设置自定义变换（平移 + 四元数）：
```bash
./transform_manager 192.168.11.1 set T_custom 0.1 0.2 0.3 0 0 0 1
```

将变换重置为单位变换：
```bash
./transform_manager 192.168.11.1 reset T_custom
```

从设备刷新变换：
```bash
./transform_manager 192.168.11.1 refresh
```

## 常用变换名称

Aurora 设备上通常可用的变换包括：

| 名称 | 描述 |
|------|------|
| `T_cam_imu` | 相机到 IMU 的变换 |
| `T_body_cam` | 机体坐标系到相机的变换 |
| `T_lidar_body` | 激光雷达到机体坐标系的变换 |

注意：可用的变换取决于您的设备配置。

## API 参考

本演示使用以下 SDK API：

- `RemoteSDK::createTransformManager()` - 创建 TransformManager 实例
- `TransformManager::getAllTransformNames()` - 获取所有变换名称列表
- `TransformManager::getTransform()` - 按名称获取变换
- `TransformManager::setTransform()` - 设置变换值
- `TransformManager::refresh()` - 从设备重新加载变换

## 故障排除

1. **"Failed to create TransformManager"**: 确保设备固件支持变换管理（>= 2.1.1）
2. **"Failed to get transform"**: 指定的变换名称可能不存在
3. **"Failed to set transform"**: 变换可能是只读的或名称无效
