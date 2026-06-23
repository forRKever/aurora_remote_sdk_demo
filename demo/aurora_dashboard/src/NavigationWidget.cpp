#include "NavigationWidget.h"
#include <QPainter>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

NavigationWidget::NavigationWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumSize(300, 250);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void NavigationWidget::updateGuidance(NavigationGuidance guidance) {
    // Debounce: require 2 consecutive same commands before switching
    if (guidance.command != pendingCommand_) {
        pendingCommand_ = guidance.command;
        pendingCount_ = 1;
    } else {
        pendingCount_++;
    }

    if (pendingCount_ >= 2) {
        stableCommand_ = pendingCommand_;
    }

    // Use stable command for display
    guidance.command = stableCommand_;
    // Regenerate command text for stable command
    switch (stableCommand_) {
        case NAV_GO_STRAIGHT: guidance.commandText = "GO STRAIGHT"; break;
        case NAV_TURN_LEFT:   guidance.commandText = "TURN LEFT"; break;
        case NAV_TURN_RIGHT:  guidance.commandText = "TURN RIGHT"; break;
        case NAV_SLOW_DOWN:   guidance.commandText = "SLOW DOWN"; break;
        case NAV_STOP:        guidance.commandText = "STOP"; break;
    }

    guidance_ = guidance;
    hasData_ = true;
    update();
}

QColor NavigationWidget::zoneColor(NavZone zone, int alpha) const {
    switch (zone) {
        case ZONE_SAFE:    return QColor(0, 200, 0, alpha);
        case ZONE_WARNING: return QColor(255, 200, 0, alpha);
        case ZONE_DANGER:  return QColor(255, 40, 40, alpha);
    }
    return QColor(128, 128, 128, alpha);
}

QString NavigationWidget::sectorName(int sector) const {
    switch (sector) {
        case SECTOR_FRONT:       return "FRONT";
        case SECTOR_FRONT_LEFT:  return "F-LEFT";
        case SECTOR_LEFT:        return "LEFT";
        case SECTOR_REAR:        return "REAR";
        case SECTOR_RIGHT:       return "RIGHT";
        case SECTOR_FRONT_RIGHT: return "F-RIGHT";
    }
    return "?";
}

void NavigationWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int W = width();
    int H = height();

    // Background
    p.fillRect(rect(), QColor(0x0f, 0x0f, 0x1a));

    if (!hasData_ || !guidance_.valid) {
        p.setPen(QColor(200, 200, 200));
        p.setFont(QFont("monospace", 12));
        QString msg = hasData_ ? "TRACKING LOST - NO GUIDANCE" : "Waiting for LIDAR data...";
        QColor col = hasData_ ? QColor(255, 80, 80) : QColor(200, 200, 200);
        p.setPen(col);
        p.drawText(rect(), Qt::AlignCenter, msg);
        return;
    }

    // Layout: radar on left, info on right, command banner at bottom
    int bannerH = 60;
    int radarArea = H - bannerH;
    int radarSize = qMin(W * 2 / 3, radarArea) - 20;
    if (radarSize < 80) radarSize = 80;
    int radarR = radarSize / 2;
    int radarCX = radarR + 10;
    int radarCY = radarArea / 2;

    // --- Draw range rings ---
    p.setPen(QPen(QColor(60, 60, 80), 1, Qt::DashLine));
    float ranges[] = {0.5f, 1.5f, 3.0f};
    for (float range : ranges) {
        int r = (int)(radarR * range / NAV_RADAR_MAX_RANGE);
        if (r > 0 && r <= radarR) {
            p.drawEllipse(QPoint(radarCX, radarCY), r, r);
            // Label
            p.setFont(QFont("monospace", 7));
            p.setPen(QColor(100, 100, 120));
            p.drawText(radarCX + r + 2, radarCY - 2, QString("%1m").arg(range, 0, 'f', 1));
            p.setPen(QPen(QColor(60, 60, 80), 1, Qt::DashLine));
        }
    }
    // Outer ring
    p.setPen(QPen(QColor(80, 80, 100), 1));
    p.drawEllipse(QPoint(radarCX, radarCY), radarR, radarR);

    // --- Draw sectors as filled pie slices ---
    // Sector angles (in Qt's coordinate system: 0=3 o'clock, positive=CCW)
    // We want FRONT = top of screen = 90 degrees in Qt coords
    // LIDAR convention: 0=forward, positive=CCW (left)
    // Qt drawPie: angle in 1/16 degree, 0=3 o'clock, positive=CCW

    // Sector start angles in LIDAR frame (degrees)
    struct SectorDef { int sector; float startDeg; float spanDeg; };
    SectorDef sectors[] = {
        { SECTOR_FRONT,       -30, 60 },
        { SECTOR_FRONT_LEFT,   30, 60 },
        { SECTOR_LEFT,         90, 60 },
        { SECTOR_REAR,        150, 60 },  // 150 to 210 (wraps)
        { SECTOR_RIGHT,      -150, 60 },  // -150 to -90
        { SECTOR_FRONT_RIGHT, -90, 60 },
    };

    for (auto& sd : sectors) {
        NavZone zone = guidance_.sectorZone[sd.sector];
        QColor fillColor = zoneColor(zone, 80);

        // Convert LIDAR angle to Qt drawPie angle
        // LIDAR: 0=forward(+X), positive=CCW
        // Screen: we want forward=up, so rotate by 90
        // Qt drawPie: 0=right, positive=CCW, in 1/16 degree units
        float qtStartDeg = 90.0f + sd.startDeg;  // shift so forward=up
        int qtStart16 = (int)(qtStartDeg * 16);
        int qtSpan16 = (int)(sd.spanDeg * 16);

        QRect pieRect(radarCX - radarR, radarCY - radarR, radarR * 2, radarR * 2);
        p.setPen(QPen(QColor(80, 80, 100, 100), 1));
        p.setBrush(fillColor);
        p.drawPie(pieRect, qtStart16, qtSpan16);
    }

    // --- Draw obstacle dots on radar ---
    // Use sector min distances to show obstacle arc on each sector
    p.setPen(Qt::NoPen);
    for (auto& sd : sectors) {
        float dist = guidance_.sectorMinDist[sd.sector];
        if (dist >= NAV_RADAR_MAX_RANGE || dist <= 0) continue;

        int r = (int)(radarR * dist / NAV_RADAR_MAX_RANGE);
        float midAngleLidar = sd.startDeg + sd.spanDeg / 2.0f;
        // Convert to screen: forward=up => screen angle = -(lidar angle) + 90 in math convention
        // screen X = cx + r * cos(screen_angle), Y = cy - r * sin(screen_angle)
        float screenAngleRad = (90.0f + midAngleLidar) * (float)M_PI / 180.0f;
        int ox = radarCX + (int)(r * cos(screenAngleRad));
        int oy = radarCY - (int)(r * sin(screenAngleRad));

        p.setBrush(QColor(255, 255, 255, 200));
        p.drawEllipse(QPoint(ox, oy), 4, 4);
    }

    // --- Draw robot icon at center ---
    p.setPen(QPen(QColor(100, 180, 255), 2));
    p.setBrush(QColor(100, 180, 255, 180));
    p.drawEllipse(QPoint(radarCX, radarCY), 6, 6);

    // --- Draw recommended direction arrow ---
    {
        float arrowAngleLidar = guidance_.recommendedTurnDeg;  // degrees from forward
        float screenAngleRad = (90.0f + arrowAngleLidar) * (float)M_PI / 180.0f;

        // Arrow length proportional to front distance (min 30%, max 90% of radar)
        float distRatio = qBound(0.3f, guidance_.frontDistance / NAV_RADAR_MAX_RANGE, 0.9f);
        int arrowLen = (int)(radarR * distRatio);

        int endX = radarCX + (int)(arrowLen * cos(screenAngleRad));
        int endY = radarCY - (int)(arrowLen * sin(screenAngleRad));

        QColor arrowColor = zoneColor(guidance_.overallZone, 255);
        if (guidance_.overallZone == ZONE_SAFE)
            arrowColor = QColor(0, 255, 100);

        p.setPen(QPen(arrowColor, 4));
        p.drawLine(radarCX, radarCY, endX, endY);

        // Arrowhead
        float dx = endX - radarCX;
        float dy = endY - radarCY;
        float len = sqrt(dx * dx + dy * dy);
        if (len > 0) {
            dx /= len; dy /= len;
            float px = -dy, py = dx;
            int headLen = 12;
            int h1x = endX - (int)(headLen * (dx * 0.6 + px * 0.4));
            int h1y = endY - (int)(headLen * (dy * 0.6 + py * 0.4));
            int h2x = endX - (int)(headLen * (dx * 0.6 - px * 0.4));
            int h2y = endY - (int)(headLen * (dy * 0.6 - py * 0.4));
            p.drawLine(endX, endY, h1x, h1y);
            p.drawLine(endX, endY, h2x, h2y);
        }
    }

    // --- Draw "FRONT" label at top of radar ---
    p.setPen(QColor(180, 180, 200));
    p.setFont(QFont("monospace", 8));
    p.drawText(radarCX - 18, radarCY - radarR - 5, "FRONT");

    // --- Sector distance readouts (right side) ---
    int infoX = radarCX + radarR + 20;
    int infoY = 20;
    p.setFont(QFont("monospace", 10));

    // Display order: front sectors first, then sides, then rear
    int displayOrder[] = { SECTOR_FRONT, SECTOR_FRONT_LEFT, SECTOR_FRONT_RIGHT,
                           SECTOR_LEFT, SECTOR_RIGHT, SECTOR_REAR };
    for (int s : displayOrder) {
        QColor textCol = zoneColor(guidance_.sectorZone[s], 255);
        p.setPen(textCol);

        QString distStr;
        if (guidance_.sectorMinDist[s] >= NAV_RADAR_MAX_RANGE)
            distStr = "---";
        else
            distStr = QString::number(guidance_.sectorMinDist[s], 'f', 2) + "m";

        QString line = QString("%1: %2").arg(sectorName(s), -8).arg(distStr);
        p.drawText(infoX, infoY, line);
        infoY += 18;
    }

    // --- Command banner at bottom ---
    QRect bannerRect(0, H - bannerH, W, bannerH);
    QColor bannerBg = zoneColor(guidance_.overallZone, 200);
    p.fillRect(bannerRect, bannerBg);

    // Command text
    p.setPen(QColor(255, 255, 255));
    p.setFont(QFont("Arial", 20, QFont::Bold));
    p.drawText(bannerRect.adjusted(10, 0, 0, -18), Qt::AlignVCenter | Qt::AlignLeft,
               guidance_.commandText);

    // Front distance subtitle
    p.setFont(QFont("monospace", 11));
    p.setPen(QColor(255, 255, 255, 200));
    QString frontStr;
    if (guidance_.frontDistance >= NAV_RADAR_MAX_RANGE)
        frontStr = "Front: clear";
    else
        frontStr = QString("Front: %1m").arg(guidance_.frontDistance, 0, 'f', 2);
    p.drawText(bannerRect.adjusted(10, 22, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, frontStr);
}
