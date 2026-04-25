# Transform Manager Demo

This demo demonstrates how to use the Aurora SDK's TransformManager to manage coordinate frame transforms on the device.

## Features

- **List**: Display all available transforms with their values
- **Get**: Retrieve a specific transform by name
- **Set**: Update a transform with custom translation and rotation values
- **Reset**: Reset a transform to identity (zero translation, no rotation)
- **Refresh**: Reload transforms from the device

## Requirements

- Aurora device with firmware >= 2.1.1
- Aurora Remote SDK 2.1.1 or later

## Usage

```bash
transform_manager <server_address> <command> [args...]
```

### Commands

| Command | Description |
|---------|-------------|
| `list` | List all transforms with values |
| `get <name>` | Get a specific transform |
| `set <name> <tx> <ty> <tz> <qx> <qy> <qz> <qw>` | Set a transform |
| `reset <name>` | Reset a transform to identity |
| `refresh` | Refresh config from device |

### Transform Format

Transforms are represented as SE3 poses with:
- **Translation**: [tx, ty, tz] in meters
- **Quaternion**: [qx, qy, qz, qw] (Hamilton convention, w is the scalar part)

### Examples

List all transforms:
```bash
./transform_manager 192.168.11.1 list
```

Get a specific transform:
```bash
./transform_manager 192.168.11.1 get T_cam_imu
```

Set a custom transform (translation + quaternion):
```bash
./transform_manager 192.168.11.1 set T_custom 0.1 0.2 0.3 0 0 0 1
```

Reset a transform to identity:
```bash
./transform_manager 192.168.11.1 reset T_custom
```

Refresh transforms from device:
```bash
./transform_manager 192.168.11.1 refresh
```

## Common Transform Names

Typical transforms available on Aurora devices include:

| Name | Description |
|------|-------------|
| `T_cam_imu` | Camera to IMU transform |
| `T_body_cam` | Body frame to camera transform |
| `T_lidar_body` | LiDAR to body frame transform |

Note: Available transforms depend on your device configuration.

## API Reference

This demo uses the following SDK APIs:

- `RemoteSDK::createTransformManager()` - Create a TransformManager instance
- `TransformManager::getAllTransformNames()` - Get list of all transform names
- `TransformManager::getTransform()` - Get a transform by name
- `TransformManager::setTransform()` - Set a transform value
- `TransformManager::refresh()` - Reload transforms from device

## Troubleshooting

1. **"Failed to create TransformManager"**: Ensure the device firmware supports transform management (>= 2.1.1)
2. **"Failed to get transform"**: The specified transform name may not exist
3. **"Failed to set transform"**: The transform may be read-only or the name is invalid
