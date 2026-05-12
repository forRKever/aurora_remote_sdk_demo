#include "SdkWorker.h"
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QTime>
#include <QCoreApplication>
#include <QStandardPaths>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <limits>

// 全局日誌函數
static void logToFile(const QString& msg) {
    QString logPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/aurora_dashboard.log";
    QFile logFile(logPath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream out(&logFile);
        out << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
            << " | " << msg << "\n";
        logFile.close();
    }
}

// Helper: convert RemoteImageRef to QImage without OpenCV
// Supports format 0 (grayscale) and format 1 (BGR).
static QImage remoteImageRefToQImage(const RemoteImageRef& ref) {
    if (ref.isEmpty()) return QImage();

    const auto& d = ref._desc;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(ref._data);

    if (d.format == 0) {
        // Grayscale: one byte per pixel
        QImage img(d.width, d.height, QImage::Format_Grayscale8);
        for (uint32_t row = 0; row < d.height; ++row) {
            memcpy(img.scanLine(row),
                   data + row * d.stride,
                   d.width);
        }
        return img;
    }
    else if (d.format == 1) {
        // BGR 3-byte per pixel → convert to RGB for Qt
        QImage img(d.width, d.height, QImage::Format_RGB888);
        for (uint32_t row = 0; row < d.height; ++row) {
            const uint8_t* src = data + row * d.stride;
            uint8_t* dst = img.scanLine(row);
            for (uint32_t col = 0; col < d.width; ++col) {
                dst[0] = src[2]; // R ← B[src]
                dst[1] = src[1]; // G
                dst[2] = src[0]; // B ← R[src]
                src += 3; dst += 3;
            }
        }
        return img;
    }
    // Unsupported format
    return QImage();
}

// JET colormap: t in [0,1], maps blue -> cyan -> green -> yellow -> red
static void jetColor(float t, uint8_t& r, uint8_t& g, uint8_t& b) {
    // Clamp t to [0,1]
    t = qBound(0.0f, t, 1.0f);

    // Segmented JET colormap with 4 transitions
    if (t < 0.25f) {
        // blue to cyan: (0,0,1) -> (0,1,1)
        float x = t / 0.25f;  // [0,1] in this segment
        r = 0;
        g = (uint8_t)(255 * x);
        b = 255;
    } else if (t < 0.5f) {
        // cyan to green: (0,1,1) -> (0,1,0)
        float x = (t - 0.25f) / 0.25f;
        r = 0;
        g = 255;
        b = (uint8_t)(255 * (1.0f - x));
    } else if (t < 0.75f) {
        // green to yellow: (0,1,0) -> (1,1,0)
        float x = (t - 0.5f) / 0.25f;
        r = (uint8_t)(255 * x);
        g = 255;
        b = 0;
    } else {
        // yellow to red: (1,1,0) -> (1,0,0)
        float x = (t - 0.75f) / 0.25f;
        r = 255;
        g = (uint8_t)(255 * (1.0f - x));
        b = 0;
    }
}

// Convert float32 depth frame to JET colormap QImage (no OpenCV dependency)
// Auto-detects depth range, handles NaN/inf values gracefully
static QImage depthFrameToColorQImage(const RemoteEnhancedImagingFrame& frame) {
    const auto& img = frame.image;
    if (img.isEmpty()) return QImage();

    const auto& d = img._desc;
    const float* data = reinterpret_cast<const float*>(img._data);
    uint32_t stride_floats = (d.stride > 0) ? (d.stride / sizeof(float)) : d.width;

    // First pass: find min/max valid depth for auto-scaling
    float minDepth = FLT_MAX, maxDepth = -FLT_MAX;
    for (uint32_t row = 0; row < d.height; ++row) {
        const float* src = data + row * stride_floats;
        for (uint32_t col = 0; col < d.width; ++col) {
            float depth = src[col];
            // Skip invalid: NaN, inf, negative, or zero
            if (std::isfinite(depth) && depth > 0.0f) {
                if (depth < minDepth) minDepth = depth;
                if (depth > maxDepth) maxDepth = depth;
            }
        }
    }

    // Fallback range if no valid data found
    if (!std::isfinite(minDepth) || !std::isfinite(maxDepth) || minDepth >= maxDepth) {
        minDepth = 0.0f;
        maxDepth = 5.0f;
    }

    float depthRange = maxDepth - minDepth;
    if (depthRange < 0.01f) depthRange = 0.01f;  // Avoid division by near-zero

    // Second pass: render with auto-scaled colors
    QImage result(d.width, d.height, QImage::Format_RGB888);
    for (uint32_t row = 0; row < d.height; ++row) {
        uint8_t* dst = result.scanLine(row);
        const float* src = data + row * stride_floats;
        for (uint32_t col = 0; col < d.width; ++col) {
            float depth = src[col];

            uint8_t r, g, b;
            if (!std::isfinite(depth) || depth <= 0.0f) {
                // Invalid: black
                r = g = b = 0;
            } else {
                // Normalize to [0, 1] using actual min/max
                float norm = (depth - minDepth) / depthRange;
                norm = qBound(0.0f, norm, 1.0f);
                // Invert: near (low value) = red, far (high value) = blue
                norm = 1.0f - norm;
                jetColor(norm, r, g, b);
            }

            *dst++ = r; *dst++ = g; *dst++ = b;
        }
    }
    return result;
}

SdkWorker::SdkWorker() {
    sdk_ = RemoteSDK::CreateSession();

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &SdkWorker::onPollTimeout);

    mapTimer_ = new QTimer(this);
    connect(mapTimer_, &QTimer::timeout, this, &SdkWorker::onMapRefreshTimeout);

    lidarMapTimer_ = new QTimer(this);
    connect(lidarMapTimer_, &QTimer::timeout, this, &SdkWorker::onLidarMapTimeout);

    lidarScanTimer_ = new QTimer(this);
    connect(lidarScanTimer_, &QTimer::timeout, this, &SdkWorker::onLidarScanTimeout);

    depthTimer_ = new QTimer(this);
    connect(depthTimer_, &QTimer::timeout, this, &SdkWorker::onDepthTimeout);

    segmentationTimer_ = new QTimer(this);
    connect(segmentationTimer_, &QTimer::timeout, this, &SdkWorker::onSegmentationTimeout);

    colmapStatusTimer_ = new QTimer(this);
    connect(colmapStatusTimer_, &QTimer::timeout, this, &SdkWorker::onColmapStatusTimeout);

    // Initialize map generation options with explicit values
    mapGenOptions_.map_canvas_height = 150;
    mapGenOptions_.map_canvas_width = 150;
    mapGenOptions_.resolution = 0.05f;
    // NOTE: active_map_only default is 1 (show only active map)
    // Setting to 0 caused "all maps merged view" to fail with 4x4 gray placeholder

    // Initialize segmentation label info
    segLabelInfo_.label_count = 0;
}

SdkWorker::~SdkWorker() {
    if (sdk_) {
        if (connected_) {
            sdk_->lidar2DMapBuilder.stopPreviewMapBackgroundUpdate();
            sdk_->disconnect();
        }
        sdk_->release();
    }
}

void SdkWorker::connectToDevice(const QString& ip, int port) {
    if (!sdk_) {
        emit connectionChanged(false, "SDK not initialized");
        return;
    }

    if (connected_) {
        sdk_->lidar2DMapBuilder.stopPreviewMapBackgroundUpdate();
        sdk_->disconnect();
        pollTimer_->stop();
        mapTimer_->stop();
        connected_ = false;
    }

    QByteArray ipBytes = ip.toUtf8();
    const char* ipCStr = ipBytes.constData();

    SDKServerConnectionDesc desc(ipCStr);

    if (!sdk_->connect(desc)) {
        emit connectionChanged(false, "Failed to connect");
        return;
    }

    connected_ = true;
    logToFile("✓ Connected to device: " + ip);

    // Enable map data syncing FIRST (before starting map builder)
    logToFile(">>> Enabling map data syncing...");
    sdk_->controller.setMapDataSyncing(true);
    logToFile("✓ Map data syncing enabled");

    // Start occupancy grid map background update
    logToFile(">>> Starting preview map background update...");
    logToFile(QString("    Map options: W=%1 H=%2 Res=%3")
              .arg(mapGenOptions_.map_canvas_width)
              .arg(mapGenOptions_.map_canvas_height)
              .arg(mapGenOptions_.resolution));

    if (!sdk_->lidar2DMapBuilder.startPreviewMapBackgroundUpdate(mapGenOptions_)) {
        logToFile("✗ Failed to start map builder");
        emit connectionChanged(false, "Failed to start map builder");
        sdk_->disconnect();
        connected_ = false;
        return;
    }

    logToFile("✓ Map builder started successfully");

    // Enable auto floor detection
    sdk_->lidar2DMapBuilder.setPreviewMapAutoFloorDetection(true);

    // Use Qt-style delay instead of std::this_thread
    logToFile(">>> Waiting 1 second for device initialization...");
    QTime delayTime = QTime::currentTime().addMSecs(1000);
    while (QTime::currentTime() < delayTime) {
        QCoreApplication::processEvents();
    }
    logToFile("✓ Device initialization complete, starting polling...");

    pollTimer_->start(100);        // 100ms polling for pose/device info
    mapTimer_->start(10000);       // 10s for VSLAM map data
    lidarMapTimer_->start(30000);  // 30s for occupancy grid (generateFullMap is blocking)
    lidarScanTimer_->start(100);   // 100ms for LIDAR scan points overlay

    logToFile("✓ Timers started: poll=100ms, map=10s, lidar=500ms, scan=100ms");

    // Subscribe to depth camera (graceful if unsupported)
    if (sdk_->enhancedImaging.isDepthCameraSupported()) {
        if (sdk_->controller.setEnhancedImagingSubscription(
                SLAMTEC_AURORA_SDK_ENHANCED_IMAGE_TYPE_DEPTH, true)) {
            depthTimer_->start(200);  // ~5fps for depth camera
            logToFile("✓ Depth camera subscription enabled, polling at 200ms");
        } else {
            logToFile("✗ Failed to subscribe to depth camera");
        }
    } else {
        logToFile("ℹ Depth camera not supported by this device");
    }

    // Subscribe to semantic segmentation (graceful if unsupported)
    if (sdk_->enhancedImaging.isSemanticSegmentationSupported()) {
        if (sdk_->controller.setEnhancedImagingSubscription(
                SLAMTEC_AURORA_SDK_ENHANCED_IMAGE_TYPE_SEMANTIC, true)) {
            segmentationTimer_->start(200);  // ~5fps for semantic segmentation
            logToFile("✓ Semantic segmentation subscription enabled, polling at 200ms");

            // Load segmentation labels
            if (sdk_->enhancedImaging.getSemanticSegmentationLabels(segLabelInfo_)) {
                logToFile(QString("✓ Segmentation labels loaded: %1 classes").arg((int)segLabelInfo_.label_count));
            } else {
                segLabelInfo_.label_count = 0;
                logToFile("✗ Failed to load segmentation labels");
            }
        } else {
            logToFile("✗ Failed to subscribe to semantic segmentation");
        }
    } else {
        logToFile("ℹ Semantic segmentation not supported by this device");
    }

    emit connectionChanged(true, "Connected to " + ip);
}

void SdkWorker::disconnectDevice() {
    if (!sdk_ || !connected_) {
        return;
    }

    pollTimer_->stop();
    mapTimer_->stop();
    lidarMapTimer_->stop();
    lidarScanTimer_->stop();
    depthTimer_->stop();
    segmentationTimer_->stop();
    colmapStatusTimer_->stop();

    // Stop COLMAP recording if active
    sdk_->colmapDataRecorder.stopRecording();

    sdk_->lidar2DMapBuilder.stopPreviewMapBackgroundUpdate();
    sdk_->disconnect();
    connected_ = false;

    emit connectionChanged(false, "Disconnected");
}

void SdkWorker::onPollTimeout() {
    if (!sdk_ || !connected_) {
        return;
    }

    // Get pose - C++ wrapper returns bool (true=success, false=fail)
    slamtec_aurora_sdk_pose_t pose;
    if (sdk_->dataProvider.getCurrentPose(pose)) {
        emit poseUpdated(pose.translation.x, pose.translation.y, pose.translation.z,
                         pose.rpy.roll, pose.rpy.pitch, pose.rpy.yaw);
    }

    // Get device info
    RemoteDeviceBasicInfo info;
    uint64_t ts;
    if (sdk_->dataProvider.getLastDeviceBasicInfo(info, ts)) {
        static bool deviceInfoLogged = false;
        if (!deviceInfoLogged) {
            deviceInfoLogged = true;
            logToFile(QString("DeviceInfo SUCCESS | firmware=%1 | serial=%2")
                      .arg(QString::fromUtf8(info.firmware_version_string))
                      .arg(QString::fromStdString(info.getDeviceSerialNumberString())));
        }

        slamtec_aurora_sdk_mapping_flag_t flags;
        sdk_->dataProvider.getMappingFlags(flags);
        bool trackingLost = (flags & SLAMTEC_AURORA_SDK_MAPPING_FLAG_LOSTED) != 0;

        slamtec_aurora_sdk_global_map_desc_t globalDesc;
        sdk_->dataProvider.getGlobalMappingInfo(globalDesc);

        emit deviceInfoUpdated(
            QString::fromUtf8(info.firmware_version_string),
            QString::fromStdString(info.getDeviceSerialNumberString()),
            info.device_uptime_us,
            trackingLost,
            (int)globalDesc.activeMapID,
            (int)globalDesc.totalKFCount,
            (int)globalDesc.totalMPCount
        );
    } else {
        static int deviceInfoFailCount = 0;
        if (++deviceInfoFailCount % 10 == 0) {
            logToFile(QString("DeviceInfo FAIL #%1").arg(deviceInfoFailCount));
        }
    }

    // ── Floor detection info ─────────────────────────────────────────────
    std::vector<slamtec_aurora_sdk_floor_detection_desc_t> floors;
    int curFloor = -1;
    if (sdk_->floorDetector.getAllDetectionDesc(floors, curFloor)) {
        int total = (int)floors.size();
        float h = 0.0f, conf = 0.0f;
        for (const auto& f : floors) {
            if (f.floorID == curFloor) {
                h = f.typical_height;
                conf = f.confidence;
                break;
            }
        }
        if (curFloor != lastFloorID_ || total != lastTotalFloors_) {
            lastFloorID_ = curFloor;
            lastTotalFloors_ = total;
            emit floorInfoUpdated(curFloor, total, h, conf);
        }
    }

    // ── Pose quality indicator ───────────────────────────────────────────
    PoseCovariance cov;
    uint64_t covTs = 0;
    if (sdk_->dataProvider.getRecentPoseCovariance(cov, &covTs)) {
        PoseCovarianceReadable readable;
        if (cov.toHumanReadable(readable)) {
            float r95 = readable.getPositionRadius95XY();
            QString q = r95 < 0.05f ? "EXCELLENT"
                      : r95 < 0.10f ? "GOOD"
                      : r95 < 0.30f ? "FAIR" : "POOR";
            if (q != lastQuality_) {
                lastQuality_ = q;
                emit poseQualityUpdated(q, r95);
            }
        }
    }

    // ── Camera frame preview ─────────────────────────────────────────────
    // peekTrackingData returns the most recent tracking frame buffered by
    // the SDK. Returns false when no new frame is available.
    RemoteTrackingFrameInfo trackingFrame;
    if (sdk_->dataProvider.peekTrackingData(trackingFrame)) {
        QImage leftImg  = remoteImageRefToQImage(trackingFrame.leftImage);
        QImage rightImg = remoteImageRefToQImage(trackingFrame.rightImage);
        // Emit even if one side is null — the widget handles partial data.
        emit cameraFrameUpdated(leftImg, rightImg);
    }
}

void SdkWorker::onMapRefreshTimeout() {
    if (!sdk_ || !connected_) {
        return;
    }

    // Update VSLAM map data (keyframes and map points)
    slamtec_aurora_sdk_global_map_desc_t globalDesc;
    if (!sdk_->dataProvider.getGlobalMappingInfo(globalDesc)) {
        logToFile("WARNING: getGlobalMappingInfo failed");
        return;
    }

    QVector<QVector3D> keyframes;
    QVector<QVector3D> mapPoints;

    RemoteMapDataVisitor visitor;

    visitor.subscribeKeyFrameData([&](const RemoteKeyFrameData& kf) {
        keyframes.append(QVector3D(kf.desc.pose.translation.x, kf.desc.pose.translation.y, kf.desc.pose.translation.z));
    });

    visitor.subscribeMapPointData([&](const slamtec_aurora_sdk_map_point_desc_t& mp) {
        mapPoints.append(QVector3D(mp.position.x, mp.position.y, mp.position.z));
    });

    sdk_->dataProvider.accessMapData(visitor, {(uint32_t)globalDesc.activeMapID});

    logToFile(QString("VSLAM: KF=%1 MP=%2").arg(keyframes.size()).arg(mapPoints.size()));

    // Log signal emission
    if (keyframes.isEmpty() && mapPoints.isEmpty()) {
        logToFile("  WARNING: Both KF and MP are empty, still emitting signal");
    }

    emit mapDataUpdated(keyframes, mapPoints);
}

void SdkWorker::onLidarMapTimeout() {
    if (!sdk_ || !connected_) return;
    if (generating_) return;

    generating_ = true;

    LIDAR2DGridMapGenerationOptions options = mapGenOptions_;
    logToFile(">>> generateFullMap start...");
    auto fullMap = sdk_->lidar2DMapBuilder.generateFullMap(options, true, 5000);

    if (!fullMap) {
        logToFile("✗ generateFullMap failed or timeout");
        generating_ = false;
        return;
    }

    slamtec_aurora_sdk_2dmap_dimension_t mapDim;
    fullMap->getMapDimension(mapDim);
    logToFile(QString("generateFullMap dim: x=[%1,%2] y=[%3,%4]")
              .arg(mapDim.min_x, 0, 'f', 2).arg(mapDim.max_x, 0, 'f', 2)
              .arg(mapDim.min_y, 0, 'f', 2).arg(mapDim.max_y, 0, 'f', 2));

    float w = mapDim.max_x - mapDim.min_x;
    float h = mapDim.max_y - mapDim.min_y;
    if (w < options.resolution || h < options.resolution) {
        logToFile("✗ Map too small");
        generating_ = false;
        return;
    }

    slamtec_aurora_sdk_rect_t fetchRect;
    fetchRect.x = mapDim.min_x;
    fetchRect.y = mapDim.min_y;
    fetchRect.width  = w;
    fetchRect.height = h;

    std::vector<uint8_t> mapData;
    slamtec_aurora_sdk_2d_gridmap_fetch_info_t fetchInfo;
    if (!fullMap->readCellData(fetchRect, fetchInfo, mapData)) {
        logToFile("✗ readCellData failed");
        generating_ = false;
        return;
    }

    uint32_t cw = fetchInfo.cell_width;
    uint32_t ch = fetchInfo.cell_height;
    if (cw == 0 || ch == 0 || mapData.empty() || mapData.size() != (size_t)cw * ch) {
        logToFile(QString("✗ Invalid cell data: %1x%2").arg(cw).arg(ch));
        generating_ = false;
        return;
    }

    int countBlack = 0, countWhite = 0, countGray = 0;
    for (uint8_t p : mapData) {
        if (p < 64) countBlack++;
        else if (p > 192) countWhite++;
        else countGray++;
    }
    logToFile(QString("✓ Full map: %1x%2 px | Black:%3 White:%4 Gray:%5")
              .arg(cw).arg(ch).arg(countBlack).arg(countWhite).arg(countGray));

    QImage gridImage(cw, ch, QImage::Format_Grayscale8);
    std::memcpy(gridImage.bits(), mapData.data(), mapData.size());

    emit occupancyMapUpdated(gridImage, fetchRect.x, fetchRect.y, options.resolution);
    generating_ = false;
}

void SdkWorker::onLidarScanTimeout() {
    if (!sdk_ || !connected_) {
        return;
    }

    // Fetch recent LIDAR scan single layer with pose
    SingleLayerLIDARScan scan;
    slamtec_aurora_sdk_pose_se3_t poseSE3;

    if (!sdk_->dataProvider.peekRecentLIDARScanSingleLayer(scan, poseSE3)) {
        return;
    }

    if (scan.info.scan_count == 0) {
        return;
    }

    // Extract quaternion components from pose
    double qx = poseSE3.quaternion.x;
    double qy = poseSE3.quaternion.y;
    double qz = poseSE3.quaternion.z;
    double qw = poseSE3.quaternion.w;

    // Convert scan points to world coordinates
    QVector<QPointF> worldPoints;
    worldPoints.reserve(scan.info.scan_count);

    for (size_t i = 0; i < scan.info.scan_count; ++i) {
        const auto& point = scan.scanData[i];
        double dist = point.dist;
        double angle = point.angle;

        // Skip invalid points (quality should be > 0 for valid points)
        if (dist <= 0 || point.quality <= 0 || !std::isfinite(dist) || !std::isfinite(angle)) {
            continue;
        }

        // Transform from polar to Cartesian in local frame
        double localX = dist * std::cos(angle);
        double localY = dist * std::sin(angle);
        double localZ = 0.0;  // Single-layer LIDAR

        // Apply quaternion rotation: p' = q * p * q^-1
        // For efficiency, only compute the rotated X,Y components
        double tx = 2.0 * (-qz * localY);
        double ty = 2.0 * (qz * localX);
        double tz = 2.0 * (qx * localY - qy * localX);

        // Transform to world frame
        double worldX = poseSE3.translation.x + localX + qw * tx + (qy * tz - qz * ty);
        double worldY = poseSE3.translation.y + localY + qw * ty + (qz * tx - qx * tz);

        worldPoints.append(QPointF(worldX, worldY));
    }

    if (!worldPoints.empty()) {
        emit lidarScanUpdated(worldPoints);
    }
}

void SdkWorker::requestMapRefresh() {
    mapRefreshRequested_ = true;
    // 同時立刻觸發一次 2D 地圖更新
    if (connected_ && !generating_) {
        lidarMapTimer_->stop();
        lidarMapTimer_->start(1000);  // 1 秒後立即觸發一次
    }
    onMapRefreshTimeout();
}

void SdkWorker::startMapping() {
    if (!sdk_ || !connected_) {
        logToFile("ERROR: Not connected, cannot start mapping");
        emit mappingStatusChanged("Not connected");
        return;
    }

    logToFile(">>> Attempting to enter MAPPING mode...");
    if (sdk_->controller.requireMappingMode(5000)) {
        logToFile("✓ Successfully entered MAPPING mode");
        emit mappingStatusChanged("Mapping mode activated");
    } else {
        logToFile("✗ FAILED to enter mapping mode!");
        emit mappingStatusChanged("Failed to activate mapping mode");
    }
}

void SdkWorker::stopMapping() {
    if (!sdk_ || !connected_) {
        emit mappingStatusChanged("Not connected");
        return;
    }

    logToFile(">>> Attempting to enter LOCALIZATION mode...");
    if (sdk_->controller.requirePureLocalizationMode(5000)) {
        logToFile("✓ Successfully entered LOCALIZATION mode");
        emit mappingStatusChanged("Localization mode activated");
    } else {
        logToFile("✗ FAILED to enter localization mode!");
        emit mappingStatusChanged("Failed to activate localization mode");
    }
}

void SdkWorker::resetMap() {
    if (!sdk_ || !connected_) {
        emit mappingStatusChanged("Not connected");
        return;
    }

    if (sdk_->controller.requireMapReset(5000)) {
        emit mappingStatusChanged("Map reset");
    } else {
        emit mappingStatusChanged("Failed to reset map");
    }
}

void SdkWorker::startColmapRecording(QString folder) {
    if (!sdk_ || !connected_) {
        emit colmapRecordingStatus(false, 0, "Not connected");
        return;
    }

    std::string folderStd = folder.toStdString();

    // Configure COLMAP options
    sdk_->colmapDataRecorder.setOptionString("image_quality", "preview");
    sdk_->colmapDataRecorder.setOptionBool("undistort", true);
    sdk_->colmapDataRecorder.setOptionBool("stereo_recording", false);
    sdk_->colmapDataRecorder.setOptionBool("undistort_force_focal_center", true);

    // Start recording
    if (sdk_->colmapDataRecorder.startRecording(folderStd.c_str())) {
        logToFile(QString("✓ COLMAP recording started: %1").arg(folder));
        colmapStatusTimer_->start(2000);  // Poll every 2 seconds
        emit colmapRecordingStatus(true, 0, "Recording...");
    } else {
        logToFile("✗ Failed to start COLMAP recording");
        emit colmapRecordingStatus(false, 0, "Failed to start");
    }
}

void SdkWorker::stopColmapRecording() {
    if (!sdk_ || !connected_) {
        emit colmapRecordingStatus(false, 0, "Not connected");
        return;
    }

    sdk_->colmapDataRecorder.stopRecording();
    colmapStatusTimer_->stop();
    logToFile("✓ COLMAP recording stopped");
    emit colmapRecordingStatus(false, 0, "Recording stopped");
}

void SdkWorker::onColmapStatusTimeout() {
    if (!sdk_ || !connected_) {
        colmapStatusTimer_->stop();
        return;
    }

    int64_t kfCount = 0;
    if (sdk_->colmapDataRecorder.queryStatusInt64("kf_count", &kfCount)) {
        emit colmapRecordingStatus(true, (int)kfCount, QString("KF: %1").arg(kfCount));
    }
}

void SdkWorker::downloadMap(const QString& savePath) {
    if (!sdk_ || !connected_) {
        emit mapTransferFinished(false, "Not connected");
        return;
    }

    std::string pathStd = savePath.toStdString();

    std::promise<bool> done;
    auto future = done.get_future();

    auto callback = [](void* ud, int ok) {
        auto* p = reinterpret_cast<std::promise<bool>*>(ud);
        p->set_value(ok != 0);
    };

    emit mapTransferProgress(0.f);

    if (!sdk_->mapManager.startDownloadSession(pathStd.c_str(), callback, &done)) {
        emit mapTransferFinished(false, "Failed to start download");
        return;
    }

    // Poll progress in background thread
    std::thread([this]() {
        if (!sdk_) return;

        while (sdk_->mapManager.isSessionActive()) {
            slamtec_aurora_sdk_mapstorage_session_status_t status;
            sdk_->mapManager.querySessionStatus(status);
            emit mapTransferProgress(status.progress);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }).detach();

    // Wait for completion
    std::thread([this, future = std::move(future)]() mutable {
        bool ok = future.get();
        emit mapTransferFinished(ok, ok ? "Download complete" : "Download failed");
    }).detach();
}

void SdkWorker::uploadMap(const QString& filePath) {
    if (!sdk_ || !connected_) {
        emit mapTransferFinished(false, "Not connected");
        return;
    }

    std::string pathStd = filePath.toStdString();

    std::promise<bool> done;
    auto future = done.get_future();

    auto callback = [](void* ud, int ok) {
        auto* p = reinterpret_cast<std::promise<bool>*>(ud);
        p->set_value(ok != 0);
    };

    emit mapTransferProgress(0.f);

    if (!sdk_->mapManager.startUploadSession(pathStd.c_str(), callback, &done)) {
        emit mapTransferFinished(false, "Failed to start upload");
        return;
    }

    // Poll progress in background thread
    std::thread([this]() {
        if (!sdk_) return;

        while (sdk_->mapManager.isSessionActive()) {
            slamtec_aurora_sdk_mapstorage_session_status_t status;
            sdk_->mapManager.querySessionStatus(status);
            emit mapTransferProgress(status.progress);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }).detach();

    // Wait for completion
    std::thread([this, future = std::move(future)]() mutable {
        bool ok = future.get();
        emit mapTransferFinished(ok, ok ? "Upload complete" : "Upload failed");
    }).detach();
}

void SdkWorker::onDepthTimeout() {
    if (!sdk_ || !connected_) {
        return;
    }

    RemoteEnhancedImagingFrame depthFrame;
    slamtec_aurora_sdk_errorcode_t errorCode;
    if (!sdk_->enhancedImaging.peekDepthCameraFrame(
            depthFrame, SLAMTEC_AURORA_SDK_DEPTHCAM_FRAME_TYPE_DEPTH_MAP, &errorCode)) {
        return;  // NOT_READY is normal — skip silently
    }

    QImage img = depthFrameToColorQImage(depthFrame);
    if (!img.isNull()) {
        emit depthFrameUpdated(img);
    }
}

// Colorize segmentation map: each class gets a distinct color
static QImage segmentationFrameToColorQImage(const RemoteEnhancedImagingFrame& frame) {
    const auto& img = frame.image;
    if (img.isEmpty()) return QImage();

    const auto& d = img._desc;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(img._data);

    // Generate deterministic colors for each class (based on demo's approach)
    std::vector<uint32_t> classColors(256);
    classColors[0] = 0x000000;  // Background: black (transparent)

    // Generate colors for classes 1-255
    for (int i = 1; i < 256; i++) {
        // Use deterministic pseudo-random colors
        uint32_t r = ((i * 73) % 256);
        if (r < 50) r += 80;  // Avoid too dark colors
        uint32_t g = ((i * 131) % 256);
        if (g < 50) g += 80;
        uint32_t b = ((i * 173) % 256);
        if (b < 50) b += 80;
        classColors[i] = (r << 16) | (g << 8) | b;
    }

    // Convert to RGB image
    QImage result(d.width, d.height, QImage::Format_RGB888);
    for (uint32_t row = 0; row < d.height; ++row) {
        uint8_t* dst = result.scanLine(row);
        const uint8_t* src = data + row * d.stride;
        for (uint32_t col = 0; col < d.width; ++col) {
            uint8_t classId = src[col];
            uint32_t color = classColors[classId];
            dst[0] = (color >> 16) & 0xFF;  // R
            dst[1] = (color >> 8) & 0xFF;   // G
            dst[2] = color & 0xFF;           // B
            dst += 3;
        }
    }

    return result;
}

void SdkWorker::onSegmentationTimeout() {
    if (!sdk_ || !connected_) {
        return;
    }

    RemoteEnhancedImagingFrame segFrame;
    slamtec_aurora_sdk_errorcode_t errorCode;
    if (!sdk_->enhancedImaging.peekSemanticSegmentationFrame(segFrame, &errorCode)) {
        return;  // NOT_READY is normal — skip silently
    }

    // Compute dominant label (pure C++, no OpenCV)
    QString dominantLabel;
    if (segLabelInfo_.label_count > 0 && !segFrame.image.isEmpty()) {
        const auto& desc = segFrame.image._desc;
        const uint8_t* rawData = reinterpret_cast<const uint8_t*>(segFrame.image._data);
        int counts[256] = {};
        for (uint32_t r = 0; r < desc.height; ++r) {
            const uint8_t* row = rawData + r * desc.stride;
            for (uint32_t c = 0; c < desc.width; ++c) counts[row[c]]++;
        }
        counts[0] = 0;  // exclude background
        int domClass = (int)(std::max_element(counts, counts + 256) - counts);
        if (domClass > 0 && domClass < (int)segLabelInfo_.label_count) {
            std::string n = segLabelInfo_.label_names[domClass].name;
            if (n != "(null)") dominantLabel = QString::fromStdString(n);
        }
    }

    QImage img = segmentationFrameToColorQImage(segFrame);
    if (!img.isNull()) {
        emit semanticSegmentationFrameUpdated(img, dominantLabel);
    }
}
