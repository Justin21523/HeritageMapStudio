#pragma once

#include "models/HeritageSite.h"

#include <QString>
#include <QVector>

// TimelineRangeInfo 描述目前資料集中可用的年份範圍。
// 重點：這是資料分析結果，不直接操作 UI，也不碰 OpenGL。
struct TimelineRangeInfo {
    bool hasDatedSites = false;

    int minYear = 0;
    int maxYear = 0;

    int datedSiteCount = 0;
    int undatedSiteCount = 0;

    QString summary() const;

    // 從 HeritageSite 資料集中計算年份範圍。
    static TimelineRangeInfo fromSites(const QVector<HeritageSite> &sites);
};

// TimelineFilterState 是目前時間軸篩選狀態。
// TimelinePanel 產生它，MainWindow 接收它，MapCanvas / Search Results 根據它過濾。
struct TimelineFilterState {
    bool enabled = false;

    int startYear = 0;
    int endYear = 0;

    // 沒有 start_year / end_year 的資料是否顯示。
    bool showUndatedSites = true;

    static TimelineFilterState createDisabled();

    static TimelineFilterState createForRange(int startYear,
                                              int endYear,
                                              bool showUndatedSites = true);

    // 判斷某個文化地點是否通過時間篩選。
    bool matchesSite(const HeritageSite &site) const;

    // 判斷一組年份區間是否和目前 filter range 有交集。
    bool matchesYearRange(int siteStartYear, int siteEndYear) const;

    QString displayLabel() const;
};