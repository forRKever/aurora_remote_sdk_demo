# 持久化配置演示

本演示程序展示如何使用 Aurora SDK 的 PersistentConfigManager 来管理设备配置条目。

## 功能特性

- **枚举**: 列出设备上所有已注册的配置条目
- **获取**: 以 JSON 格式检索配置值
- **设置**: 使用 JSON 数据更新配置值
- **重置**: 将单个或所有配置重置为默认值

## 先决条件

- Aurora 设备固件版本 >= 2.1.1
- Aurora Remote SDK 2.1.1 或更高版本

## 使用方法

```bash
persistent_config <服务器地址> <命令> [参数...]
```

### 命令

| 命令 | 描述 |
|------|------|
| `enum` | 列出所有已注册的配置条目 |
| `get <过滤路径> [-o <文件>]` | 获取配置（可选保存到文件） |
| `set <过滤路径> <键> <json\|文件>` | 使用键设置配置 |
| `reset <过滤路径>` | 将配置重置为默认值 |
| `reset-all` | 将所有配置重置为默认值 |

### 示例

列出所有配置条目：
```bash
./persistent_config 192.168.11.1 enum
```

获取特定配置：
```bash
./persistent_config 192.168.11.1 get recorder.dashcam
```

将配置保存到文件：
```bash
./persistent_config 192.168.11.1 get recorder.dashcam -o config.json
```

从 JSON 字符串设置配置：
```bash
./persistent_config 192.168.11.1 set recorder.dashcam @overwrite '{"enabled":true}'
```

从文件设置配置：
```bash
./persistent_config 192.168.11.1 set recorder.dashcam @overwrite config.json
```

重置特定配置：
```bash
./persistent_config 192.168.11.1 reset recorder.dashcam
```

重置所有配置：
```bash
./persistent_config 192.168.11.1 reset-all
```

## 配置键

设置配置时，可以使用不同的键来控制值的应用方式：

- `@overwrite`: 完全替换现有配置
- 其他键可能是模块特定的，请参阅设备文档

## API 参考

本演示使用以下 SDK API：

- `RemoteSDK::persistentConfig.enumAllEntries()` - 列出所有配置条目路径
- `RemoteSDK::persistentConfig.getConfig()` - 以 JSON 字符串获取配置
- `RemoteSDK::persistentConfig.setConfig()` - 使用键和 JSON 值设置配置
- `RemoteSDK::persistentConfig.resetConfig()` - 将特定配置重置为默认值
- `RemoteSDK::persistentConfig.resetAllConfig()` - 将所有配置重置为默认值

## 故障排除

1. **"Failed to enumerate entries"**: 确保设备固件支持持久化配置（>= 2.1.1）
2. **"Failed to set config"**: 检查 JSON 格式是否有效以及过滤路径是否存在
3. **"Config is empty"**: 指定的配置路径可能不存在或没有设置值
