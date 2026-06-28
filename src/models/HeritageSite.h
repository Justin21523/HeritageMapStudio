#pragma once

#include <QColor>
#include <QString>

// HeritageSite 是文化地點的核心資料模型。
// 重點：model 只描述資料，不處理 UI，也不直接操作 database。
struct HeritageSite {
    int id = -1;

    QString title;
    QString category;
    QString description;

    double latitude = 0.0;
    double longitude = 0.0;

    // Phase 2 先使用 mapX/mapY 當 OpenGL 平面座標。
    // Phase 4 會再把 latitude/longitude 做正式 GIS 投影轉換。
    double mapX = 0.0;
    double mapY = 0.0;

    QString address;
    QString city;
    QString district;

    int startYear = 0;
    int endYear = 0;
    QString periodLabel;

    QString sourceName;
    QString sourceUrl;
    QString preservationStatus;
    QString tags;

    QString createdAt;
    QString updatedAt;

    // 顯示用：優先使用 periodLabel，沒有才用年份組合。
    QString displayPeriod() const;

    // 顯示用：把 city / district / address 組合成簡短地點字串。
    QString displayLocation() const;
};

// 依文化類型給 OpenGL 點位顏色。
// 放在 model 附近，是因為 category → color 是 domain display rule，不屬於 MainWindow。
QColor colorForHeritageCategory(const QString &category);