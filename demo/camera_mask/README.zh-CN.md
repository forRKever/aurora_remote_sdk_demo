# 相机遮罩演示

本演示程序展示如何使用 Aurora SDK 的 CameraMaskManager 来管理设备上的静态相机遮罩。

## 功能特性

- **状态**: 检查静态相机遮罩是否启用
- **启用/禁用**: 切换静态相机遮罩功能
- **列表**: 显示所有已配置遮罩的相机索引
- **获取遮罩**: 下载相机遮罩图像并保存到文件（需要 OpenCV）
- **设置遮罩**: 为特定相机上传遮罩图像（需要 OpenCV）
- **移除遮罩**: 删除相机遮罩
- **刷新**: 从设备重新加载遮罩配置

## 使用场景

相机遮罩适用于：
- 排除相机视野中的静态障碍物
- 遮挡具有反射表面的区域
- 忽略有移动物体的区域（如机械臂）
- 通过过滤有问题的图像区域来提高 SLAM 精度

## 先决条件

- Aurora 设备固件版本 >= 2.1.1
- Aurora Remote SDK 2.1.1 或更高版本
- OpenCV（可选，get-mask 和 set-mask 命令需要）

## 使用方法

```bash
camera_mask <服务器地址> <命令> [参数...]
```

### 命令

| 命令 | 描述 |
|------|------|
| `status` | 获取遮罩启用状态 |
| `enable <0\|1>` | 启用/禁用静态遮罩 |
| `list` | 列出有遮罩的相机索引 |
| `get-mask <相机索引> <输出文件>` | 获取遮罩图像并保存为 PNG（需要 OpenCV） |
| `set-mask <相机索引> <输入文件>` | 从 PNG 设置遮罩图像（需要 OpenCV） |
| `remove-mask <相机索引>` | 移除相机的静态遮罩 |
| `refresh` | 从设备刷新配置 |

### 示例

检查遮罩状态：
```bash
./camera_mask 192.168.11.1 status
```

启用静态遮罩：
```bash
./camera_mask 192.168.11.1 enable 1
```

禁用静态遮罩：
```bash
./camera_mask 192.168.11.1 enable 0
```

列出有遮罩的相机：
```bash
./camera_mask 192.168.11.1 list
```

下载相机 0 的遮罩：
```bash
./camera_mask 192.168.11.1 get-mask 0 mask_cam0.png
```

上传相机 0 的遮罩：
```bash
./camera_mask 192.168.11.1 set-mask 0 mask_cam0.png
```

移除相机 0 的遮罩：
```bash
./camera_mask 192.168.11.1 remove-mask 0
```

## 遮罩图像格式

- **格式**: 8 位灰度 PNG
- **尺寸**: 必须与相机分辨率匹配
- **数值**:
  - `0`（黑色）: 遮罩区域（从处理中排除）
  - `255`（白色）: 活动区域（包含在处理中）
  - 中间值可用于软遮罩

## 创建遮罩图像

您可以使用任何图像编辑软件创建遮罩图像：

1. 创建与相机分辨率匹配的灰度图像
2. 在要排除的区域涂上黑色（0）
3. 要包含的区域保持白色（255）
4. 保存为 PNG 格式

使用 ImageMagick 的示例：
```bash
# 创建空白白色遮罩
convert -size 640x480 xc:white mask.png

# 添加黑色矩形以遮挡区域
convert mask.png -fill black -draw "rectangle 0,0 100,480" mask_left_blocked.png
```

## API 参考

本演示使用以下 SDK API：

- `RemoteSDK::createCameraMaskManager()` - 创建 CameraMaskManager 实例
- `CameraMaskManager::isStaticMaskEnabled()` - 检查遮罩是否启用
- `CameraMaskManager::setStaticMaskEnable()` - 启用/禁用遮罩
- `CameraMaskManager::getStaticCameraMaskImageIdList()` - 列出有遮罩的相机
- `CameraMaskManager::getStaticCameraMaskImage()` - 下载遮罩图像
- `CameraMaskManager::setStaticCameraMaskImage()` - 上传遮罩图像
- `CameraMaskManager::removeStaticCameraMaskImage()` - 移除遮罩
- `CameraMaskManager::refresh()` - 从设备重新加载配置

## 故障排除

1. **"Failed to create CameraMaskManager"**: 确保固件版本 >= 2.1.1
2. **"Image must be 8-bit grayscale"**: 将遮罩转换为灰度格式
3. **"Failed to set mask image"**: 检查图像尺寸是否与相机分辨率匹配
4. **get-mask/set-mask 命令不可用**: 图像 I/O 需要 OpenCV
