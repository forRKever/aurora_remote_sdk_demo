#pragma once

#include <QWidget>
#include <QElapsedTimer>
#include "NavigationGuidance.h"

class NavigationWidget : public QWidget {
    Q_OBJECT

public:
    explicit NavigationWidget(QWidget* parent = nullptr);

public slots:
    void updateGuidance(NavigationGuidance guidance);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    NavigationGuidance guidance_;
    bool hasData_ = false;

    // Command debouncing: require stable for 2 frames before switching
    NavCommand stableCommand_ = NAV_GO_STRAIGHT;
    NavCommand pendingCommand_ = NAV_GO_STRAIGHT;
    int pendingCount_ = 0;

    QColor zoneColor(NavZone zone, int alpha = 150) const;
    QString sectorName(int sector) const;
};
