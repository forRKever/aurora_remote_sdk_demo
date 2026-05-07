#pragma once

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QPointF>
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
    void mapDataUpdated(QVector<QPointF> keyframes,
                        QVector<QPointF> mapPoints);
    void occupancyMapUpdated(QImage gridImage, float minX, float minY, float resolution);
    void cameraFrameUpdated(QImage left, QImage right);
    void mappingStatusChanged(QString status);
    void connectionChanged(bool connected, QString message);
    void mapTransferProgress(float progress);
    void mapTransferFinished(bool success, QString message);

private slots:
    void onPollTimeout();
    void onMapRefreshTimeout();

private:
    RemoteSDK* sdk_ = nullptr;
    QTimer* pollTimer_ = nullptr;
    QTimer* mapTimer_ = nullptr;
    bool connected_ = false;
    bool mapRefreshRequested_ = false;
    LIDAR2DGridMapGenerationOptions mapGenOptions_;
};
