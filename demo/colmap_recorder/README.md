# Aurora COLMAP Dataset Recorder Demo

This demo demonstrates how to use the DataRecorder mechanism to record COLMAP-compatible datasets from an Aurora device using the Aurora Remote SDK. COLMAP is a general-purpose Structure-from-Motion (SfM) and Multi-View Stereo (MVS) pipeline that can be used for 3D reconstruction.

## Features

- **COLMAP Dataset Recording**: Records image streams and camera poses in COLMAP-compatible format
- **Flexible Recording Options**: Configurable image quality, file formats, and processing options
- **Device Discovery**: Auto-discovers Aurora devices on the network or connects to a specific device
- **Real-time Status Monitoring**: Displays keyframe count and recording progress
- **Configurable Output**: Supports binary, text, or both output formats
- **Stereo Support**: Optional stereo image recording for improved reconstruction
- **Image Processing**: Optional image undistortion and focal center adjustment

## Requirements

- Aurora device with camera system
- Aurora Remote SDK
- Network connection between host and Aurora device
- Sufficient storage space for dataset recording

## Usage

### Basic Usage

```bash
# Record dataset with default settings
./colmap_recorder --output /path/to/dataset

# Record with specific device and timeout
./colmap_recorder --output /path/to/dataset --device 192.168.1.100 --timeout 300

# Record with preview quality images (compressed)
./colmap_recorder --output /path/to/dataset --image-quality preview
```

### Command Line Arguments

#### Required Arguments
- `--output <folder>`: Folder path to store the recorded dataset

#### Optional Arguments
- `--device <ip>`: Device IP address (auto-discover if not specified)
- `--timeout <seconds>`: Recording timeout in seconds (0 = no timeout, default)

#### Common Options
- `--image-quality <type>`: Image stream type
  - `raw` (default): Record raw image stream. Best quality but high bandwidth
  - `preview`: Record preview image stream (compressed, with quality warning)

#### COLMAP Recorder Options
- `--stereo-recording`: Enable stereo image recording (default: false)
- `--undistort`: Enable image undistortion (default: true)
- `--no-undistort`: Disable image undistortion
- `--force-focal-center`: Force focal center to image center (default: true)
- `--no-force-focal-center`: Do not force focal center to image center
- `--keep-unused-points`: Keep unused map points (default: false)
- `--multi-mapper`: Store data in sparse/n/ folder structure (default: false)
- `--file-format <format>`: Output file format
  - `binary` (default): Store data in binary format (faster, smaller)
  - `text`: Store data in text format (human-readable)
  - `all`: Store data in both binary and text formats

## Example Output

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

## COLMAP Dataset Structure

The recorded dataset follows COLMAP's standard directory structure:

```
dataset_folder/
├── images/              # Image files
│   ├── 000001.jpg
│   ├── 000002.jpg
│   └── ...
├── sparse/              # Sparse reconstruction data
│   ├── cameras.bin      # Camera parameters (binary format)
│   ├── images.bin       # Image information (binary format)
│   ├── points3D.bin     # 3D point cloud (binary format)
│   ├── cameras.txt      # Camera parameters (text format, if --file-format text/all)
│   ├── images.txt       # Image information (text format, if --file-format text/all)
│   └── points3D.txt     # 3D point cloud (text format, if --file-format text/all)
└── sparse/0/            # Multi-mapper format (if --multi-mapper enabled)
    └── ...
```

## Integration Example

Basic COLMAP dataset recording in your application:

```cpp
#include "aurora_pubsdk_inc.h"

using namespace rp::standalone::aurora;

// Create SDK session and connect
RemoteSDK* sdk = RemoteSDK::CreateSession();
SDKServerConnectionDesc deviceDesc("tcp://192.168.1.100:8090");

if (sdk->connect(deviceDesc)) {
    // Enable background map data syncing (required for recording)
    sdk->startBackgroundMapDataSyncing();
    
    // Configure recording options
    sdk->colmapDataRecorder.setOptionString("image_quality", "raw");
    sdk->colmapDataRecorder.setOptionBool("undistort", true);
    sdk->colmapDataRecorder.setOptionString("file_format", "binary");
    
    // Start recording
    if (sdk->colmapDataRecorder.startRecording("/path/to/dataset")) {
        // Monitor recording progress
        while (sdk->colmapDataRecorder.isRecording()) {
            int64_t keyframeCount = 0;
            if (sdk->colmapDataRecorder.queryStatusInt64("kf_count", &keyframeCount)) {
                std::cout << "Keyframes recorded: " << keyframeCount << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Stop recording
        sdk->colmapDataRecorder.stopRecording();
    }
    
    sdk->stopBackgroundMapDataSyncing();
}

sdk->disconnect();
sdk->release();
```

## Advanced Usage

### High-Quality Reconstruction Dataset

For best reconstruction quality:

```bash
./colmap_recorder --output /path/to/dataset \
                  --image-quality raw \
                  --undistort \
                  --force-focal-center \
                  --file-format all
```

### Stereo Dataset Recording

For stereo vision applications:

```bash
./colmap_recorder --output /path/to/dataset \
                  --stereo-recording \
                  --undistort \
                  --file-format binary
```

### Multi-Session Recording

For complex scenes requiring multiple mapping sessions:

```bash
./colmap_recorder --output /path/to/dataset \
                  --multi-mapper \
                  --keep-unused-points \
                  --file-format all
```

## COLMAP Processing

After recording, you can process the dataset with COLMAP:

```bash
# Automatic reconstruction
colmap automatic_reconstructor \
    --workspace_path /path/to/dataset \
    --image_path /path/to/dataset/images

# Manual reconstruction pipeline
colmap feature_extractor --database_path /path/to/dataset/database.db \
                        --image_path /path/to/dataset/images

colmap exhaustive_matcher --database_path /path/to/dataset/database.db

colmap mapper --database_path /path/to/dataset/database.db \
              --image_path /path/to/dataset/images \
              --output_path /path/to/dataset/sparse
```

## Use Cases

- **3D Reconstruction**: Create detailed 3D models from recorded image sequences
- **Structure from Motion**: Generate camera poses and sparse point clouds
- **Multi-View Stereo**: Dense 3D reconstruction from multiple viewpoints
- **SLAM Dataset Creation**: Generate datasets for SLAM algorithm development and testing
- **Computer Vision Research**: Create custom datasets for vision algorithm evaluation
- **Digital Twin Creation**: Generate 3D models for digital twin applications
- **Photogrammetry**: Professional surveying and measurement applications
- **Virtual Reality Content**: Create VR environments from real-world captures

## Performance Notes

- **Storage Requirements**: Raw image quality requires significant storage space
- **Network Bandwidth**: Real-time recording requires stable, high-bandwidth connection
- **Processing Time**: Image undistortion adds computational overhead
- **Memory Usage**: Monitor system memory during long recording sessions
- **Keyframe Selection**: The system automatically selects optimal keyframes for reconstruction

## Tips for Best Results

1. **Stable Connection**: Ensure reliable network connection to avoid data loss
2. **Good Lighting**: Record in well-lit environments for better feature detection
3. **Overlap**: Maintain sufficient overlap between consecutive images
4. **Smooth Motion**: Use steady, controlled camera movements
5. **Texture**: Include textured surfaces for better feature matching
6. **Coverage**: Capture the scene from multiple angles and distances