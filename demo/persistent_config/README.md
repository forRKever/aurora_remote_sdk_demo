# Persistent Config Demo

This demo demonstrates how to use the Aurora SDK's PersistentConfigManager to manage device configuration entries.

## Features

- **Enumerate**: List all registered configuration entries on the device
- **Get**: Retrieve configuration values in JSON format
- **Set**: Update configuration values with JSON data
- **Reset**: Reset individual or all configurations to default values

## Requirements

- Aurora device with firmware >= 2.1.1
- Aurora Remote SDK 2.1.1 or later

## Usage

```bash
persistent_config <server_address> <command> [args...]
```

### Commands

| Command | Description |
|---------|-------------|
| `enum` | List all registered config entries |
| `get <filter_path> [-o <file>]` | Get config (optionally save to file) |
| `set <filter_path> <key> <json\|file>` | Set config with key |
| `reset <filter_path>` | Reset config to default |
| `reset-all` | Reset all configs to default |

### Examples

List all configuration entries:
```bash
./persistent_config 192.168.11.1 enum
```

Get a specific configuration:
```bash
./persistent_config 192.168.11.1 get recorder.dashcam
```

Save configuration to a file:
```bash
./persistent_config 192.168.11.1 get recorder.dashcam -o config.json
```

Set configuration from JSON string:
```bash
./persistent_config 192.168.11.1 set recorder.dashcam @overwrite '{"enabled":true}'
```

Set configuration from file:
```bash
./persistent_config 192.168.11.1 set recorder.dashcam @overwrite config.json
```

Reset a specific configuration:
```bash
./persistent_config 192.168.11.1 reset recorder.dashcam
```

Reset all configurations:
```bash
./persistent_config 192.168.11.1 reset-all
```

## Configuration Keys

When setting configuration, you can use different keys to control how the value is applied:

- `@overwrite`: Completely replace the existing configuration
- Other keys may be module-specific, refer to device documentation

## API Reference

This demo uses the following SDK APIs:

- `RemoteSDK::persistentConfig.enumAllEntries()` - List all config entry paths
- `RemoteSDK::persistentConfig.getConfig()` - Get config as JSON string
- `RemoteSDK::persistentConfig.setConfig()` - Set config with key and JSON value
- `RemoteSDK::persistentConfig.resetConfig()` - Reset specific config to default
- `RemoteSDK::persistentConfig.resetAllConfig()` - Reset all configs to default

## Troubleshooting

1. **"Failed to enumerate entries"**: Ensure the device firmware supports persistent config (>= 2.1.1)
2. **"Failed to set config"**: Check that the JSON format is valid and the filter path exists
3. **"Config is empty"**: The specified config path may not exist or has no value set
