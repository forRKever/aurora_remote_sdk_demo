#pragma once

#include <QWidget>
#include <QImage>

class LidarMapWidget : public QWidget {
    Q_OBJECT
public:
    explicit LidarMapWidget(QWidget* parent = nullptr);
    ~LidarMapWidget() = default;

public slots:
    void setOccupancyMap(QImage img, float minX, float minY, float resolution);
    void updateCurrentPose(double x, double y, double z, double yaw);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage gridImage_;
    float minX_ = 0.0f;
    float minY_ = 0.0f;
    float resolution_ = 0.05f;

    double poseX_ = 0.0;
    double poseY_ = 0.0;
    double poseYaw_ = 0.0;
    bool hasPose_ = false;
};
