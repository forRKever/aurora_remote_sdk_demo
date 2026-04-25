# Datalogger Demo

This demo provides an interactive console for monitoring and controlling the Aurora device's datalogger (dashcam recorder) functionality.

## Features

- **Real-time Dashboard**: Auto-refreshing status display
- **Recording Control**: Enable/disable datalogger recording
- **Storage Management**: View storage info and session history
- **Size Limit**: Configure maximum recording size
- **Session Management**: Invalidate old recording sessions

## Requirements

- Aurora device with firmware >= 2.1.1
- Aurora Remote SDK 2.1.1 or later

## Usage

```bash
dashcam_recorder <server_address>
```

### Example

```bash
./dashcam_recorder 192.168.11.1
```

## Interactive Controls

| Key | Action |
|-----|--------|
| `S` | Start (Enable) datalogger recording |
| `T` | Stop (Disable) datalogger recording |
| `L` | Set storage size limit (in GB) |
| `I` | Invalidate all recording sessions |
| `R` | Manual refresh |
| `Q` | Quit |

## Dashboard Display

The dashboard shows:

### Recording Status
- **Enabled**: Whether dashcam recording is enabled
- **Recording**: Whether actively recording
- **Working State**: Current operational state
- **Status Message**: Descriptive status message
- **Size Limit**: Maximum allowed recording size
- **Current Size**: Current recording data size

### Storage Information
- **Storage Path**: Where recordings are saved
- **External Storage**: Whether using external storage
- **Total Space**: Total storage capacity
- **Free Space**: Available storage space
- **Used by Datalogger**: Space used by recordings

### Session List
- **Session ID**: Unique session identifier
- **Blobs**: Number of data blobs in session
- **Start Time**: When recording started
- **Duration**: How long the recording lasted
- **Size**: Total size of the session

## Working States

| State | Description |
|-------|-------------|
| `UNKNOWN` | State not determined |
| `INITIALIZING` | System is starting up |
| `READY` | Ready to record (enabled but not recording) |
| `RECORDING` | Actively recording |
| `ERROR_INIT` | Initialization error |
| `ERROR_STORAGE_FULL` | Storage is full |
| `ERROR_WRITE_FAILED` | Write operation failed |

## API Reference

This demo uses the following SDK C APIs:

- `slamtec_aurora_sdk_dashcam_recorder_get_status()` - Get recording status
- `slamtec_aurora_sdk_dashcam_recorder_set_enable()` - Enable/disable recording
- `slamtec_aurora_sdk_dashcam_recorder_set_size_limit()` - Set max recording size
- `slamtec_aurora_sdk_dashcam_recorder_get_storage_info()` - Get storage details
- `slamtec_aurora_sdk_dashcam_recorder_invalidate_sessions()` - Clear sessions
- `slamtec_aurora_sdk_dashcam_storage_info_*` - Storage info accessor functions

## Use Cases

1. **Continuous Recording**: Enable datalogger to record all visual data for later review
2. **Storage Monitoring**: Track available storage and manage recording limits
3. **Session Review**: View recording history and session metadata
4. **Maintenance**: Clear old sessions to free up storage space

