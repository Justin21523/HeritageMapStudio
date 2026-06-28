#pragma once

#include "models/HeritageSite.h"

#include <QString>
#include <QStringList>

// SearchFilterState 是目前搜尋與 facet 篩選狀態。
// SearchPanel 產生它，MainWindow 接收它，MapCanvas / Search Results 根據它過濾資料。
struct SearchFilterState {
    QString keyword;

    QString category;
    QString district;
    QString periodLabel;
    QString preservationStatus;
    QString sourceName;
    QString tag;

    // 是否把 description 納入 keyword search。
    bool searchDescription = true;

    static SearchFilterState createDefault();

    bool isDefault() const;

    // 判斷某筆 HeritageSite 是否符合目前搜尋條件。
    bool matchesSite(const HeritageSite &site) const;

    QString displayLabel() const;
};

// tags 在文化資料裡很常用 semicolon 分隔。
// 這個 helper 同時給 SearchFilterState 和 SearchPanel 使用。
QStringList splitHeritageTags(const QString &tags);