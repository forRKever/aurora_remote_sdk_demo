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

    depthTimer_ = new QTimer(this);
    connect(depthTimer_, &QTimer::timeout, this, &SdkWorker::onDepthTimeout);

    // Initialize map generation options with explicit values
    mapGenOptions_.map_canvas_height = 150;
    mapGenOptions_.map_canvas_width = 150;
    mapGenOptions_.resolution = 0.05f;
    mapGenOptions_.active_map_only = 0;  // Include all maps (active_map_only=1 may filter out data during mapping)
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

    pollTimer_->start(100);      // 100ms polling for pose/device info
    mapTimer_->start(10000);     // 10s for VSLAM map data
    lidarMapTimer_->start(500);  // 500ms for occupancy grid updates

    logToFile("✓ Timers started: poll=100ms, map=10s, lidar=500ms");

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

    emit connectionChanged(true, "Connected to " + ip);
}

void SdkWorker::disconnectDevice() {
    if (!sdk_ || !connected_) {
        return;
    }

    pollTimer_->stop();
    mapTimer_->stop();
    lidarMapTimer_->stop();
    depthTimer_->stop();

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
    if (!sdk_ || !connected_) {
        return;
    }

    // Update occupancy grid map
    if (!sdk_->lidar2DMapBuilder.isPreviewMapBackgroundUpdateActive()) {
        return;
    }

    // Always request redraw to ensure SDK flushes latest LiDAR data
    sdk_->lidar2DMapBuilder.requireRedrawPreviewMap();

    // Check if there is any update
    slamtec_aurora_sdk_rect_t dirtyRect;
    bool mapBigChange = false;
    sdk_->lidar2DMapBuilder.getAndResetPreviewMapDirtyRect(dirtyRect, mapBigChange);
    if (dirtyRect.width <= 0 || dirtyRect.height <= 0) {
        return;
    }

    const OccupancyGridMap2DRef& gridMap = sdk_->lidar2DMapBuilder.getPreviewMap();
    float resolution = gridMap.getResolution();

    // Query the actual populated bounding box instead of fixed canvas
    slamtec_aurora_sdk_2dmap_dimension_t mapDim;
    gridMap.getMapDimension(mapDim);

    // Skip if no meaningful data yet (dimensions too small)
    if ((mapDim.max_x - mapDim.min_x) < resolution || (mapDim.max_y - mapDim.min_y) < resolution) {
        return;
    }

    slamtec_aurora_sdk_rect_t fetchRect;
    fetchRect.x      = mapDim.min_x;
    fetchRect.y      = mapDim.min_y;
    fetchRect.width  = mapDim.max_x - mapDim.min_x;
    fetchRect.height = mapDim.max_y - mapDim.min_y;

    std::vector<uint8_t> mapData;
    slamtec_aurora_sdk_2d_gridmap_fetch_info_t fetchInfo;
    gridMap.readCellData(fetchRect, fetchInfo, mapData);

    uint32_t cell_width = fetchInfo.cell_width;
    uint32_t cell_height = fetchInfo.cell_height;

    if (cell_width == 0 || cell_height == 0 || mapData.empty()) {
        return;
    }

    if (mapData.size() != cell_width * cell_height) {
        return;
    }

    // Analyze map data content
    int countBlack = 0, countWhite = 0, countGray = 0;
    for (uint8_t pixel : mapData) {
        if (pixel < 64) countBlack++;      // 0-63: obstacle
        else if (pixel > 192) countWhite++; // 193-255: free
        else countGray++;                  // 64-192: unknown
    }

    logToFile(QString("LIDAR map emit: %1x%2 px | Black:%3 White:%4 Gray:%5 | BigChange=%6")
              .arg(cell_width).arg(cell_height)
              .arg(countBlack).arg(countWhite).arg(countGray)
              .arg(mapBigChange ? "Y" : "N"));

    QImage gridImage(cell_width, cell_height, QImage::Format_Grayscale8);
    std::memcpy(gridImage.bits(), mapData.data(), mapData.size());

    emit occupancyMapUpdated(gridImage, fetchRect.x, fetchRect.y, resolution);
}

void SdkWorker::requestMapRefresh() {
    mapRefreshRequested_ = true;
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
