#include "SdkWorker.h"
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QTime>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QMatrix4x4>
#include <QVector3D>
#include <QSet>
#include <QPainter>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <limits>
#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

    depthTimer_ = new QTimer(this);
    connect(depthTimer_, &QTimer::timeout, this, &SdkWorker::onDepthTimeout);

    segmentationTimer_ = new QTimer(this);
    connect(segmentationTimer_, &QTimer::timeout, this, &SdkWorker::onSegmentationTimeout);

    colmapStatusTimer_ = new QTimer(this);
    connect(colmapStatusTimer_, &QTimer::timeout, this, &SdkWorker::onColmapStatusTimeout);

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

    // Enable map data syncing
    logToFile(">>> Enabling map data syncing...");
    sdk_->controller.setMapDataSyncing(true);
    logToFile("✓ Map data syncing enabled");

    // Use Qt-style delay instead of std::this_thread
    logToFile(">>> Waiting 1 second for device initialization...");
    QTime delayTime = QTime::currentTime().addMSecs(1000);
    while (QTime::currentTime() < delayTime) {
        QCoreApplication::processEvents();
    }
    logToFile("✓ Device initialization complete, starting polling...");

    pollTimer_->start(100);        // 100ms polling for pose/device info
    mapTimer_->start(10000);       // 10s for VSLAM map data

    logToFile("✓ Timers started: poll=100ms, map=10s");

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
    depthTimer_->stop();
    segmentationTimer_->stop();
    colmapStatusTimer_->stop();

    // Stop COLMAP recording if active
    sdk_->colmapDataRecorder.stopRecording();

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
        // Update lastPose* for depth cloud transformation
        lastPoseX_ = pose.translation.x;
        lastPoseY_ = pose.translation.y;
        lastPoseZ_ = pose.translation.z;
        lastPoseRoll_ = pose.rpy.roll;
        lastPosePitch_ = pose.rpy.pitch;
        lastPoseYaw_ = pose.rpy.yaw;

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

        // ── Tracking keypoints overlay ────────────────────────────────────
        // Draw matched (green) and unmatched (red) keypoints on the left image.
        // QPainter on QImage is thread-safe as long as we own the QImage.
        size_t kpCount = trackingFrame.getKeypointsLeftCount();
        const slamtec_aurora_sdk_keypoint_t* kpts = trackingFrame.getKeypointsLeftBuffer();
        if (!leftImg.isNull() && kpCount > 0 && kpts) {
            // Grayscale images need conversion to RGB before colored drawing
            if (leftImg.format() == QImage::Format_Grayscale8)
                leftImg = leftImg.convertToFormat(QImage::Format_RGB888);
            QPainter kp(&leftImg);
            kp.setRenderHint(QPainter::Antialiasing);
            for (size_t i = 0; i < kpCount; ++i) {
                bool matched = (kpts[i].flags != 0);
                kp.setPen(QPen(matched ? QColor(0, 230, 80) : QColor(255, 60, 60), 1));
                kp.setBrush(Qt::NoBrush);
                kp.drawEllipse(QPointF(kpts[i].x, kpts[i].y), 2.5, 2.5);
            }
        }

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
    QVector<QVector3D> allMapPoints;  // All raw points from SDK
    QVector<QVector3D> mapPoints;     // Downsampled points for display

    RemoteMapDataVisitor visitor;

    // Aurora 座標系：Z 向上，X-Y 水平面
    // OpenGL 座標系：Y 向上，X-Z 水平面
    // 映射：Aurora(x, y, z) → OpenGL(x, z, y)
    visitor.subscribeKeyFrameData([&](const RemoteKeyFrameData& kf) {
        keyframes.append(QVector3D(kf.desc.pose.translation.x, kf.desc.pose.translation.z, kf.desc.pose.translation.y));
    });

    visitor.subscribeMapPointData([&](const slamtec_aurora_sdk_map_point_desc_t& mp) {
        allMapPoints.append(QVector3D(mp.position.x, mp.position.z, mp.position.y));
    });

    sdk_->dataProvider.accessMapData(visitor, {(uint32_t)globalDesc.activeMapID});

    // Voxel grid downsampling at 5cm to reduce visual clutter
    {
        const float VOXEL = 0.05f;
        QSet<quint64> seen;
        seen.reserve(allMapPoints.size());
        for (const QVector3D& p : allMapPoints) {
            int ix = (int)std::round(p.x() / VOXEL) + 2000;
            int iy = (int)std::round(p.y() / VOXEL) + 2000;
            int iz = (int)std::round(p.z() / VOXEL) + 2000;
            quint64 key = ((quint64)(ix & 0xFFF))
                        | ((quint64)(iy & 0xFFF) << 12)
                        | ((quint64)(iz & 0xFFF) << 24);
            if (!seen.contains(key)) {
                seen.insert(key);
                mapPoints.append(p);
            }
        }
    }

    logToFile(QString("VSLAM: KF=%1 MP=%2 (downsampled from %3)")
        .arg(keyframes.size()).arg(mapPoints.size()).arg(allMapPoints.size()));

    // Log signal emission
    if (keyframes.isEmpty() && mapPoints.isEmpty()) {
        logToFile("  WARNING: Both KF and MP are empty, still emitting signal");
    }

    emit mapDataUpdated(keyframes, mapPoints);
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

    logToFile(">>> Attempting to reset map...");
    if (sdk_->controller.requireMapReset(5000)) {
        logToFile("✓ Map reset successful");
        emit mappingStatusChanged("Map reset");
        // Clear MapWidget display by emitting empty map data
        emit mapDataUpdated(QVector<QVector3D>(), QVector<QVector3D>());
        // Also clear depth cloud
        depthCloudAccum_.clear();
        depthCloudColorAccum_.clear();
        emit depthCloudUpdated(depthCloudAccum_, depthCloudColorAccum_);
    } else {
        logToFile("✗ FAILED to reset map!");
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
        logToFile("✗ downloadMap: Not connected");
        emit mapTransferFinished(false, "Not connected");
        return;
    }

    std::string pathStd = savePath.toStdString();
    logToFile(QString(">>> Starting map download to: %1").arg(savePath));

    // Use shared_ptr to manage promise lifecycle - prevent broken_promise exception
    auto sharedDone = std::make_shared<std::promise<bool>>();
    auto future = sharedDone->get_future();

    // Lambda that will be called by SDK - captures shared_ptr for lifetime extension
    auto callback = [](void* ud, int ok) {
        auto* p = reinterpret_cast<std::shared_ptr<std::promise<bool>>*>(ud);
        logToFile(QString(">>> Download callback invoked with status: %1").arg(ok));
        if (p) {
            (*p)->set_value(ok != 0);
            delete p;  // Clean up heap-allocated shared_ptr copy
        }
    };

    emit mapTransferProgress(0.f);

    // Heap-allocate a copy of shared_ptr to pass to C-style callback
    auto* cbArg = new std::shared_ptr<std::promise<bool>>(sharedDone);

    logToFile(">>> Calling startDownloadSession...");
    if (!sdk_->mapManager.startDownloadSession(pathStd.c_str(), callback, cbArg)) {
        logToFile("✗ Failed to start download session");
        delete cbArg;  // Clean up if start failed
        emit mapTransferFinished(false, "Failed to start download");
        return;
    }

    logToFile("✓ Download session started successfully");

    // Poll progress in background thread
    std::thread([this]() {
        logToFile(">>> Progress polling thread started");
        try {
            if (!sdk_) {
                logToFile("✗ SDK is null in polling thread");
                return;
            }

            int pollCount = 0;
            while (sdk_ && sdk_->mapManager.isSessionActive()) {
                slamtec_aurora_sdk_mapstorage_session_status_t status;
                sdk_->mapManager.querySessionStatus(status);
                logToFile(QString("  [Poll %1] Progress: %2%").arg(++pollCount).arg(status.progress, 0, 'f', 1));
                emit mapTransferProgress(status.progress);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            logToFile(">>> Progress polling thread ended (session inactive)");
        } catch (const std::exception& e) {
            logToFile(QString("✗ Exception in polling thread: %1").arg(e.what()));
        } catch (...) {
            logToFile("✗ Unknown exception in polling thread");
        }
    }).detach();

    // Wait for completion
    std::thread([this, future = std::move(future)]() mutable {
        try {
            logToFile(">>> Completion wait thread started");
            bool ok = future.get();
            logToFile(QString("✓ Download completed with status: %1").arg(ok ? "SUCCESS" : "FAILED"));
            emit mapTransferFinished(ok, ok ? "Download complete" : "Download failed");
        } catch (const std::exception& e) {
            logToFile(QString("✗ Exception in completion thread: %1").arg(e.what()));
            emit mapTransferFinished(false, QString("Error: %1").arg(e.what()));
        } catch (...) {
            logToFile("✗ Unknown exception in completion thread");
            emit mapTransferFinished(false, "Unknown error");
        }
    }).detach();
}

void SdkWorker::uploadMap(const QString& filePath) {
    if (!sdk_ || !connected_) {
        logToFile("✗ uploadMap: Not connected");
        emit mapTransferFinished(false, "Not connected");
        return;
    }

    std::string pathStd = filePath.toStdString();
    logToFile(QString(">>> Starting map upload from: %1").arg(filePath));

    // Use shared_ptr to manage promise lifecycle - prevent broken_promise exception
    auto sharedDone = std::make_shared<std::promise<bool>>();
    auto future = sharedDone->get_future();

    // Lambda that will be called by SDK - captures shared_ptr for lifetime extension
    auto callback = [](void* ud, int ok) {
        auto* p = reinterpret_cast<std::shared_ptr<std::promise<bool>>*>(ud);
        logToFile(QString(">>> Upload callback invoked with status: %1").arg(ok));
        if (p) {
            (*p)->set_value(ok != 0);
            delete p;  // Clean up heap-allocated shared_ptr copy
        }
    };

    emit mapTransferProgress(0.f);

    // Heap-allocate a copy of shared_ptr to pass to C-style callback
    auto* cbArg = new std::shared_ptr<std::promise<bool>>(sharedDone);

    logToFile(">>> Calling startUploadSession...");
    if (!sdk_->mapManager.startUploadSession(pathStd.c_str(), callback, cbArg)) {
        logToFile("✗ Failed to start upload session");
        delete cbArg;  // Clean up if start failed
        emit mapTransferFinished(false, "Failed to start upload");
        return;
    }

    logToFile("✓ Upload session started successfully");

    // Poll progress in background thread
    std::thread([this]() {
        logToFile(">>> Progress polling thread started (upload)");
        try {
            if (!sdk_) {
                logToFile("✗ SDK is null in polling thread");
                return;
            }

            int pollCount = 0;
            while (sdk_ && sdk_->mapManager.isSessionActive()) {
                slamtec_aurora_sdk_mapstorage_session_status_t status;
                sdk_->mapManager.querySessionStatus(status);
                logToFile(QString("  [Poll %1] Progress: %2%").arg(++pollCount).arg(status.progress, 0, 'f', 1));
                emit mapTransferProgress(status.progress);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            logToFile(">>> Progress polling thread ended (session inactive)");
        } catch (const std::exception& e) {
            logToFile(QString("✗ Exception in polling thread: %1").arg(e.what()));
        } catch (...) {
            logToFile("✗ Unknown exception in polling thread");
        }
    }).detach();

    // Wait for completion
    std::thread([this, future = std::move(future)]() mutable {
        try {
            logToFile(">>> Completion wait thread started (upload)");
            bool ok = future.get();
            logToFile(QString("✓ Upload completed with status: %1").arg(ok ? "SUCCESS" : "FAILED"));
            emit mapTransferFinished(ok, ok ? "Upload complete" : "Upload failed");
        } catch (const std::exception& e) {
            logToFile(QString("✗ Exception in completion thread: %1").arg(e.what()));
            emit mapTransferFinished(false, QString("Error: %1").arg(e.what()));
        } catch (...) {
            logToFile("✗ Unknown exception in completion thread");
            emit mapTransferFinished(false, "Unknown error");
        }
    }).detach();
}

void SdkWorker::onDepthTimeout() {
    if (!sdk_ || !connected_) {
        return;
    }

    // Fetch depth map for visualization
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

    // ─────────────────────────────────────────────────────────────────────
    // Accumulate depth cloud (POINT3D)
    // ─────────────────────────────────────────────────────────────────────

    RemoteEnhancedImagingFrame pointCloud3D;
    if (!sdk_->enhancedImaging.peekDepthCameraFrame(
            pointCloud3D, SLAMTEC_AURORA_SDK_DEPTHCAM_FRAME_TYPE_POINT3D, &errorCode)) {
        return;  // Camera does not support POINT3D or not ready
    }

    const auto& desc = pointCloud3D.image._desc;
    if (desc.width == 0 || desc.height == 0) {
        return;  // Invalid frame
    }

    const float* point_data = reinterpret_cast<const float*>(pointCloud3D.image._data);
    if (!point_data) {
        return;
    }

    // Log camera resolution once
    static bool camResLogged = false;
    if (!camResLogged) {
        const int step = 16;
        int maxPtsPerFrame = (desc.width / step) * (desc.height / step);
        logToFile(QString("Depth camera resolution: %1 x %2, step=%3, max pts/frame=%4")
            .arg(desc.width).arg(desc.height).arg(step).arg(maxPtsPerFrame));
        camResLogged = true;
    }

    // Movement threshold: only append if moved >15cm OR rotated >20 degrees
    const double MOVE_THRESHOLD = 0.15;  // meters
    const double YAW_THRESHOLD = 20.0 * M_PI / 180.0;  // radians

    double dx = lastPoseX_ - lastDepthX_;
    double dy = lastPoseY_ - lastDepthY_;
    double dz = lastPoseZ_ - lastDepthZ_;
    double dyaw = std::abs(lastPoseYaw_ - lastDepthYaw_);

    // Handle angle wrapping (±π)
    if (dyaw > M_PI) dyaw = 2 * M_PI - dyaw;

    double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (dist < MOVE_THRESHOLD && dyaw < YAW_THRESHOLD) {
        // Not enough movement, skip this frame
        return;
    }

    // Update last depth sampling pose
    lastDepthX_ = lastPoseX_;
    lastDepthY_ = lastPoseY_;
    lastDepthZ_ = lastPoseZ_;
    lastDepthYaw_ = lastPoseYaw_;

    // Fetch paired rectified RGB image for coloring depth points
    const uint8_t* colorData = nullptr;
    int colorFormat = -1;
    uint32_t colorStride = 0;
    uint32_t colorW = 0, colorH = 0;
    {
        RemoteEnhancedImagingFrame rectFrame;
        bool hasColor = sdk_->enhancedImaging.peekDepthCameraRelatedRectifiedImage(
            rectFrame, pointCloud3D.desc.timestamp_ns);
        if (hasColor && rectFrame.image._data) {
            colorData = reinterpret_cast<const uint8_t*>(rectFrame.image._data);
            colorFormat = rectFrame.image._desc.format;
            colorStride = rectFrame.image._desc.stride;
            colorW = rectFrame.image._desc.width;
            colorH = rectFrame.image._desc.height;
        }
    }

    // Build rotation matrix from RPY (Aurora: Rz(yaw) * Ry(pitch) * Rx(roll))
    QMatrix4x4 rot;
    rot.rotate(QQuaternion::fromEulerAngles(
        lastPoseRoll_ * 180.0 / M_PI,    // roll in degrees
        lastPosePitch_ * 180.0 / M_PI,   // pitch in degrees
        lastPoseYaw_ * 180.0 / M_PI      // yaw in degrees
    ));

    // Downsample: sample every 16×16 pixels (reduced from 8×8 for less density)
    // This reduces points per frame from ~4800 to ~1200, keeping cloud cleaner
    const int step = 16;
    const float distance_min = 0.3f;  // meters
    const float distance_max = 6.0f;  // meters

    for (uint32_t row = 0; row < desc.height; row += step) {
        for (uint32_t col = 0; col < desc.width; col += step) {
            uint32_t idx = (row * desc.width + col) * 3;  // 3 floats per pixel

            // Camera frame: X right, Y down, Z forward
            float cx = point_data[idx + 0];
            float cy = point_data[idx + 1];
            float cz = point_data[idx + 2];

            // Check distance validity
            float dist = std::sqrt(cx*cx + cy*cy + cz*cz);
            if (dist < distance_min || dist > distance_max) {
                continue;  // Skip invalid or too far/near
            }

            // Transform from camera frame to Aurora body frame
            // Camera: X=right, Y=down, Z=forward
            // Body:   X=forward, Y=right, Z=up
            // Mapping: body = (camera_z, camera_x, -camera_y)
            QVector3D pt_body(cz, cx, -cy);

            // Apply pose rotation + translation (in Aurora world frame)
            QVector3D pt_world = rot.map(pt_body) + QVector3D(lastPoseX_, lastPoseY_, lastPoseZ_);

            // Convert Aurora frame (X,Y,Z up) to OpenGL frame (X,Z,Y up)
            QVector3D pt_opengl(pt_world.x(), pt_world.z(), pt_world.y());

            // Sample RGB color at (col, row) if available
            QVector3D color(0.5f, 0.7f, 0.9f);  // Default: light blue if no color data
            if (colorData && col < colorW && row < colorH) {
                if (colorFormat == 1) {  // BGR format
                    uint32_t pixIdx = row * colorStride + col * 3;
                    float b = colorData[pixIdx + 0] / 255.0f;
                    float g = colorData[pixIdx + 1] / 255.0f;
                    float r = colorData[pixIdx + 2] / 255.0f;
                    color = QVector3D(r, g, b);
                } else if (colorFormat == 0) {  // Grayscale
                    uint32_t pixIdx = row * colorStride + col;
                    float v = colorData[pixIdx] / 255.0f;
                    color = QVector3D(v, v, v);
                }
            }

            depthCloudAccum_.append(pt_opengl);
            depthCloudColorAccum_.append(color);
        }
    }

    // Limit total points to avoid unbounded growth
    // Reduced from 300000 to 50000 for tighter sliding window (~8 sec history)
    const int max_points = 50000;
    if (depthCloudAccum_.size() > max_points) {
        // Remove oldest 50% of points (changed from 20%) for better sliding-window effect
        int remove_count = max_points / 2;
        depthCloudAccum_.remove(0, remove_count);
        if (depthCloudColorAccum_.size() >= remove_count) {
            depthCloudColorAccum_.remove(0, remove_count);
        }
    }

    // Emit accumulated cloud with colors
    emit depthCloudUpdated(depthCloudAccum_, depthCloudColorAccum_);
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

void SdkWorker::clearDepthCloud() {
    depthCloudAccum_.clear();
    depthCloudColorAccum_.clear();
    emit depthCloudUpdated(depthCloudAccum_, depthCloudColorAccum_);
}
