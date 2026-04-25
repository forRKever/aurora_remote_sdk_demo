# 数据记录仪演示

本演示程序提供一个交互式控制台，用于监控和控制 Aurora 设备的数据记录仪功能。

## 功能特性

- **实时仪表板**: 自动刷新的状态显示
- **录制控制**: 启用/禁用数据记录
- **存储管理**: 查看存储信息和会话历史
- **大小限制**: 配置最大录制大小
- **会话管理**: 使旧录制会话失效

## 先决条件

- Aurora 设备固件版本 >= 2.1.1
- Aurora Remote SDK 2.1.1 或更高版本

## 使用方法

```bash
dashcam_recorder <服务器地址>
```

### 示例

```bash
./dashcam_recorder 192.168.11.1
```

## 交互式控制

| 按键 | 操作 |
|------|------|
| `S` | 开始（启用）数据记录 |
| `T` | 停止（禁用）数据记录 |
| `L` | 设置存储大小限制（以 GB 为单位） |
| `I` | 使所有录制会话失效 |
| `R` | 手动刷新 |
| `Q` | 退出 |

## 仪表板显示

仪表板显示以下内容：

### 录制状态
- **Enabled（已启用）**: 行车记录是否已启用
- **Recording（录制中）**: 是否正在录制
- **Working State（工作状态）**: 当前操作状态
- **Status Message（状态消息）**: 描述性状态消息
- **Size Limit（大小限制）**: 允许的最大录制大小
- **Current Size（当前大小）**: 当前录制数据大小

### 存储信息
- **Storage Path（存储路径）**: 录制保存位置
- **External Storage（外部存储）**: 是否使用外部存储
- **Total Space（总空间）**: 总存储容量
- **Free Space（可用空间）**: 可用存储空间
- **Used by Datalogger（数据记录使用）**: 录制使用的空间

### 会话列表
- **Session ID（会话 ID）**: 唯一会话标识符
- **Blobs（数据块）**: 会话中的数据块数量
- **Start Time（开始时间）**: 录制开始时间
- **Duration（时长）**: 录制持续时间
- **Size（大小）**: 会话总大小

## 工作状态

| 状态 | 描述 |
|------|------|
| `UNKNOWN` | 状态未确定 |
| `INITIALIZING` | 系统正在启动 |
| `READY` | 准备录制（已启用但未录制） |
| `RECORDING` | 正在录制 |
| `ERROR_INIT` | 初始化错误 |
| `ERROR_STORAGE_FULL` | 存储已满 |
| `ERROR_WRITE_FAILED` | 写入操作失败 |

## API 参考

本演示使用以下 SDK C API：

- `slamtec_aurora_sdk_dashcam_recorder_get_status()` - 获取录制状态
- `slamtec_aurora_sdk_dashcam_recorder_set_enable()` - 启用/禁用录制
- `slamtec_aurora_sdk_dashcam_recorder_set_size_limit()` - 设置最大录制大小
- `slamtec_aurora_sdk_dashcam_recorder_get_storage_info()` - 获取存储详情
- `slamtec_aurora_sdk_dashcam_recorder_invalidate_sessions()` - 清除会话
- `slamtec_aurora_sdk_dashcam_storage_info_*` - 存储信息访问器函数

## 使用场景

1. **连续录制**: 启用数据记录仪以录制所有视觉数据供后续查看
2. **存储监控**: 跟踪可用存储并管理录制限制
3. **会话审查**: 查看录制历史和会话元数据
4. **维护**: 清除旧会话以释放存储空间
