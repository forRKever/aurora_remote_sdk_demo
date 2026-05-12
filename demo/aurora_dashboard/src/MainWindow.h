#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QProgressBar>
#include <QFileDialog>
#include <QSplitter>
#include "SdkWorker.h"
#include "MapWidget.h"
#include "CameraPreviewWidget.h"
#include "DepthCamWidget.h"
#include "SemanticSegmentationWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onRefreshMapClicked();
    void onDownloadMapClicked();
    void onUploadMapClicked();
    void onExportMapPngClicked();
    void onStartMappingClicked();
    void onStopMappingClicked();
    void onResetMapClicked();

    void updatePoseUI(double x, double y, double z, double roll, double pitch, double yaw);
    void updateDeviceUI(QString firmware, QString serial, quint64 uptime_us, bool trackingLost,
                        int activeMapId, int kfCount, int mpCount);
    void updateConnectionUI(bool connected, QString message);
    void updateMapProgress(float progress);
    void onMapTransferDone(bool success, QString message);
    void onMappingStatusChanged(QString status);

private:
    void setupUI();
    void setupWorker();
    QString formatUptime(quint64 uptime_us);

    // Left panel widgets
    QLineEdit* ipEdit_;
    QSpinBox* portSpin_;
    QPushButton* connectBtn_;
    QPushButton* disconnectBtn_;
    QLabel* statusLabel_;

    QLabel* poseXLabel_;
    QLabel* poseYLabel_;
    QLabel* poseZLabel_;
    QLabel* poseRollLabel_;
    QLabel* posePitchLabel_;
    QLabel* poseYawLabel_;

    QLabel* slamStateLabel_;
    QLabel* firmwareLabel_;
    QLabel* serialLabel_;
    QLabel* uptimeLabel_;
    QLabel* mapIdLabel_;
    QLabel* kfLabel_;
    QLabel* mpLabel_;

    // Mapping control widgets
    QPushButton* startMappingBtn_;
    QPushButton* stopMappingBtn_;
    QPushButton* resetMapBtn_;
    QLabel* mappingStatusLabel_;

    // Right panel widgets
    MapWidget* mapWidget_;
    CameraPreviewWidget* cameraWidget_;
    DepthCamWidget* depthWidget_;
    SemanticSegmentationWidget* segmentationWidget_;
    QSplitter* centerSplitter_;   // horizontal splitter
    QSplitter* leftSubSplitter_;  // vertical splitter for depth + seg
    QSplitter* rightSubSplitter_; // vertical splitter for VSLAM + camera + mapOps
    QPushButton* refreshMapBtn_;
    QPushButton* downloadMapBtn_;
    QPushButton* uploadMapBtn_;
    QPushButton* exportMapPngBtn_;
    QLineEdit* uploadFileEdit_;
    QProgressBar* progressBar_;
    QLabel* opStatusLabel_;

    // Worker
    SdkWorker* worker_;
    QThread* workerThread_;
    QString lastUploadPath_;
};
