#include "LidarMapWidget.h"
#include <QPainter>
#include <QResizeEvent>
#include <cmath>

LidarMapWidget::LidarMapWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(400, 400);
    setAttribute(Qt::WA_OpaquePaintEvent);  // Tell Qt we paint all pixels ourselves
}

void LidarMapWidget::setOccupancyMap(QImage img, float minX, float minY, float resolution) {
    gridImage_ = img;
    minX_ = minX;
    minY_ = minY;
    resolution_ = resolution;
    update();
}

void LidarMapWidget::updateCurrentPose(double x, double y, double z, double yaw) {
    (void)z;  // Unused in 2D view
    poseX_ = x;
    poseY_ = y;
    poseYaw_ = yaw;
    hasPose_ = true;
    update();
}

void LidarMapWidget::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Background color
    painter.fillRect(rect(), QColor(0x0f, 0x0f, 0x1a));

    if (gridImage_.isNull()) {
        painter.setPen(QColor(200, 200, 200));
        painter.setFont(QFont("monospace", 12));
        painter.drawText(rect(), Qt::AlignCenter, "Waiting for LIDAR map data...");
        return;
    }

    // Calculate aspect ratio and fit within widget
    int imgW = gridImage_.width();
    int imgH = gridImage_.height();
    int widgetW = width();
    int widgetH = height();

    if (imgW <= 0 || imgH <= 0) return;

    float imgAspect = (float)imgW / imgH;
    float widgetAspect = (float)widgetW / widgetH;

    int displayW, displayH;
    if (imgAspect > widgetAspect) {
        // Image is wider, fit to width
        displayW = widgetW;
        displayH = (int)(widgetW / imgAspect);
    } else {
        // Image is taller, fit to height
        displayH = widgetH;
        displayW = (int)(widgetH * imgAspect);
    }

    int offsetX = (widgetW - displayW) / 2;
    int offsetY = (widgetH - displayH) / 2;

    QRect destRect(offsetX, offsetY, displayW, displayH);

    // Draw occupancy grid map
    painter.drawImage(destRect, gridImage_);

    // Draw current pose if available
    if (hasPose_) {
        // Calculate grid image coordinates
        float imgX = (float)(poseX_ - minX_) / resolution_;
        float imgY = (float)(poseY_ - minY_) / resolution_;

        // Convert to widget coordinates
        float scaleX = (float)displayW / imgW;
        float scaleY = (float)displayH / imgH;

        int screenX = offsetX + (int)(imgX * scaleX);
        int screenY = offsetY + (int)(imgY * scaleY);

        // Draw robot position as yellow circle
        painter.setPen(QPen(QColor(255, 200, 0), 2));
        painter.setBrush(QColor(255, 200, 0, 150));
        painter.drawEllipse(QPoint(screenX, screenY), 8, 8);

        // Draw heading arrow
        int arrowLen = 20;
        int arrowEndX = screenX + (int)(arrowLen * sin(poseYaw_));
        int arrowEndY = screenY - (int)(arrowLen * cos(poseYaw_));

        painter.setPen(QPen(QColor(255, 200, 0), 2));
        painter.drawLine(screenX, screenY, arrowEndX, arrowEndY);

        // Draw small arrowhead
        double angle = poseYaw_;
        int headLen = 6;
        int head1X = arrowEndX - (int)(headLen * sin(angle - 0.4));
        int head1Y = arrowEndY + (int)(headLen * cos(angle - 0.4));
        int head2X = arrowEndX - (int)(headLen * sin(angle + 0.4));
        int head2Y = arrowEndY + (int)(headLen * cos(angle + 0.4));
        painter.drawLine(arrowEndX, arrowEndY, head1X, head1Y);
        painter.drawLine(arrowEndX, arrowEndY, head2X, head2Y);
    }

    // Draw info text overlay
    painter.setPen(QColor(200, 200, 200, 180));
    painter.setFont(QFont("monospace", 9));

    QString info = QString("Map Size: %1x%2 | Resolution: %3 m/cell")
        .arg(imgW).arg(imgH).arg(resolution_, 0, 'f', 3);
    painter.drawText(5, 20, info);

    if (hasPose_) {
        QString poseStr = QString("Robot: (%1, %2) | Yaw: %3°")
            .arg(poseX_, 0, 'f', 2).arg(poseY_, 0, 'f', 2).arg(poseYaw_ * 180.0 / 3.14159, 0, 'f', 1);
        painter.drawText(5, 40, poseStr);
    }

    painter.setPen(QColor(150, 150, 150, 100));
    painter.setFont(QFont("monospace", 8));
    painter.drawText(5, height() - 10, "Min corner: (" + QString::number(minX_, 'f', 2) +
                                       ", " + QString::number(minY_, 'f', 2) + ")");
}
