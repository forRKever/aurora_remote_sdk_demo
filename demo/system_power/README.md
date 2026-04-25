# System Power Management Demo

## Overview

This demo demonstrates how to use the Aurora Remote SDK's power management API to control Aurora device power operations including reboot and shutdown.

## Features

- **Device Status**: Display device information before power operations
- **Reboot**: Restart the Aurora device
- **Shutdown**: Power off the Aurora device
- **Safety Confirmation**: Requires user confirmation before executing power operations

## Requirements

- Aurora device with firmware >= 2.1.1
- Aurora Remote SDK 2.1.1 or later

## Building

This demo is built automatically with the other demos:

```bash
cd build
cmake ..
make system_power
```

## Usage

```bash
./system_power <server_address> <command>
```

### Commands

| Command | Description |
|---------|-------------|
| `status` | Show device status without performing any power operation |
| `reboot` | Reboot the Aurora device |
| `shutdown` | Shutdown the Aurora device |

### Examples

```bash
# Check device status
./system_power 192.168.11.1 status

# Reboot the device
./system_power 192.168.11.1 reboot

# Shutdown the device
./system_power 192.168.11.1 shutdown
```

## Output Example

### Status Check

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

### Reboot Operation

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

## Safety Considerations

- **Data Loss**: Power operations may cause loss of unsaved data. Ensure all important data is saved before executing these commands.
- **Confirmation Required**: Both reboot and shutdown commands require user confirmation to prevent accidental execution.
- **Network Disconnection**: After a successful reboot or shutdown command, the network connection to the device will be lost.
- **Recovery Time**: After a reboot, the device typically takes 30-60 seconds to become available again.

## API Reference

This demo uses the following SDK API:

```cpp
bool requestPowerOperation(
    slamtec_aurora_sdk_power_operation_t operation,
    uint64_t timeout_ms = 5000,
    const void* reserved = nullptr,
    size_t reserved_size = 0,
    slamtec_aurora_sdk_errorcode_t* errcode = nullptr
);
```

### Power Operations

| Operation | Constant | Description |
|-----------|----------|-------------|
| Reboot | `SLAMTEC_AURORA_SDK_POWER_OP_REBOOT` | Restart the device |
| Shutdown | `SLAMTEC_AURORA_SDK_POWER_OP_SHUTDOWN` | Power off the device |

## Use Cases

1. **System Maintenance**: Reboot the device after configuration changes
2. **Firmware Updates**: Some updates may require a reboot to take effect
3. **Power Management**: Shutdown the device when not in use to save power
4. **Troubleshooting**: Reboot can help resolve temporary issues

## Troubleshooting

### Command Fails

- Ensure the device is connected and accessible
- Verify the network connection is stable
- Check that the firmware version supports power operations (>= 2.1.1)

### Device Does Not Reboot

- The command may have timed out before being processed
- Try increasing the timeout value
- Check device logs for any errors

### Cannot Reconnect After Reboot

- Wait at least 60 seconds for the device to fully boot
- Verify the device is powered on
- Check network connectivity

## Related Demos

- [Device Info Monitor](../device_info_monitor/README.md) - Monitor device status
- [Persistent Config](../persistent_config/README.md) - Manage device configuration
