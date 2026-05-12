#include "MainWindow.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QThread>
#include <QDesktopServices>
#include <QUrl>
#include <QStandardPaths>

MainWindow::MainWindow()
    : ipEdit_(nullptr), portSpin_(nullptr), connectBtn_(nullptr),
      disconnectBtn_(nullptr), statusLabel_(nullptr),
      startMappingBtn_(nullptr), stopMappingBtn_(nullptr), resetMapBtn_(nullptr),
      clearDepthCloudBtn_(nullptr), mappingStatusLabel_(nullptr),
      startColmapBtn_(nullptr), stopColmapBtn_(nullptr), colmapStatusLabel_(nullptr),
      mapWidget_(nullptr), cameraWidget_(nullptr), depthWidget_(nullptr), segmentationWidget_(nullptr),
      centerSplitter_(nullptr), leftSubSplitter_(nullptr), rightSubSplitter_(nullptr),
      worker_(nullptr), workerThread_(nullptr) {
    setWindowTitle("Aurora Dashboard");
    setGeometry(100, 100, 1200, 800);

    setupUI();
    setupWorker();
}

MainWindow::~MainWindow() {
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait(5000);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // 先斷線：停止所有計時器、關閉 SDK 連線
    if (worker_ && workerThread_ && workerThread_->isRunning()) {
        QMetaObject::invokeMethod(worker_, "disconnectDevice",
                                  Qt::BlockingQueuedConnection);
    }
    event->accept();
}

void MainWindow::setupUI() {
    // Central widget
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    // Main layout: left panel + map
    QHBoxLayout* mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // ========== LEFT PANEL ==========
    QWidget* leftPanel = new QWidget;
    QVBoxLayout* leftLayout = new QVBoxLayout(leftPanel);
    leftPanel->setMaximumWidth(280);

    // Connection group
    QGroupBox* connGroup = new QGroupBox("Connection");
    QVBoxLayout* connLayout = new QVBoxLayout(connGroup);

    ipEdit_ = new QLineEdit;
    ipEdit_->setPlaceholderText("192.168.11.1");
    ipEdit_->setText("192.168.11.1");

    portSpin_ = new QSpinBox;
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(7447);

    QHBoxLayout* ipLayout = new QHBoxLayout;
    ipLayout->addWidget(new QLabel("IP:"));
    ipLayout->addWidget(ipEdit_);

    QHBoxLayout* portLayout = new QHBoxLayout;
    portLayout->addWidget(new QLabel("Port:"));
    portLayout->addWidget(portSpin_);

    connLayout->addLayout(ipLayout);
    connLayout->addLayout(portLayout);

    connectBtn_ = new QPushButton("Connect");
    disconnectBtn_ = new QPushButton("Disconnect");
    disconnectBtn_->setEnabled(false);

    connLayout->addWidget(connectBtn_);
    connLayout->addWidget(disconnectBtn_);

    statusLabel_ = new QLabel("● Disconnected");
    statusLabel_->setStyleSheet("color: red; font-weight: bold;");
    connLayout->addWidget(statusLabel_);

    leftLayout->addWidget(connGroup);

    // Pose group
    QGroupBox* poseGroup = new QGroupBox("Pose");
    QVBoxLayout* poseLayout = new QVBoxLayout(poseGroup);

    poseXLabel_ = new QLabel("X: --");
    poseYLabel_ = new QLabel("Y: --");
    poseZLabel_ = new QLabel("Z: --");
    poseRollLabel_ = new QLabel("Roll: --");
    posePitchLabel_ = new QLabel("Pitch: --");
    poseYawLabel_ = new QLabel("Yaw: --");
    poseQualityLabel_ = new QLabel("Quality: --");
    poseQualityLabel_->setFont(QFont("monospace", 9, QFont::Bold));

    poseLayout->addWidget(poseXLabel_);
    poseLayout->addWidget(poseYLabel_);
    poseLayout->addWidget(poseZLabel_);
    poseLayout->addWidget(poseRollLabel_);
    poseLayout->addWidget(posePitchLabel_);
    poseLayout->addWidget(poseYawLabel_);
    poseLayout->addWidget(poseQualityLabel_);

    leftLayout->addWidget(poseGroup);

    // Device group
    QGroupBox* devGroup = new QGroupBox("Device Status");
    QVBoxLayout* devLayout = new QVBoxLayout(devGroup);

    slamStateLabel_ = new QLabel("SLAM: --");
    slamStateLabel_->setStyleSheet("font-weight: bold;");
    firmwareLabel_ = new QLabel("Firmware: --");
    serialLabel_ = new QLabel("Serial: --");
    uptimeLabel_ = new QLabel("Uptime: --");
    mapIdLabel_ = new QLabel("Map ID: --");
    kfLabel_ = new QLabel("Keyframes: --");
    mpLabel_ = new QLabel("Map Points: --");
    floorLabel_ = new QLabel("Floor: --");

    devLayout->addWidget(slamStateLabel_);
    devLayout->addWidget(firmwareLabel_);
    devLayout->addWidget(serialLabel_);
    devLayout->addWidget(uptimeLabel_);
    devLayout->addWidget(mapIdLabel_);
    devLayout->addWidget(kfLabel_);
    devLayout->addWidget(mpLabel_);
    devLayout->addWidget(floorLabel_);

    leftLayout->addWidget(devGroup);

    // Mapping control group
    QGroupBox* mapCtrlGroup = new QGroupBox("Mapping Control");
    QVBoxLayout* mapCtrlLayout = new QVBoxLayout(mapCtrlGroup);

    QHBoxLayout* mappingBtnLayout = new QHBoxLayout;
    startMappingBtn_ = new QPushButton("Start Mapping");
    stopMappingBtn_ = new QPushButton("Stop Mapping");
    stopMappingBtn_->setEnabled(false);
    mappingBtnLayout->addWidget(startMappingBtn_);
    mappingBtnLayout->addWidget(stopMappingBtn_);
    mapCtrlLayout->addLayout(mappingBtnLayout);

    resetMapBtn_ = new QPushButton("Reset Map");
    mapCtrlLayout->addWidget(resetMapBtn_);

    clearDepthCloudBtn_ = new QPushButton("Clear Depth Cloud");
    mapCtrlLayout->addWidget(clearDepthCloudBtn_);

    mappingStatusLabel_ = new QLabel("Status: Idle");
    mappingStatusLabel_->setWordWrap(true);
    mapCtrlLayout->addWidget(mappingStatusLabel_);

    leftLayout->addWidget(mapCtrlGroup);

    // COLMAP 3D Recording Control group
    QGroupBox* colmapGroup = new QGroupBox("3D Recording (COLMAP)");
    QVBoxLayout* colmapLayout = new QVBoxLayout(colmapGroup);

    QHBoxLayout* colmapBtnLayout = new QHBoxLayout;
    startColmapBtn_ = new QPushButton("Start Recording");
    stopColmapBtn_ = new QPushButton("Stop Recording");
    stopColmapBtn_->setEnabled(false);
    colmapBtnLayout->addWidget(startColmapBtn_);
    colmapBtnLayout->addWidget(stopColmapBtn_);
    colmapLayout->addLayout(colmapBtnLayout);

    colmapStatusLabel_ = new QLabel("Status: Idle");
    colmapStatusLabel_->setWordWrap(true);
    colmapLayout->addWidget(colmapStatusLabel_);

    leftLayout->addWidget(colmapGroup);
    leftLayout->addStretch();

    mainLayout->addWidget(leftPanel);

    // ========== RIGHT PANEL ==========
    QWidget* rightPanel = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    // Outer horizontal splitter: [leftSubSplitter_ | rightSubSplitter_]
    centerSplitter_ = new QSplitter(Qt::Horizontal);
    centerSplitter_->setChildrenCollapsible(false);

    // Left sub-pane: vertical splitter with [depthWidget_, segmentationWidget_]
    leftSubSplitter_ = new QSplitter(Qt::Vertical);
    leftSubSplitter_->setChildrenCollapsible(true);

    depthWidget_ = new DepthCamWidget;
    depthWidget_->setMinimumHeight(0);
    leftSubSplitter_->addWidget(depthWidget_);

    segmentationWidget_ = new SemanticSegmentationWidget;
    segmentationWidget_->setMinimumHeight(0);
    leftSubSplitter_->addWidget(segmentationWidget_);

    leftSubSplitter_->setSizes({400, 400});

    // Right sub-pane: vertical splitter with [mapWidget_, cameraWidget_, mapOpsGroup]
    rightSubSplitter_ = new QSplitter(Qt::Vertical);
    rightSubSplitter_->setChildrenCollapsible(true);

    // Create 3D map widget
    mapWidget_ = new MapWidget;
    mapWidget_->setMinimumHeight(200);
    rightSubSplitter_->addWidget(mapWidget_);

    cameraWidget_ = new CameraPreviewWidget;
    cameraWidget_->setMinimumHeight(0);
    rightSubSplitter_->addWidget(cameraWidget_);

    // Map operations group
    QGroupBox* mapOpsGroup = new QGroupBox("Map Operations");
    QVBoxLayout* mapOpsLayout = new QVBoxLayout(mapOpsGroup);

    QHBoxLayout* refreshLayout = new QHBoxLayout;
    refreshMapBtn_ = new QPushButton("Refresh Map");
    refreshLayout->addWidget(refreshMapBtn_);
    refreshLayout->addStretch();
    mapOpsLayout->addLayout(refreshLayout);

    QHBoxLayout* downloadLayout = new QHBoxLayout;
    downloadMapBtn_ = new QPushButton("Download Map");
    downloadLayout->addWidget(downloadMapBtn_);
    downloadLayout->addStretch();
    mapOpsLayout->addLayout(downloadLayout);

    QHBoxLayout* uploadLayout = new QHBoxLayout;
    uploadLayout->addWidget(new QLabel("Upload:"));
    uploadFileEdit_ = new QLineEdit;
    uploadFileEdit_->setReadOnly(true);
    uploadFileEdit_->setPlaceholderText("Select .stcm file");
    uploadMapBtn_ = new QPushButton("Browse...");
    uploadMapBtn_->setMaximumWidth(80);
    uploadLayout->addWidget(uploadFileEdit_);
    uploadLayout->addWidget(uploadMapBtn_);
    mapOpsLayout->addLayout(uploadLayout);

    QHBoxLayout* exportLayout = new QHBoxLayout;
    exportMapPngBtn_ = new QPushButton("Export Map as PNG");
    exportLayout->addWidget(exportMapPngBtn_);
    exportLayout->addStretch();
    mapOpsLayout->addLayout(exportLayout);

    progressBar_ = new QProgressBar;
    progressBar_->setVisible(false);
    progressBar_->setMaximum(100);
    mapOpsLayout->addWidget(progressBar_);

    opStatusLabel_ = new QLabel;
    opStatusLabel_->setWordWrap(true);
    mapOpsLayout->addWidget(opStatusLabel_);

    rightSubSplitter_->addWidget(mapOpsGroup);
    rightSubSplitter_->setSizes({450, 220, 130});

    // Compose the horizontal splitter
    centerSplitter_->addWidget(leftSubSplitter_);
    centerSplitter_->addWidget(rightSubSplitter_);
    centerSplitter_->setSizes({380, 620});

    rightLayout->addWidget(centerSplitter_);
    mainLayout->addWidget(rightPanel, 1);

    // Connect signals
    connect(connectBtn_, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(disconnectBtn_, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(refreshMapBtn_, &QPushButton::clicked, this, &MainWindow::onRefreshMapClicked);
    connect(downloadMapBtn_, &QPushButton::clicked, this, &MainWindow::onDownloadMapClicked);
    connect(uploadMapBtn_, &QPushButton::clicked, this, &MainWindow::onUploadMapClicked);
    connect(exportMapPngBtn_, &QPushButton::clicked, this, &MainWindow::onExportMapPngClicked);
    connect(startMappingBtn_, &QPushButton::clicked, this, &MainWindow::onStartMappingClicked);
    connect(stopMappingBtn_, &QPushButton::clicked, this, &MainWindow::onStopMappingClicked);
    connect(resetMapBtn_, &QPushButton::clicked, this, &MainWindow::onResetMapClicked);
    connect(clearDepthCloudBtn_, &QPushButton::clicked, this, [this]() {
        QMetaObject::invokeMethod(worker_, "clearDepthCloud", Qt::QueuedConnection);
    });

    // COLMAP recording buttons
    connect(startColmapBtn_, &QPushButton::clicked, this, [this]() {
        QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        QString defaultPath = downloadDir + "/aurora_colmap_dataset";

        QString folderPath = QFileDialog::getExistingDirectory(
            this, "Select COLMAP Output Folder", defaultPath
        );

        if (!folderPath.isEmpty()) {
            colmapStatusLabel_->setText("Starting...");
            startColmapBtn_->setEnabled(false);
            stopColmapBtn_->setEnabled(true);

            QMetaObject::invokeMethod(worker_, "startColmapRecording",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, folderPath));
        }
    });

    connect(stopColmapBtn_, &QPushButton::clicked, this, [this]() {
        startColmapBtn_->setEnabled(true);
        stopColmapBtn_->setEnabled(false);
        colmapStatusLabel_->setText("Stopping...");

        QMetaObject::invokeMethod(worker_, "stopColmapRecording", Qt::QueuedConnection);
    });
}

void MainWindow::setupWorker() {
    workerThread_ = new QThread(this);
    worker_ = new SdkWorker;
    worker_->moveToThread(workerThread_);

    connect(worker_, &SdkWorker::poseUpdated, this, &MainWindow::updatePoseUI, Qt::QueuedConnection);
    connect(worker_, &SdkWorker::deviceInfoUpdated, this, &MainWindow::updateDeviceUI, Qt::QueuedConnection);
    connect(worker_, &SdkWorker::mapDataUpdated, mapWidget_, &MapWidget::updateMapData, Qt::QueuedConnection);
    connect(worker_, &SdkWorker::cameraFrameUpdated, cameraWidget_, &CameraPreviewWidget::updateFrame,
            Qt::QueuedConnection);
    connect(worker_, &SdkWorker::depthFrameUpdated, depthWidget_, &DepthCamWidget::updateDepthFrame,
            Qt::QueuedConnection);
    connect(worker_, &SdkWorker::depthCloudUpdated, mapWidget_, &MapWidget::updateDepthCloud,
            Qt::QueuedConnection);
    connect(worker_, &SdkWorker::semanticSegmentationFrameUpdated, segmentationWidget_, &SemanticSegmentationWidget::updateSegmentationFrame,
            Qt::QueuedConnection);
    connect(worker_, &SdkWorker::floorInfoUpdated, this,
        [this](int fid, int total, float h, float conf) {
            floorLabel_->setText(
                QString("Floor %1/%2 | H:%3m | %4%")
                .arg(fid).arg(total).arg(h, 0, 'f', 2).arg((int)(conf * 100)));
        }, Qt::QueuedConnection);
    connect(worker_, &SdkWorker::poseQualityUpdated, this,
        [this](QString q, float r95) {
            static const QMap<QString, QString> colorMap = {
                {"EXCELLENT", "green"},
                {"GOOD", "#aacc00"},
                {"FAIR", "orange"},
                {"POOR", "red"}
            };
            poseQualityLabel_->setText(
                QString("Quality: %1 (±%2m)").arg(q).arg(r95, 0, 'f', 3));
            poseQualityLabel_->setStyleSheet(
                QString("color:%1; font-weight:bold;").arg(colorMap.value(q, "gray")));
        }, Qt::QueuedConnection);
    connect(worker_, &SdkWorker::colmapRecordingStatus, this,
        [this](bool isRecording, int kfCount, QString message) {
            if (isRecording) {
                colmapStatusLabel_->setText(QString("Recording... | %1").arg(message));
            } else {
                colmapStatusLabel_->setText("Status: " + message);
                startColmapBtn_->setEnabled(true);
                stopColmapBtn_->setEnabled(false);
            }
        }, Qt::QueuedConnection);
    connect(worker_, &SdkWorker::connectionChanged, this, &MainWindow::updateConnectionUI);
    connect(worker_, &SdkWorker::mappingStatusChanged, this, &MainWindow::onMappingStatusChanged);
    connect(worker_, &SdkWorker::mapTransferProgress, this, &MainWindow::updateMapProgress);
    connect(worker_, &SdkWorker::mapTransferFinished, this, &MainWindow::onMapTransferDone);

    workerThread_->start();
}

void MainWindow::onConnectClicked() {
    QString ip = ipEdit_->text().trimmed();
    int port = portSpin_->value();

    if (ip.isEmpty()) {
        statusLabel_->setText("● Error: IP empty");
        statusLabel_->setStyleSheet("color: red; font-weight: bold;");
        return;
    }

    QMetaObject::invokeMethod(worker_, "connectToDevice",
                              Qt::QueuedConnection,
                              Q_ARG(QString, ip),
                              Q_ARG(int, port));
}

void MainWindow::onDisconnectClicked() {
    QMetaObject::invokeMethod(worker_, "disconnectDevice", Qt::QueuedConnection);
}

void MainWindow::onRefreshMapClicked() {
    QMetaObject::invokeMethod(worker_, "requestMapRefresh", Qt::QueuedConnection);
}

void MainWindow::onDownloadMapClicked() {
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString defaultPath = downloadDir + "/aurora_map.stcm";

    QString filePath = QFileDialog::getSaveFileName(
        this, "Save Map File", defaultPath, "STCM Files (*.stcm);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        progressBar_->setVisible(true);
        progressBar_->setValue(0);
        opStatusLabel_->setText("Downloading...");

        QMetaObject::invokeMethod(worker_, "downloadMap",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, filePath));
    }
}

void MainWindow::onUploadMapClicked() {
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    QString filePath = QFileDialog::getOpenFileName(
        this, "Select Map File", downloadDir, "STCM Files (*.stcm);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        uploadFileEdit_->setText(filePath);
        lastUploadPath_ = filePath;

        progressBar_->setVisible(true);
        progressBar_->setValue(0);
        opStatusLabel_->setText("Uploading...");

        QMetaObject::invokeMethod(worker_, "uploadMap",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, filePath));
    }
}

void MainWindow::updatePoseUI(double x, double y, double z, double roll, double pitch, double yaw) {
    poseXLabel_->setText(QString::asprintf("X: %.3f m", x));
    poseYLabel_->setText(QString::asprintf("Y: %.3f m", y));
    poseZLabel_->setText(QString::asprintf("Z: %.3f m", z));
    poseRollLabel_->setText(QString::asprintf("Roll: %.2f°", roll * 180.0 / 3.14159));
    posePitchLabel_->setText(QString::asprintf("Pitch: %.2f°", pitch * 180.0 / 3.14159));
    poseYawLabel_->setText(QString::asprintf("Yaw: %.2f°", yaw * 180.0 / 3.14159));

    mapWidget_->updateCurrentPose(x, y, z, yaw);
}

void MainWindow::updateDeviceUI(QString firmware, QString serial, quint64 uptime_us, bool trackingLost,
                                int activeMapId, int kfCount, int mpCount) {
    QString slamState = trackingLost ? "● TRACKING LOST" : "● TRACKING";
    slamStateLabel_->setText(slamState);
    slamStateLabel_->setStyleSheet(trackingLost ? "color: red; font-weight: bold;"
                                                 : "color: green; font-weight: bold;");

    firmwareLabel_->setText("Firmware: " + firmware);
    serialLabel_->setText("Serial: " + serial);
    uptimeLabel_->setText("Uptime: " + formatUptime(uptime_us));
    mapIdLabel_->setText(QString("Map ID: %1").arg(activeMapId));
    kfLabel_->setText(QString("Keyframes: %1").arg(kfCount));
    mpLabel_->setText(QString("Map Points: %1").arg(mpCount));
}

void MainWindow::updateConnectionUI(bool connected, QString message) {
    if (connected) {
        statusLabel_->setText("● " + message);
        statusLabel_->setStyleSheet("color: green; font-weight: bold;");
        connectBtn_->setEnabled(false);
        disconnectBtn_->setEnabled(true);
        ipEdit_->setReadOnly(true);
        portSpin_->setReadOnly(true);
    } else {
        statusLabel_->setText("● " + message);
        statusLabel_->setStyleSheet("color: red; font-weight: bold;");
        connectBtn_->setEnabled(true);
        disconnectBtn_->setEnabled(false);
        ipEdit_->setReadOnly(false);
        portSpin_->setReadOnly(false);
        cameraWidget_->clearFrame();
        depthWidget_->clearFrame();
    }
}

void MainWindow::updateMapProgress(float progress) {
    progressBar_->setValue(static_cast<int>(progress));
}

void MainWindow::onMapTransferDone(bool success, QString message) {
    progressBar_->setVisible(false);
    opStatusLabel_->setText(message);
    opStatusLabel_->setStyleSheet(success ? "color: green;" : "color: red;");
}

QString MainWindow::formatUptime(quint64 uptime_us) {
    quint64 seconds = uptime_us / 1000000;
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    return QString::asprintf("%dh %dm %ds", hours, minutes, secs);
}

void MainWindow::onStartMappingClicked() {
    startMappingBtn_->setEnabled(false);
    stopMappingBtn_->setEnabled(true);
    mappingStatusLabel_->setText("Status: Starting...");

    QMetaObject::invokeMethod(worker_, "startMapping", Qt::QueuedConnection);
}

void MainWindow::onStopMappingClicked() {
    startMappingBtn_->setEnabled(true);
    stopMappingBtn_->setEnabled(false);
    mappingStatusLabel_->setText("Status: Stopping...");

    QMetaObject::invokeMethod(worker_, "stopMapping", Qt::QueuedConnection);
}

void MainWindow::onResetMapClicked() {
    mappingStatusLabel_->setText("Status: Resetting...");
    QMetaObject::invokeMethod(worker_, "resetMap", Qt::QueuedConnection);
}

void MainWindow::onMappingStatusChanged(QString status) {
    mappingStatusLabel_->setText("Status: " + status);
}

void MainWindow::onExportMapPngClicked() {
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString defaultPath = downloadDir + "/aurora_map.png";

    QString filePath = QFileDialog::getSaveFileName(
        this, "Save Map as PNG", defaultPath, "PNG Images (*.png);;All Files (*)"
    );

    if (!filePath.isEmpty()) {
        bool success = mapWidget_->exportToPng(filePath);
        if (success) {
            opStatusLabel_->setText("Map exported to: " + filePath);
            opStatusLabel_->setStyleSheet("color: green;");
        } else {
            opStatusLabel_->setText("Failed to export map");
            opStatusLabel_->setStyleSheet("color: red;");
        }
    }
}
