#pragma once

#include <QWidget>
#include <QImage>
#include <QString>

class SemanticSegmentationWidget : public QWidget {
    Q_OBJECT

public:
    explicit SemanticSegmentationWidget(QWidget* parent = nullptr);
    ~SemanticSegmentationWidget() = default;

public slots:
    void updateSegmentationFrame(QImage img, QString label);
    void clearFrame();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage segmentationImage_;
    QString dominantLabel_;
    bool hasFrame_ = false;
};
