# Aurora COLMAP 数据集记录演示

本演示展示如何使用 DataRecorder 机制从 Aurora 设备记录 COLMAP 兼容的数据集。COLMAP 是一个通用的运动结构恢复 (SfM) 和多视图立体视觉 (MVS) 管道，可用于 3D 重建。

## 功能特性

- **COLMAP 数据集记录**: 以 COLMAP 兼容格式记录图像流和相机位姿
- **灵活记录选项**: 可配置图像质量、文件格式和处理选项
- **设备发现**: 自动发现网络上的 Aurora 设备或连接到指定设备
- **实时状态监控**: 显示关键帧数量和记录进度
- **可配置输出**: 支持二进制、文本或两种输出格式
- **立体视觉支持**: 可选的立体图像记录以改善重建效果
- **图像处理**: 可选的图像去畸变和焦点中心调整

## 系统要求

- 带有相机系统的 Aurora 设备
- Aurora Remote SDK
- 主机与 Aurora 设备之间的网络连接
- 足够的存储空间用于数据集记录

## 使用方法

### 基本用法

```bash
# 使用默认设置记录数据集
./colmap_recorder --output /path/to/dataset

# 使用指定设备和超时时间记录
./colmap_recorder --output /path/to/dataset --device 192.168.1.100 --timeout 300

# 使用预览质量图像记录（压缩）
./colmap_recorder --output /path/to/dataset --image-quality preview
```

### 命令行参数

#### 必需参数
- `--output <folder>`: 存储记录数据集的文件夹路径

#### 可选参数
- `--device <ip>`: 设备 IP 地址（如未指定则自动发现）
- `--timeout <seconds>`: 记录超时时间（秒），0 表示无超时（默认）

#### 通用选项
- `--image-quality <type>`: 图像流类型
  - `raw`（默认）: 记录原始图像流。最佳质量但占用高带宽
  - `preview`: 记录预览图像流（压缩，带质量警告）

#### COLMAP 记录器选项
- `--stereo-recording`: 启用立体图像记录（默认：false）
- `--undistort`: 启用图像去畸变（默认：true）
- `--no-undistort`: 禁用图像去畸变
- `--force-focal-center`: 强制焦点中心到图像中心（默认：true）
- `--no-force-focal-center`: 不强制焦点中心到图像中心
- `--keep-unused-points`: 保留未使用的地图点（默认：false）
- `--multi-mapper`: 在 sparse/n/ 文件夹结构中存储数据（默认：false）
- `--file-format <format>`: 输出文件格式
  - `binary`（默认）: 以二进制格式存储数据（更快，更小）
  - `text`: 以文本格式存储数据（人类可读）
  - `all`: 同时以二进制和文本格式存储数据

## 输出示例

```
Dataset will be stored in: /tmp/colmap_dataset

Recording options:
  Image quality: raw
  Stereo recording: disabled
  Undistort images: enabled
  Force focal center: enabled
  Keep unused map points: disabled
  Multi-mapper: disabled
  File format: binary

Aurora SDK Version: 2.0.1-rc2
Device connection string not provided, trying to discover aurora devices...
Found 1 aurora devices
Device 0
  option 0: tcp://192.168.1.100:8090
Selected first device
Connecting to the selected device...
Connected to the selected device
Starting background map data syncing...
Configuring recording options...
Starting colmap dataset recording...
Recording started successfully
Press Ctrl+C to stop recording
Current keyframe count: 5
Current keyframe count: 12
Current keyframe count: 23
...
Ctrl-C pressed, stopping recording...
Stopping recording...
Recording stopped successfully
Dataset saved to: /tmp/colmap_dataset
```

## COLMAP 数据集结构

记录的数据集遵循 COLMAP 的标准目录结构：

```
dataset_folder/
├── images/              # 图像文件
│   ├── 000001.jpg
│   ├── 000002.jpg
│   └── ...
├── sparse/              # 稀疏重建数据
│   ├── cameras.bin      # 相机参数（二进制格式）
│   ├── images.bin       # 图像信息（二进制格式）
│   ├── points3D.bin     # 3D 点云（二进制格式）
│   ├── cameras.txt      # 相机参数（文本格式，如果 --file-format text/all）
│   ├── images.txt       # 图像信息（文本格式，如果 --file-format text/all）
│   └── points3D.txt     # 3D 点云（文本格式，如果 --file-format text/all）
└── sparse/0/            # 多重映射格式（如果启用 --multi-mapper）
    └── ...
```

## 集成示例

在应用程序中进行基本 COLMAP 数据集记录：

```cpp
#include "aurora_pubsdk_inc.h"

using namespace rp::standalone::aurora;

// 创建 SDK 会话并连接
RemoteSDK* sdk = RemoteSDK::CreateSession();
SDKServerConnectionDesc deviceDesc("tcp://192.168.1.100:8090");

if (sdk->connect(deviceDesc)) {
    // 启用后台地图数据同步（记录所需）
    sdk->startBackgroundMapDataSyncing();
    
    // 配置记录选项
    sdk->colmapDataRecorder.setOptionString("image_quality", "raw");
    sdk->colmapDataRecorder.setOptionBool("undistort", true);
    sdk->colmapDataRecorder.setOptionString("file_format", "binary");
    
    // 开始记录
    if (sdk->colmapDataRecorder.startRecording("/path/to/dataset")) {
        // 监控记录进度
        while (sdk->colmapDataRecorder.isRecording()) {
            int64_t keyframeCount = 0;
            if (sdk->colmapDataRecorder.queryStatusInt64("kf_count", &keyframeCount)) {
                std::cout << "已记录关键帧: " << keyframeCount << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // 停止记录
        sdk->colmapDataRecorder.stopRecording();
    }
    
    sdk->stopBackgroundMapDataSyncing();
}

sdk->disconnect();
sdk->release();
```

## 高级用法

### 高质量重建数据集

为获得最佳重建质量：

```bash
./colmap_recorder --output /path/to/dataset \
                  --image-quality raw \
                  --undistort \
                  --force-focal-center \
                  --file-format all
```

### 立体数据集记录

用于立体视觉应用：

```bash
./colmap_recorder --output /path/to/dataset \
                  --stereo-recording \
                  --undistort \
                  --file-format binary
```

### 多会话记录

对于需要多个映射会话的复杂场景：

```bash
./colmap_recorder --output /path/to/dataset \
                  --multi-mapper \
                  --keep-unused-points \
                  --file-format all
```

## COLMAP 处理

记录后，您可以使用 COLMAP 处理数据集：

```bash
# 自动重建
colmap automatic_reconstructor \
    --workspace_path /path/to/dataset \
    --image_path /path/to/dataset/images

# 手动重建管道
colmap feature_extractor --database_path /path/to/dataset/database.db \
                        --image_path /path/to/dataset/images

colmap exhaustive_matcher --database_path /path/to/dataset/database.db

colmap mapper --database_path /path/to/dataset/database.db \
              --image_path /path/to/dataset/images \
              --output_path /path/to/dataset/sparse
```

## 使用场景

- **3D 重建**: 从记录的图像序列创建详细的 3D 模型
- **运动结构恢复**: 生成相机位姿和稀疏点云
- **多视图立体视觉**: 从多个视角进行密集 3D 重建
- **SLAM 数据集创建**: 为 SLAM 算法开发和测试生成数据集
- **计算机视觉研究**: 为视觉算法评估创建自定义数据集
- **数字孪生创建**: 为数字孪生应用生成 3D 模型
- **摄影测量**: 专业测量和测绘应用
- **虚拟现实内容**: 从真实世界捕获创建 VR 环境

## 性能说明

- **存储要求**: 原始图像质量需要大量存储空间
- **网络带宽**: 实时记录需要稳定的高带宽连接
- **处理时间**: 图像去畸变会增加计算开销
- **内存使用**: 在长时间记录会话期间监控系统内存
- **关键帧选择**: 系统自动选择用于重建的最佳关键帧

## 获得最佳结果的技巧

1. **稳定连接**: 确保可靠的网络连接以避免数据丢失
2. **良好照明**: 在光照良好的环境中记录以获得更好的特征检测
3. **重叠**: 保持连续图像之间的足够重叠
4. **平稳运动**: 使用稳定、受控的相机运动
5. **纹理**: 包含纹理表面以获得更好的特征匹配
6. **覆盖**: 从多个角度和距离捕获场景