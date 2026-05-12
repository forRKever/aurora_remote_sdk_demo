#pragma once

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QPointF>
#include <QVector3D>
#include <QString>
#include <QImage>
#include "aurora_pubsdk_inc.h"

using namespace rp::standalone::aurora;

class SdkWorker : public QObject {
    Q_OBJECT

public:
    SdkWorker();
    ~SdkWorker();

public slots:
    void connectToDevice(const QString& ip, int port);
    void disconnectDevice();
    void requestMapRefresh();
    void downloadMap(const QString& savePath);
    void uploadMap(const QString& filePath);
    void startMapping();
    void stopMapping();
    void resetMap();

signals:
    void poseUpdated(double x, double y, double z,
                     double roll, double pitch, double yaw);
    void deviceInfoUpdated(QString firmware, QString serial,
                           quint64 uptime_us, bool trackingLost,
                           int activeMapId, int kfCount, int mpCount);
    void mapDataUpdated(QVector<QVector3D> keyframes,
                        QVector<QVector3D> mapPoints);
    void occupancyMapUpdated(QImage gridImage, float minX, float minY, float resolution);
    void lidarScanUpdated(QVector<QPointF> worldPoints);
    void cameraFrameUpdated(QImage left, QImage right);
    void depthFrameUpdated(QImage img);
    void semanticSegmentationFrameUpdated(QImage img, QString dominantLabel);
    void mappingStatusChanged(QString status);
    void connectionChanged(bool connected, QString message);
    void mapTransferProgress(float progress);
    void mapTransferFinished(bool success, QString message);

private slots:
    void onPollTimeout();
    void onMapRefreshTimeout();
    void onLidarMapTimeout();
    void onLidarScanTimeout();
    void onDepthTimeout();
    void onSegmentationTimeout();

private:
    RemoteSDK* sdk_ = nullptr;
    QTimer* pollTimer_ = nullptr;
    QTimer* mapTimer_ = nullptr;
    QTimer* lidarMapTimer_ = nullptr;   // 500ms polling for occupancy grid
    QTimer* lidarScanTimer_ = nullptr;  // 100ms polling for LIDAR scan points
    QTimer* depthTimer_ = nullptr;      // 200ms polling for depth camera
    QTimer* segmentationTimer_ = nullptr;  // 200ms polling for semantic segmentation
    bool connected_ = false;
    bool mapRefreshRequested_ = false;
    LIDAR2DGridMapGenerationOptions mapGenOptions_;
    slamtec_aurora_sdk_semantic_segmentation_label_info_t segLabelInfo_;
};
