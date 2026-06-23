#pragma once

#include <QString>
#include <QMetaType>
#include <array>
#include <cfloat>

// 6 sectors, 60 degrees each, covering full 360
enum NavSector {
    SECTOR_FRONT = 0,        // -30 to +30 degrees
    SECTOR_FRONT_LEFT = 1,   // +30 to +90
    SECTOR_LEFT = 2,         // +90 to +150
    SECTOR_REAR = 3,         // +150 to -150
    SECTOR_RIGHT = 4,        // -150 to -90
    SECTOR_FRONT_RIGHT = 5,  // -90 to -30
    SECTOR_COUNT = 6
};

enum NavCommand {
    NAV_GO_STRAIGHT,
    NAV_TURN_LEFT,
    NAV_TURN_RIGHT,
    NAV_SLOW_DOWN,
    NAV_STOP
};

enum NavZone {
    ZONE_SAFE,       // > 1.5m
    ZONE_WARNING,    // 0.5m - 1.5m
    ZONE_DANGER      // < 0.5m
};

static constexpr float NAV_DANGER_THRESHOLD  = 0.5f;   // meters
static constexpr float NAV_WARNING_THRESHOLD = 1.5f;   // meters
static constexpr float NAV_RADAR_MAX_RANGE   = 5.0f;   // meters (display range)
static constexpr float NAV_MIN_SCAN_DIST     = 0.25f;  // ignore points closer than this (user's body)

struct NavigationGuidance {
    std::array<float, SECTOR_COUNT> sectorMinDist;       // min distance per sector
    std::array<NavZone, SECTOR_COUNT> sectorZone;        // zone per sector
    std::array<int, SECTOR_COUNT> sectorObstacleCount;   // point count per sector

    NavCommand command;
    float recommendedTurnDeg;  // positive = left, negative = right
    float frontDistance;       // closest obstacle in front sector
    NavZone overallZone;       // worst across forward-facing sectors
    QString commandText;
    bool valid;

    NavigationGuidance() {
        sectorMinDist.fill(FLT_MAX);
        sectorZone.fill(ZONE_SAFE);
        sectorObstacleCount.fill(0);
        command = NAV_GO_STRAIGHT;
        recommendedTurnDeg = 0;
        frontDistance = FLT_MAX;
        overallZone = ZONE_SAFE;
        commandText = "NO DATA";
        valid = false;
    }
};

Q_DECLARE_METATYPE(NavigationGuidance)
