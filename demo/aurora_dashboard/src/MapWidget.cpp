#include "MapWidget.h"
#include <QPainter>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <cmath>
#include <algorithm>
#include <vector>
#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const char* VERTEX_SHADER = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
uniform float uYMin;
uniform float uYRange;
uniform float uUseVertexColor;
out float vHeight;
out float vWorldY;
out vec3 vColor;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    gl_PointSize = 1.5;
    vHeight = (uYRange > 0.0001) ? (aPos.y - uYMin) / uYRange : 0.5;
    vWorldY = aPos.y;
    vColor = aColor;
}
)";

const char* FRAGMENT_SHADER = R"(
#version 330 core
in float vHeight;
in float vWorldY;
in vec3 vColor;
uniform vec3 uColor;
uniform float uUseVertexColor;
uniform float uSliceMode;
uniform float uSliceMinY;
uniform float uSliceMaxY;
out vec4 fragColor;
void main() {
    // Height slice filter: discard points outside the slice band
    if (uSliceMode > 0.5 && uColor.r < 0.0) {
        if (vWorldY < uSliceMinY || vWorldY > uSliceMaxY) discard;
    }
    if (uColor.r >= 0.0) {
        fragColor = vec4(uColor, 1.0);
    } else if (uUseVertexColor > 0.5) {
        // Per-vertex RGB color (colored depth cloud)
        fragColor = vec4(vColor, 0.9);
    } else {
        // Green heatmap style (matching demo map_render)
        float h = clamp(vHeight, 0.0, 1.0);
        float intensity = 0.4 + h * 0.6;
        fragColor = vec4(0.0, intensity, 0.0, 0.8);
    }
}
)";

MapWidget::MapWidget(QWidget* parent)
    : QOpenGLWidget(parent), target_(0, 0, 0) {
    setMinimumSize(400, 400);
    setFocusPolicy(Qt::StrongFocus);
}

MapWidget::~MapWidget() {
    makeCurrent();
    if (shader_) delete shader_;
    if (vaoPoints_) glDeleteVertexArrays(1, &vaoPoints_);
    if (vboPoints_) glDeleteBuffers(1, &vboPoints_);
    if (vaoTrail_) glDeleteVertexArrays(1, &vaoTrail_);
    if (vboTrail_) glDeleteBuffers(1, &vboTrail_);
    if (vaoDepthCloud_) glDeleteVertexArrays(1, &vaoDepthCloud_);
    if (vboDepthCloud_) glDeleteBuffers(1, &vboDepthCloud_);
    doneCurrent();
}

void MapWidget::updateMapData(QVector<QVector3D> keyframes, QVector<QVector3D> mapPoints) {
    makeCurrent();
    keyframes_ = keyframes;
    mapPoints_ = mapPoints;

    // Calculate Y bounds
    if (!mapPoints_.isEmpty()) {
        yMin_ = mapPoints_[0].y();
        yMax_ = mapPoints_[0].y();
        for (const auto& p : mapPoints_) {
            yMin_ = std::min(yMin_, p.y());
            yMax_ = std::max(yMax_, p.y());
        }
        if (yMin_ > yMax_) std::swap(yMin_, yMax_);
        if (yMin_ >= yMax_) yMax_ = yMin_ + 0.1f;
    }

    gpuDirty_ = true;
    update();
    doneCurrent();
}

void MapWidget::updateCurrentPose(double x, double y, double z, double yaw) {
    makeCurrent();
    // Aurora(x,y,z) → OpenGL(x,z,y)：Aurora Z 向上映射到 OpenGL Y 向上
    currentPos_ = QVector3D(x, z, y);
    currentYaw_ = yaw;
    target_ = QVector3D(x, z, y);
    update();
    doneCurrent();
}

void MapWidget::updateDepthCloud(QVector<QVector3D> positions, QVector<QVector3D> colors) {
    makeCurrent();
    depthCloud_       = std::move(positions);
    depthCloudColors_ = std::move(colors);
    depthCloudCount_  = depthCloud_.size();
    gpuDirty_ = true;
    update();
    doneCurrent();
}

void MapWidget::initializeGL() {
    initializeOpenGLFunctions();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);  // Pure black (matching demo)
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Compile shaders
    shader_ = new QOpenGLShaderProgram();
    shader_->addShaderFromSourceCode(QOpenGLShader::Vertex, VERTEX_SHADER);
    shader_->addShaderFromSourceCode(QOpenGLShader::Fragment, FRAGMENT_SHADER);
    if (!shader_->link()) {
        qWarning() << "Shader link error:" << shader_->log();
    }

    // Create VAO/VBO for points
    glGenVertexArrays(1, &vaoPoints_);
    glGenBuffers(1, &vboPoints_);
    glBindVertexArray(vaoPoints_);
    glBindBuffer(GL_ARRAY_BUFFER, vboPoints_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Create VAO/VBO for trajectory
    glGenVertexArrays(1, &vaoTrail_);
    glGenBuffers(1, &vboTrail_);
    glBindVertexArray(vaoTrail_);
    glBindBuffer(GL_ARRAY_BUFFER, vboTrail_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Create VAO/VBO for depth cloud (interleaved: XYZ + RGB, 6 floats/vertex)
    glGenVertexArrays(1, &vaoDepthCloud_);
    glGenBuffers(1, &vboDepthCloud_);
    glBindVertexArray(vaoDepthCloud_);
    glBindBuffer(GL_ARRAY_BUFFER, vboDepthCloud_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void MapWidget::resizeGL(int w, int h) {
    viewW_ = w;
    viewH_ = h;
    glViewport(0, 0, w, h);
}

void MapWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (gpuDirty_) {
        uploadPointsToGPU();
        gpuDirty_ = false;
    }

    // Compute MVP
    QMatrix4x4 projection = projMatrix();
    QMatrix4x4 view = viewMatrix();
    QMatrix4x4 mvp = projection * view;

    shader_->bind();
    shader_->setUniformValue("uMVP", mvp);
    shader_->setUniformValue("uYMin", yMin_);
    shader_->setUniformValue("uYRange", yMax_ - yMin_);
    // Toggle modes
    shader_->setUniformValue("uSliceMode", sliceMode_ ? 1.0f : 0.0f);
    shader_->setUniformValue("uSliceMinY", 0.3f);   // 30cm above floor
    shader_->setUniformValue("uSliceMaxY", 1.8f);   // 180cm (wall features)

    // Render grid
    renderGrid();

    // Render map points
    renderPoints();

    // Render accumulated depth cloud
    renderDepthCloud();

    // Render trajectory
    renderTrajectory();

    // Render current pose
    renderCurrentPose();

    shader_->release();

    // Render text overlay with QPainter
    renderText();
}

void MapWidget::uploadPointsToGPU() {
    // Always update counts first — even when empty — so render functions skip correctly
    pointCount_ = mapPoints_.size();
    trailCount_ = keyframes_.size();
    depthCloudCount_ = depthCloud_.size();

    // Points
    if (!mapPoints_.isEmpty()) {
        glBindBuffer(GL_ARRAY_BUFFER, vboPoints_);
        glBufferData(GL_ARRAY_BUFFER, pointCount_ * sizeof(QVector3D),
                     (const void*)mapPoints_.constData(), GL_DYNAMIC_DRAW);
    }

    // Trail
    if (!keyframes_.isEmpty()) {
        glBindBuffer(GL_ARRAY_BUFFER, vboTrail_);
        glBufferData(GL_ARRAY_BUFFER, trailCount_ * sizeof(QVector3D),
                     (const void*)keyframes_.constData(), GL_DYNAMIC_DRAW);
    }

    // Depth cloud: interleaved XYZ + RGB (6 floats per vertex)
    if (!depthCloud_.isEmpty()) {
        const bool hasColors = (depthCloudColors_.size() == depthCloud_.size());
        std::vector<float> interleaved;
        interleaved.reserve(depthCloud_.size() * 6);
        for (int i = 0; i < depthCloud_.size(); ++i) {
            interleaved.push_back(depthCloud_[i].x());
            interleaved.push_back(depthCloud_[i].y());
            interleaved.push_back(depthCloud_[i].z());
            if (hasColors) {
                interleaved.push_back(depthCloudColors_[i].x());
                interleaved.push_back(depthCloudColors_[i].y());
                interleaved.push_back(depthCloudColors_[i].z());
            } else {
                interleaved.push_back(0.5f);
                interleaved.push_back(0.7f);
                interleaved.push_back(0.9f);  // Fallback: light blue
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, vboDepthCloud_);
        glBufferData(GL_ARRAY_BUFFER, interleaved.size() * sizeof(float),
                     interleaved.data(), GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MapWidget::renderGrid() {
    // Disable point size for grid lines
    shader_->setUniformValue("uYRange", 0.0f);

    const float gridRange = 20.0f;
    const float gridStep = 1.0f;
    const int gridCount = (int)(2 * gridRange / gridStep) + 1;

    std::vector<QVector3D> gridVerts;

    // X-Z grid at Y=0
    for (float i = -gridRange; i <= gridRange; i += gridStep) {
        // Lines parallel to Z
        gridVerts.push_back(QVector3D(i, 0, -gridRange));
        gridVerts.push_back(QVector3D(i, 0, gridRange));

        // Lines parallel to X
        gridVerts.push_back(QVector3D(-gridRange, 0, i));
        gridVerts.push_back(QVector3D(gridRange, 0, i));
    }

    if (!gridVerts.empty()) {
        GLuint gridVao, gridVbo;
        glGenVertexArrays(1, &gridVao);
        glGenBuffers(1, &gridVbo);
        glBindVertexArray(gridVao);
        glBindBuffer(GL_ARRAY_BUFFER, gridVbo);
        glBufferData(GL_ARRAY_BUFFER, gridVerts.size() * sizeof(QVector3D), gridVerts.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        shader_->setUniformValue("uColor", QVector3D(0.3f, 0.3f, 0.4f));
        glDrawArrays(GL_LINES, 0, gridVerts.size());

        glDeleteBuffers(1, &gridVbo);
        glDeleteVertexArrays(1, &gridVao);
    }
}

void MapWidget::renderPoints() {
    if (pointCount_ <= 0) return;

    glBindVertexArray(vaoPoints_);
    shader_->setUniformValue("uColor", QVector3D(-1, -1, -1));  // Use height color
    glDrawArrays(GL_POINTS, 0, pointCount_);
}

void MapWidget::renderDepthCloud() {
    if (depthCloudCount_ <= 0) return;

    glBindVertexArray(vaoDepthCloud_);
    shader_->setUniformValue("uColor", QVector3D(-1, -1, -1));
    shader_->setUniformValue("uUseVertexColor", 1.0f);
    float ptSize = enhancedDepth_ ? 3.0f : 1.5f;
    glPointSize(ptSize);
    glDrawArrays(GL_POINTS, 0, depthCloudCount_);
    glPointSize(1.0f);
    shader_->setUniformValue("uUseVertexColor", 0.0f);
}

void MapWidget::renderTrajectory() {
    if (trailCount_ <= 1) return;

    glBindVertexArray(vaoTrail_);

    // Draw trajectory with gap detection (skip jumps > 5m)
    const float MAX_SEGMENT = 5.0f;
    shader_->setUniformValue("uColor", QVector3D(0.39f, 0.0f, 0.0f));  // Dark red (matching demo)
    for (int i = 0; i + 1 < (int)keyframes_.size(); ++i) {
        float dx = keyframes_[i+1].x() - keyframes_[i].x();
        float dy = keyframes_[i+1].y() - keyframes_[i].y();
        float dz = keyframes_[i+1].z() - keyframes_[i].z();
        if (dx*dx + dy*dy + dz*dz < MAX_SEGMENT * MAX_SEGMENT) {
            glDrawArrays(GL_LINES, i, 2);
        }
    }

    // Draw keyframe markers (small, matching demo style)
    glPointSize(2.0f);
    shader_->setUniformValue("uColor", QVector3D(0.39f, 0.0f, 0.0f));  // Dark red
    glDrawArrays(GL_POINTS, 0, trailCount_);
    glPointSize(1.0f);
}

void MapWidget::renderCurrentPose() {
    // Draw current position as red dot (matching demo style)
    GLuint poseVao, poseVbo;
    glGenVertexArrays(1, &poseVao);
    glGenBuffers(1, &poseVbo);
    glBindVertexArray(poseVao);
    glBindBuffer(GL_ARRAY_BUFFER, poseVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QVector3D), &currentPos_, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glPointSize(6.0f);
    shader_->setUniformValue("uColor", QVector3D(0.78f, 0.0f, 0.0f));  // Red (matching demo)
    glDrawArrays(GL_POINTS, 0, 1);
    glPointSize(1.0f);

    glDeleteBuffers(1, &poseVbo);
    glDeleteVertexArrays(1, &poseVao);
}

void MapWidget::renderText() {
    // Use QPainter to render text overlay (matching demo style)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setPen(QColor(255, 255, 255, 200));
    painter.setFont(QFont("monospace", 10));

    // Match demo: Map Points, Keyframes, Active Map ID
    painter.drawText(10, 30, QString("Map Points: %1").arg(mapPoints_.size()));
    painter.drawText(10, 60, QString("Keyframes: %1").arg(keyframes_.size()));
    painter.drawText(10, 90, QString("Active Map ID: %1").arg(activeMapId_));

    // Axis indicator
    renderAxisIndicator(painter);

    // Minimal help hint at bottom
    painter.setFont(QFont("monospace", 8));
    painter.setPen(QColor(200, 200, 200, 120));
    painter.drawText(5, viewH_ - 5, "T:Top R:Reset F:Front S:Slice H:Help | LMB:Rotate RMB:Pan Wheel:Zoom");

    // Extended help
    if (showHelp_) {
        painter.setFont(QFont("monospace", 8));
        painter.setPen(QColor(150, 255, 150, 200));
        int y = viewH_ - 160;
        painter.drawText(5, y,      "=== Help ===");
        painter.drawText(5, y + 15, "T = Top-down view");
        painter.drawText(5, y + 30, "R = Reset view");
        painter.drawText(5, y + 45, "F = Front view");
        painter.drawText(5, y + 60, QString("S = Height slice 0.3~1.8m (%1)").arg(sliceMode_ ? "ON" : "OFF"));
        painter.drawText(5, y + 75, QString("E = Enhanced depth (%1)").arg(enhancedDepth_ ? "ON" : "OFF"));
        painter.drawText(5, y + 90, "LMB = Rotate | RMB = Pan | Wheel = Zoom");
        painter.drawText(5, y + 105, "Press H to close");
    }
}

void MapWidget::renderAxisIndicator(QPainter& painter) {
    // Draw XYZ axis in bottom-left corner
    int cx = 50, cy = viewH_ - 50;
    int len = 35;

    // Create rotation-only view matrix (no translation)
    float az = azimuth_ * M_PI / 180.0f;
    float el = elevation_ * M_PI / 180.0f;

    // Small projection for the inset
    QMatrix4x4 insetProj;
    insetProj.ortho(-1.5, 1.5, -1.5, 1.5, -10, 10);

    // View rotation only
    QMatrix4x4 viewRot;
    QVector3D eye(cos(el) * sin(az), sin(el), cos(el) * cos(az));
    viewRot.lookAt(eye, QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    QMatrix4x4 mvp = insetProj * viewRot;

    // Project world axes
    auto project = [&](QVector3D dir) -> QPointF {
        QVector4D clip = mvp * QVector4D(dir, 1.0f);
        if (fabs(clip.w()) < 1e-6) return QPointF(cx, cy);
        return QPointF(cx + clip.x() / clip.w() * len, cy - clip.y() / clip.w() * len);
    };

    QPointF o = project(QVector3D(0, 0, 0));
    QPointF px = project(QVector3D(1, 0, 0));
    QPointF py = project(QVector3D(0, 1, 0));
    QPointF pz = project(QVector3D(0, 0, 1));

    // Draw axes with thick lines
    painter.setRenderHint(QPainter::Antialiasing);

    // X axis (red)
    painter.setPen(QPen(QColor(255, 50, 50), 2.5));
    painter.drawLine(o, px);
    painter.setPen(QColor(255, 80, 80));
    painter.setFont(QFont("monospace", 9, QFont::Bold));
    painter.drawText(px + QPointF(5, -8), "X");

    // Y axis (green)
    painter.setPen(QPen(QColor(50, 255, 50), 2.5));
    painter.drawLine(o, py);
    painter.setPen(QColor(80, 255, 80));
    painter.drawText(py + QPointF(5, -8), "Y");

    // Z axis (blue)
    painter.setPen(QPen(QColor(50, 100, 255), 2.5));
    painter.drawLine(o, pz);
    painter.setPen(QColor(100, 150, 255));
    painter.drawText(pz + QPointF(5, -8), "Z");
}

QMatrix4x4 MapWidget::projMatrix() const {
    QMatrix4x4 m;
    float aspect = (float)viewW_ / std::max(1, viewH_);
    m.perspective(45.0f, aspect, 0.1f, 1000.0f);
    return m;
}

QMatrix4x4 MapWidget::viewMatrix() const {
    float az = azimuth_ * M_PI / 180.0f;
    float el = elevation_ * M_PI / 180.0f;

    QVector3D eye(
        target_.x() + distance_ * cos(el) * sin(az),
        target_.y() + distance_ * sin(el),
        target_.z() + distance_ * cos(el) * cos(az)
    );

    QMatrix4x4 m;
    m.lookAt(eye, target_, QVector3D(0, 1, 0));
    return m;
}

void MapWidget::wheelEvent(QWheelEvent* event) {
    if (event->delta() > 0) {
        distance_ *= 0.9f;  // Zoom in
    } else {
        distance_ *= 1.1f;  // Zoom out
    }
    distance_ = std::max(0.5f, std::min(100.0f, distance_));
    update();
}

void MapWidget::mousePressEvent(QMouseEvent* event) {
    lastMousePos_ = event->pos();
    if (event->button() == Qt::LeftButton) {
        rotating_ = true;
    } else if (event->button() == Qt::RightButton) {
        panning_ = true;
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent* event) {
    QPoint delta = event->pos() - lastMousePos_;
    lastMousePos_ = event->pos();

    if (rotating_) {
        azimuth_ += delta.x() * 0.5f;
        elevation_ += delta.y() * 0.5f;
        elevation_ = std::max(-89.0f, std::min(89.0f, elevation_));
    } else if (panning_) {
        float az = azimuth_ * M_PI / 180.0f;
        QVector3D right(-sin(az), 0, cos(az));
        QVector3D forward(cos(az), 0, sin(az));
        target_ += (right * delta.x() - forward * delta.y()) * 0.01f;
    }

    update();
}

void MapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        rotating_ = false;
    } else if (event->button() == Qt::RightButton) {
        panning_ = false;
    }
}

void MapWidget::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        event->ignore();
        return;
    }

    switch (event->key()) {
    case Qt::Key_T:
        // Top-down view (looking straight down at X-Z plane)
        azimuth_ = 0.0f;
        elevation_ = 89.0f;
        distance_ = 10.0f;
        showHelp_ = false;
        update();
        break;

    case Qt::Key_R:
        // Reset to default view
        azimuth_ = 45.0f;
        elevation_ = 30.0f;
        distance_ = 10.0f;
        target_ = currentPos_;
        showHelp_ = false;
        update();
        break;

    case Qt::Key_F:
        // Front view (looking along Z axis)
        azimuth_ = 0.0f;
        elevation_ = 0.0f;
        distance_ = 10.0f;
        showHelp_ = false;
        update();
        break;

    case Qt::Key_H:
        showHelp_ = !showHelp_;
        update();
        break;

    case Qt::Key_S:
        // Toggle height slice mode (show only 0.3~1.8m band to reveal wall outlines)
        sliceMode_ = !sliceMode_;
        update();
        break;

    case Qt::Key_E:
        // Toggle enhanced depth cloud (larger points for easier reading)
        enhancedDepth_ = !enhancedDepth_;
        update();
        break;

    default:
        event->ignore();
        return;
    }

    event->accept();
}

bool MapWidget::exportToPng(const QString& filePath) {
    QImage snapshot = grabFramebuffer();
    return snapshot.save(filePath, "PNG");
}
