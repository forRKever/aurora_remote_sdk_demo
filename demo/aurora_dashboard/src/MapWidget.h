#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QVector3D>
#include <QVector>
#include <QMatrix4x4>
#include <cmath>

class MapWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT

public:
    explicit MapWidget(QWidget* parent = nullptr);
    ~MapWidget();
    bool exportToPng(const QString& filePath);

public slots:
    void updateMapData(QVector<QVector3D> keyframes, QVector<QVector3D> mapPoints);
    void updateCurrentPose(double x, double y, double z, double yaw);
    void updateDepthCloud(QVector<QVector3D> positions, QVector<QVector3D> colors);
    void setOccupancyMap(QImage, float, float, float) {}  // Placeholder

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void uploadPointsToGPU();
    void renderGrid();
    void renderPoints();
    void renderDepthCloud();
    void renderTrajectory();
    void renderCurrentPose();
    void renderText();
    void renderAxisIndicator(QPainter& painter);
    QMatrix4x4 viewMatrix() const;
    QMatrix4x4 projMatrix() const;

    // Camera (Orbit)
    float azimuth_   = 45.0f;     // Horizontal rotation (degrees)
    float elevation_ = 30.0f;     // Vertical tilt (degrees)
    float distance_  = 10.0f;     // Distance from target (m)
    QVector3D target_;             // Camera target point

    // Mouse state
    QPoint lastMousePos_;
    bool rotating_ = false;
    bool panning_  = false;

    // 3D Data
    QVector<QVector3D> mapPoints_;
    QVector<QVector3D> keyframes_;
    QVector<QVector3D> depthCloud_;
    QVector<QVector3D> depthCloudColors_;  // RGB colors for depth cloud (per-vertex)
    QVector3D currentPos_;
    double currentYaw_ = 0;

    // OpenGL
    QOpenGLShaderProgram* shader_ = nullptr;
    GLuint vaoPoints_ = 0, vboPoints_ = 0;
    GLuint vaoTrail_  = 0, vboTrail_  = 0;
    GLuint vaoDepthCloud_ = 0, vboDepthCloud_ = 0;
    int pointCount_ = 0;
    int trailCount_ = 0;
    int depthCloudCount_ = 0;
    bool gpuDirty_ = false;

    // Viewport
    int viewW_ = 1, viewH_ = 1;

    // Bounds for height coloring
    float yMin_ = 0.0f, yMax_ = 0.0f;

    // UI State
    bool showHelp_ = false;
};
