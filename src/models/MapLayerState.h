#pragma once

#include <QSet>
#include <QString>
#include <QVector>

// HeritageCategoryLayerInfo 描述一種文化地點分類圖層。
// 重點：這只是 layer metadata，不直接操作 UI，也不直接操作 OpenGL。
struct HeritageCategoryLayerInfo {
    QString key;
    QString label;
    QString description;
};

// MapLayerState 是目前地圖圖層的可見狀態。
// LayerPanel 產生它，MainWindow 接收它，MapCanvas 根據它重繪。
struct MapLayerState {
    bool heritageSitesVisible = true;

    // 目前啟用的 heritage category keys。
    // 例如 historic_site、historical_building、historic_district。
    QSet<QString> visibleHeritageCategoryKeys;

    // Heritage sites 整體透明度，範圍 0.0 ~ 1.0。
    float heritageSitesOpacity = 1.0f;

    // 下面幾個是後續 phase 會實作的圖層入口。
    // Phase 6 先建立 state，UI 顯示 coming soon。
    bool historicalEventsVisible = false;
    bool archivePhotosVisible = false;
    bool culturalRoutesVisible = false;
    bool heatmapVisible = false;

    // 建立預設 layer state。
    static MapLayerState createDefault();

    // 判斷某個 category 是否應該顯示。
    bool isHeritageCategoryVisible(const QString &category) const;
};

// 將 HeritageSite.category 正規化成 layer key。
QString heritageCategoryLayerKey(const QString &category);

// 回傳預設會出現在 LayerPanel 裡的 heritage category layers。
QVector<HeritageCategoryLayerInfo> defaultHeritageCategoryLayers();