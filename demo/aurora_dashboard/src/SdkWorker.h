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
    void startColmapRecording(QString folder);
    void stopColmapRecording();

signals:
    void poseUpdated(double x, double y, double z,
                     double roll, double pitch, double yaw);
    void deviceInfoUpdated(QString firmware, QString serial,
                           quint64 uptime_us, bool trackingLost,
                           int activeMapId, int kfCount, int mpCount);
    void mapDataUpdated(QVector<QVector3D> keyframes,
                        QVector<QVector3D> mapPoints);
    void floorInfoUpdated(int currentFloorID, int totalFloors, float currentHeight, float confidence);
    void poseQualityUpdated(QString quality, float radius95);
    void colmapRecordingStatus(bool isRecording, int kfCount, QString message);
    void cameraFrameUpdated(QImage left, QImage right);
    void depthFrameUpdated(QImage img);
    void semanticSegmentationFrameUpdated(QImage img, QString dominantLabel);
    void mappingStatusChanged(QString status);
    void connectionChanged(bool connected, QString message);
    void mapTransferProgress(float progress);
    void mapTransferFinished(bool success, QString message);
    void depthCloudUpdated(QVector<QVector3D> positions, QVector<QVector3D> colors);

public slots:
    void clearDepthCloud();

private slots:
    void onPollTimeout();
    void onMapRefreshTimeout();
    void onDepthTimeout();
    void onSegmentationTimeout();
    void onColmapStatusTimeout();

private:
    RemoteSDK* sdk_ = nullptr;
    QTimer* pollTimer_ = nullptr;
    QTimer* mapTimer_ = nullptr;
    QTimer* depthTimer_ = nullptr;      // 200ms polling for depth camera
    QTimer* segmentationTimer_ = nullptr;  // 200ms polling for semantic segmentation
    QTimer* colmapStatusTimer_ = nullptr;  // 2s polling for COLMAP recording status
    bool connected_ = false;
    bool mapRefreshRequested_ = false;
    slamtec_aurora_sdk_semantic_segmentation_label_info_t segLabelInfo_;
    int lastFloorID_ = -999;
    int lastTotalFloors_ = -1;
    QString lastQuality_;

    // Latest VSLAM map points (for 3D-to-2D reprojection on camera image)
    QVector<QVector3D> latestMapPoints_;

    // Depth cloud accumulation
    QVector<QVector3D> depthCloudAccum_;
    QVector<QVector3D> depthCloudColorAccum_;  // RGB colors for accumulated depth cloud
    double lastPoseX_ = 0;
    double lastPoseY_ = 0;
    double lastPoseZ_ = 0;
    double lastPoseRoll_ = 0;
    double lastPosePitch_ = 0;
    double lastPoseYaw_ = 0;

    // Last pose when depth cloud was appended (for movement threshold)
    // Initialized to extreme values to ensure first frame is always appended
    double lastDepthX_ = 1e9;
    double lastDepthY_ = 1e9;
    double lastDepthZ_ = 1e9;
    double lastDepthYaw_ = 1e9;

    // Camera calibration (for 3D-to-2D projection on camera image)
    bool calibrationFetched_ = false;
    float cameraFx_ = 0.0f, cameraFy_ = 0.0f;  // Focal length
    float cameraCx_ = 0.0f, cameraCy_ = 0.0f;  // Principal point
    int cameraWidth_ = 0, cameraHeight_ = 0;   // Resolution
};
