# SLAMTEC Aurora Remote SDK and Demo
Demos and SDK for SLAMTEC Aurora

### V1.1.0-rc1: Beta version, 23th Oct 2024
Initial release. 

The SDK supports the following platforms and architectures:
- Windows 10/11 (x86_64)
- Ubuntu 20.04 and higher versions with glibc 2.31 and above (aarch64, x86_64)
- Any Linux system with glibc 2.31 and above (aarch64, x86_64)

### V1.1.0-rc2: Beta version, 26th Oct 2024
- Refined some APIs
- Added API reference documentation


### V1.2.0-rc2:
- Added LIDAR Scan Data Retrieval and Rendering Demo
- Added LIDAR 2D Map Rendering Demo
- Fixed the issue of drifting when stationary

### V1.2.0-rc4:
- Support loading maps in asb format
- Added the API to enable and disable loop closure detection

### V1.2.0-rc5:

- Resolve the issue of map upload failure

### V1.2.0-rc6:

- Added support for neo pure localization mode

### V2.0.0-alpha:

- Added support for FW 2.0 features
  - Depth Camera
  - Semantic Segmentation
  - Camera Calibration Exporting
  - Basic Device Info Monitor

### V2.0.0-beta1:

- Added Timestamp enabled get pose API

### V2.0.1-beta1:

- Added depth camera post filtering support
- Added Keyframe/Map point fetching control APIs- Added support for neo pure localization mode

### V2.0.1-rc2:

- Refined some APIs
- added reloc status retriving interface


### V2.1.0-rc1:

- updated the demo code to support RGB image
- Refined local relocalization API
- Merged with the enhanced imaging demo code

### V2.1.0-rtm

- improved  the vslam map saving and loading operation speed by about ~2x
- minor bug fixes

### V2.1.1 (Upcoming)

**New Features (SDK 2.1):**

- **Time Synchronization**: Software-based time synchronization feature for accurate timestamp translation between Aurora device and client system
  - Sub-millisecond accuracy under normal network conditions
  - New demo: `time_sync` - demonstrates time sync setup and usage
  - Tutorial: `tutorials/TimeSync_Tutorial_EN.md`

- **Pose Augmentation**: High-frequency pose output using IMU data integration
  - Increases pose update rate from 10-15 Hz to 200+ Hz
  - IMU-Vision mixed mode for smooth, high-frequency pose estimates
  - New demo: `pose_augmentation` - demonstrates high-frequency pose output
  - Tutorial: `tutorials/PoseAugmentation_Tutorial_EN.md`

- **Pose Covariance**: Retrieve pose uncertainty estimates for position and orientation
  - 95% confidence ellipsoid for position uncertainty
  - 1-sigma uncertainty for rotation (roll, pitch, yaw)
  - Both polling and callback APIs supported
  - New demo: `pose_covariance` - demonstrates covariance retrieval and interpretation
  - Tutorial: `tutorials/PoseCovariance_Tutorial_EN.md`

**Documentation:**
- Added comprehensive tutorials for all three new features
- Updated main README with Advanced Features (SDK 2.1) section
- Added detailed READMEs for each new demo