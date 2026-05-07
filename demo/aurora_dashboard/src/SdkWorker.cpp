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

SdkWorker::SdkWorker() {
    sdk_ = RemoteSDK::CreateSession();

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &SdkWorker::onPollTimeout);

    mapTimer_ = new QTimer(this);
    connect(mapTimer_, &QTimer::timeout, this, &SdkWorker::onMapRefreshTimeout);

    // Initialize map generation options
    mapGenOptions_.loadDefaults();
    mapGenOptions_.active_map_only = 1;  // Use only active map
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

    // Start occupancy grid map background update FIRST
    logToFile(">>> Starting preview map background update...");
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

    pollTimer_->start(100);      // 100ms polling
    mapTimer_->start(10000);     // 10s map refresh

    logToFile("✓ Timers started: poll=100ms, map=10s");
    emit connectionChanged(true, "Connected to " + ip);
}

void SdkWorker::disconnectDevice() {
    if (!sdk_ || !connected_) {
        return;
    }

    pollTimer_->stop();
    mapTimer_->stop();

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

    // Update VSLAM map data
    slamtec_aurora_sdk_global_map_desc_t globalDesc;
    if (!sdk_->dataProvider.getGlobalMappingInfo(globalDesc)) {
        return;
    }

    QVector<QPointF> keyframes;
    QVector<QPointF> mapPoints;

    RemoteMapDataVisitor visitor;

    visitor.subscribeKeyFrameData([&](const RemoteKeyFrameData& kf) {
        keyframes.append(QPointF(kf.desc.pose.translation.x, kf.desc.pose.translation.z));
    });

    visitor.subscribeMapPointData([&](const slamtec_aurora_sdk_map_point_desc_t& mp) {
        mapPoints.append(QPointF(mp.position.x, mp.position.z));
    });

    sdk_->dataProvider.accessMapData(visitor, {(uint32_t)globalDesc.activeMapID});

    emit mapDataUpdated(keyframes, mapPoints);

    // Update occupancy grid map
    if (!sdk_->lidar2DMapBuilder.isPreviewMapBackgroundUpdateActive()) {
        logToFile("WARNING: Map background update NOT active");
        return;
    }

    slamtec_aurora_sdk_rect_t dirtyRect;
    bool mapBigChange = false;
    sdk_->lidar2DMapBuilder.getAndResetPreviewMapDirtyRect(dirtyRect, mapBigChange);

    logToFile(QString("Dirty rect: W=%1 H=%2 at X=%3 Y=%4 | BigChange=%5")
              .arg(dirtyRect.width).arg(dirtyRect.height)
              .arg(dirtyRect.x).arg(dirtyRect.y)
              .arg(mapBigChange ? "Yes" : "No"));

    // Only update if there's a dirty rectangle with non-zero dimensions
    if (dirtyRect.width <= 0 || dirtyRect.height <= 0) {
        logToFile("  -> No map updates");
        return;
    }

    const OccupancyGridMap2DRef& gridMap = sdk_->lidar2DMapBuilder.getPreviewMap();

    slamtec_aurora_sdk_2dmap_dimension_t mapDim;
    gridMap.getMapDimension(mapDim);
    float resolution = gridMap.getResolution();

    slamtec_aurora_sdk_rect_t fetchRect;
    fetchRect.x = mapDim.min_x;
    fetchRect.y = mapDim.min_y;
    fetchRect.width = mapDim.max_x - mapDim.min_x;
    fetchRect.height = mapDim.max_y - mapDim.min_y;

    std::vector<uint8_t> mapData;
    slamtec_aurora_sdk_2d_gridmap_fetch_info_t fetchInfo;

    gridMap.readCellData(fetchRect, fetchInfo, mapData);

    // Get grid dimensions from fetch info
    uint32_t cell_width = fetchInfo.cell_width;
    uint32_t cell_height = fetchInfo.cell_height;

    if (cell_width == 0 || cell_height == 0 || mapData.empty()) {
        return;
    }

    // Convert to QImage (grayscale 8-bit)
    if (mapData.size() != cell_width * cell_height) {
        qWarning() << "Map data size mismatch:" << mapData.size() << "!=" << (cell_width * cell_height);
        return;
    }

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

    if (sdk_->controller.requirePureLocalizationMode(5000)) {
        emit mappingStatusChanged("Localization mode activated");
    } else {
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
