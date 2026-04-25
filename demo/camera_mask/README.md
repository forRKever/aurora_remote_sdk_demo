# Camera Mask Demo

This demo demonstrates how to use the Aurora SDK's CameraMaskManager to manage static camera masks on the device.

## Features

- **Status**: Check if static camera masking is enabled
- **Enable/Disable**: Toggle static camera mask feature
- **List**: Display all camera indices that have masks configured
- **Get Mask**: Download a camera mask image and save to file (requires OpenCV)
- **Set Mask**: Upload a mask image for a specific camera (requires OpenCV)
- **Remove Mask**: Delete a camera mask
- **Refresh**: Reload mask configuration from device

## Use Cases

Camera masks are useful for:
- Excluding static obstacles in the camera's field of view
- Masking out areas with reflective surfaces
- Ignoring regions with moving objects (like robot arms)
- Improving SLAM accuracy by filtering problematic image regions

## Requirements

- Aurora device with firmware >= 2.1.1
- Aurora Remote SDK 2.1.1 or later
- OpenCV (optional, required for get-mask and set-mask commands)

## Usage

```bash
camera_mask <server_address> <command> [args...]
```

### Commands

| Command | Description |
|---------|-------------|
| `status` | Get mask enable status |
| `enable <0\|1>` | Enable/disable static mask |
| `list` | List camera indices with masks |
| `get-mask <camera_index> <output>` | Get mask image and save to PNG (requires OpenCV) |
| `set-mask <camera_index> <input>` | Set mask image from PNG (requires OpenCV) |
| `remove-mask <camera_index>` | Remove static mask for camera |
| `refresh` | Refresh config from device |

### Examples

Check mask status:
```bash
./camera_mask 192.168.11.1 status
```

Enable static masking:
```bash
./camera_mask 192.168.11.1 enable 1
```

Disable static masking:
```bash
./camera_mask 192.168.11.1 enable 0
```

List cameras with masks:
```bash
./camera_mask 192.168.11.1 list
```

Download mask for camera 0:
```bash
./camera_mask 192.168.11.1 get-mask 0 mask_cam0.png
```

Upload mask for camera 0:
```bash
./camera_mask 192.168.11.1 set-mask 0 mask_cam0.png
```

Remove mask for camera 0:
```bash
./camera_mask 192.168.11.1 remove-mask 0
```

## Mask Image Format

- **Format**: 8-bit grayscale PNG
- **Size**: Must match the camera resolution
- **Values**:
  - `0` (black): Masked region (excluded from processing)
  - `255` (white): Active region (included in processing)
  - Intermediate values may be used for soft masking

## Creating Mask Images

You can create mask images using any image editing software:

1. Create a grayscale image matching your camera resolution
2. Paint black (0) over areas to exclude
3. Leave white (255) for areas to include
4. Save as PNG

Example using ImageMagick:
```bash
# Create a blank white mask
convert -size 640x480 xc:white mask.png

# Add a black rectangle to mask out a region
convert mask.png -fill black -draw "rectangle 0,0 100,480" mask_left_blocked.png
```

## API Reference

This demo uses the following SDK APIs:

- `RemoteSDK::createCameraMaskManager()` - Create a CameraMaskManager instance
- `CameraMaskManager::isStaticMaskEnabled()` - Check if masking is enabled
- `CameraMaskManager::setStaticMaskEnable()` - Enable/disable masking
- `CameraMaskManager::getStaticCameraMaskImageIdList()` - List cameras with masks
- `CameraMaskManager::getStaticCameraMaskImage()` - Download a mask image
- `CameraMaskManager::setStaticCameraMaskImage()` - Upload a mask image
- `CameraMaskManager::removeStaticCameraMaskImage()` - Remove a mask
- `CameraMaskManager::refresh()` - Reload configuration from device

## Troubleshooting

1. **"Failed to create CameraMaskManager"**: Ensure firmware >= 2.1.1
2. **"Image must be 8-bit grayscale"**: Convert your mask to grayscale format
3. **"Failed to set mask image"**: Check that image dimensions match camera resolution
4. **Commands get-mask/set-mask not available**: OpenCV is required for image I/O
