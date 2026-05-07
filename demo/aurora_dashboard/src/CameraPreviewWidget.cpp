#include "CameraPreviewWidget.h"
#include <QPainter>
#include <QResizeEvent>

CameraPreviewWidget::CameraPreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(80);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background-color: #1a1a1a;");
}

void CameraPreviewWidget::updateFrame(QImage left, QImage right) {
    leftImg_  = left;
    rightImg_ = right;
    hasFrame_ = (!left.isNull() || !right.isNull());
    renderImages();
    update();
}

void CameraPreviewWidget::clearFrame() {
    leftImg_  = QImage();
    rightImg_ = QImage();
    hasFrame_ = false;
    scaledLeft_  = QImage();
    scaledRight_ = QImage();
    update();
}

void CameraPreviewWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    renderImages();
}

void CameraPreviewWidget::renderImages() {
    if (!hasFrame_) return;

    // Each image gets half the widget width minus a small gutter
    int halfW = (width() - 4) / 2;
    int h     = height();

    if (!leftImg_.isNull())
        scaledLeft_  = leftImg_.scaled(halfW, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!rightImg_.isNull())
        scaledRight_ = rightImg_.scaled(halfW, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void CameraPreviewWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(0x1a, 0x1a, 0x1a));

    if (!hasFrame_) {
        p.setPen(Qt::gray);
        p.setFont(QFont("Arial", 10));
        p.drawText(rect(), Qt::AlignCenter, "No camera signal");
        return;
    }

    int halfW = (width() - 4) / 2;

    // Left image — centred in its half
    if (!scaledLeft_.isNull()) {
        int x = (halfW - scaledLeft_.width()) / 2;
        int y = (height() - scaledLeft_.height()) / 2;
        p.drawImage(x, y, scaledLeft_);
        p.setPen(QColor(80, 80, 80));
        p.setFont(QFont("Arial", 8));
        p.drawText(x + 4, y + 14, "L");
    }

    // Right image — centred in right half
    if (!scaledRight_.isNull()) {
        int x = halfW + 4 + (halfW - scaledRight_.width()) / 2;
        int y = (height() - scaledRight_.height()) / 2;
        p.drawImage(x, y, scaledRight_);
        p.setPen(QColor(80, 80, 80));
        p.setFont(QFont("Arial", 8));
        p.drawText(x + 4, y + 14, "R");
    }

    // Dividing line between left and right
    p.setPen(QColor(60, 60, 60));
    p.drawLine(halfW + 2, 0, halfW + 2, height());
}
