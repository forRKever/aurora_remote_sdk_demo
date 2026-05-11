#include "DepthCamWidget.h"
#include <QPainter>
#include <QStyleOption>

DepthCamWidget::DepthCamWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(0);
    setStyleSheet("background-color: #1a1a1a;");
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void DepthCamWidget::updateDepthFrame(QImage img) {
    depthImage_ = img;
    hasFrame_ = !img.isNull();
    update();
}

void DepthCamWidget::clearFrame() {
    depthImage_ = QImage();
    hasFrame_ = false;
    update();
}

void DepthCamWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(0x0f, 0x0f, 0x1a));

    if (!hasFrame_) {
        painter.setPen(QColor(0x88, 0x88, 0x88));
        painter.setFont(QFont("Arial", 12));
        painter.drawText(rect(), Qt::AlignCenter, "No depth signal");
        return;
    }

    // Scale image to fit widget while preserving aspect ratio
    QSize imgSize = depthImage_.size();
    QSize widgetSize = size();

    if (imgSize.isEmpty()) {
        return;
    }

    double imgRatio = (double)imgSize.width() / imgSize.height();
    double widgetRatio = (double)widgetSize.width() / widgetSize.height();

    QRect destRect;
    if (imgRatio > widgetRatio) {
        // Image is wider than widget
        int newWidth = widgetSize.width();
        int newHeight = (int)(newWidth / imgRatio);
        int y = (widgetSize.height() - newHeight) / 2;
        destRect = QRect(0, y, newWidth, newHeight);
    } else {
        // Image is taller than widget
        int newHeight = widgetSize.height();
        int newWidth = (int)(newHeight * imgRatio);
        int x = (widgetSize.width() - newWidth) / 2;
        destRect = QRect(x, 0, newWidth, newHeight);
    }

    painter.drawImage(destRect, depthImage_);

    // Draw title
    painter.setPen(QColor(0xcc, 0xcc, 0xcc));
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(10, 20, "Depth Camera");
}
