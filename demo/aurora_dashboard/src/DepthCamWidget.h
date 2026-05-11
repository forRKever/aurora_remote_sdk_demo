#pragma once

#include <QWidget>
#include <QImage>

class DepthCamWidget : public QWidget {
    Q_OBJECT

public:
    explicit DepthCamWidget(QWidget* parent = nullptr);
    ~DepthCamWidget() = default;

public slots:
    void updateDepthFrame(QImage img);
    void clearFrame();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage depthImage_;
    bool hasFrame_ = false;
};
