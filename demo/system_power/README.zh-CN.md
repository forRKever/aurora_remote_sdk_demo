# 系统电源管理演示

## 概述

本演示程序展示如何使用 Aurora Remote SDK 的电源管理 API 来控制 Aurora 设备的电源操作，包括重启和关机。

## 功能特性

- **设备状态**: 在执行电源操作前显示设备信息
- **重启**: 重新启动 Aurora 设备
- **关机**: 关闭 Aurora 设备电源
- **安全确认**: 执行电源操作前需要用户确认

## 先决条件

- Aurora 设备固件版本 >= 2.1.1
- Aurora Remote SDK 2.1.1 或更高版本

## 构建

此演示程序会随其他演示一起自动构建：

```bash
cd build
cmake ..
make system_power
```

## 使用方法

```bash
./system_power <服务器地址> <命令>
```

### 命令

| 命令 | 描述 |
|------|------|
| `status` | 显示设备状态，不执行任何电源操作 |
| `reboot` | 重启 Aurora 设备 |
| `shutdown` | 关闭 Aurora 设备 |

### 示例

```bash
# 检查设备状态
./system_power 192.168.11.1 status

# 重启设备
./system_power 192.168.11.1 reboot

# 关闭设备
./system_power 192.168.11.1 shutdown
```

## 输出示例

### 状态检查

```
Aurora System Power Management Demo
====================================
SDK Version: 2.1.1-rtm
Info: Creating SDK session...
Info: Connecting to Aurora device at 192.168.11.1...
Success: Connected to Aurora device

Device Information
==================
  Device Name:      Aurora-XXXX
  Serial Number:    AU2XXXXXXXXX
  Device Model:     2.0.1
  Firmware Version: 2.1.1
  Build Date:       Dec 15 2024 10:30:00

Info: Status check complete. No power operation performed.
```

### 重启操作

```
Aurora System Power Management Demo
====================================
SDK Version: 2.1.1-rtm
Info: Creating SDK session...
Info: Connecting to Aurora device at 192.168.11.1...
Success: Connected to Aurora device

Device Information
==================
  Device Name:      Aurora-XXXX
  Serial Number:    AU2XXXXXXXXX
  Device Model:     2.0.1
  Firmware Version: 2.1.1
  Build Date:       Dec 15 2024 10:30:00

Warning: You are about to REBOOT the Aurora device.
This operation cannot be undone.

Type 'yes' to confirm: yes
Info: Sending reboot command...

Reboot command sent.
The device will reboot shortly.
Please wait for the device to come back online.
This typically takes 30-60 seconds.
```

## 安全注意事项

- **数据丢失**: 电源操作可能导致未保存的数据丢失。在执行这些命令之前，请确保所有重要数据已保存。
- **需要确认**: 重启和关机命令都需要用户确认，以防止意外执行。
- **网络断开**: 成功发送重启或关机命令后，与设备的网络连接将断开。
- **恢复时间**: 重启后，设备通常需要 30-60 秒才能再次可用。

## API 参考

本演示使用以下 SDK API：

```cpp
bool requestPowerOperation(
    slamtec_aurora_sdk_power_operation_t operation,
    uint64_t timeout_ms = 5000,
    const void* reserved = nullptr,
    size_t reserved_size = 0,
    slamtec_aurora_sdk_errorcode_t* errcode = nullptr
);
```

### 电源操作

| 操作 | 常量 | 描述 |
|------|------|------|
| 重启 | `SLAMTEC_AURORA_SDK_POWER_OP_REBOOT` | 重新启动设备 |
| 关机 | `SLAMTEC_AURORA_SDK_POWER_OP_SHUTDOWN` | 关闭设备电源 |

## 使用场景

1. **系统维护**: 配置更改后重启设备
2. **固件更新**: 某些更新可能需要重启才能生效
3. **电源管理**: 不使用时关闭设备以节省电源
4. **故障排除**: 重启可以帮助解决临时问题

## 故障排除

### 命令失败

- 确保设备已连接且可访问
- 验证网络连接是否稳定
- 检查固件版本是否支持电源操作（>= 2.1.1）

### 设备未重启

- 命令可能在处理前已超时
- 尝试增加超时值
- 检查设备日志是否有任何错误

### 重启后无法重新连接

- 等待至少 60 秒让设备完全启动
- 验证设备是否已开机
- 检查网络连接

## 相关演示

- [设备信息监控](../device_info_monitor/README.zh-CN.md) - 监控设备状态
- [持久配置](../persistent_config/README.zh-CN.md) - 管理设备配置
