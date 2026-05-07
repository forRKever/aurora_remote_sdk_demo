#pragma once

#include <QWidget>
#include <QImage>

/**
 * Displays a left+right stereo camera frame pair side by side.
 * Images are scaled to fit the available width while preserving aspect ratio.
 * Shows a placeholder ("No camera signal") when no frame has arrived.
 */
class CameraPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit CameraPreviewWidget(QWidget* parent = nullptr);

public slots:
    /** Called from MainWindow when SdkWorker emits cameraFrameUpdated */
    void updateFrame(QImage left, QImage right);
    /** Called when connection is lost — reverts to placeholder */
    void clearFrame();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void renderImages();

    QImage leftImg_;
    QImage rightImg_;
    bool hasFrame_ = false;

    // Cached scaled versions rebuilt on resize or new frame
    QImage scaledLeft_;
    QImage scaledRight_;
};
