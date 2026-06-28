#include "models/HeritageSite.h"

#include <QStringList>

QString HeritageSite::displayPeriod() const
{
    // 有人工整理的年代標籤時，優先顯示它。
    if (!periodLabel.trimmed().isEmpty()) {
        return periodLabel;
    }

    // 沒有 periodLabel 時，用 start/end year 組合出可讀文字。
    if (startYear > 0 && endYear > 0) {
        return QString("%1–%2").arg(startYear).arg(endYear);
    }

    if (startYear > 0) {
        return QString::number(startYear);
    }

    return "Unknown";
}

QString HeritageSite::displayLocation() const
{
    QStringList parts;

    // 只加入有值的欄位，避免畫面出現多餘逗號。
    if (!city.trimmed().isEmpty()) {
        parts << city.trimmed();
    }
    if (!district.trimmed().isEmpty()) {
        parts << district.trimmed();
    }
    if (!address.trimmed().isEmpty()) {
        parts << address.trimmed();
    }

    return parts.isEmpty() ? "Unknown location" : parts.join(", ");
}

QColor colorForHeritageCategory(const QString &category)
{
    const QString normalized = category.trimmed().toLower();

    // 顏色先固定在程式裡，Phase 6 Layer System 再改成可由使用者設定。
    if (normalized.contains("historic site")) {
        return QColor(237, 180, 72);
    }
    if (normalized.contains("historical building")) {
        return QColor(98, 180, 255);
    }
    if (normalized.contains("historic district")) {
        return QColor(115, 220, 150);
    }
    if (normalized.contains("cultural facility")) {
        return QColor(235, 110, 120);
    }
    if (normalized.contains("national")) {
        return QColor(190, 140, 255);
    }

    // 沒有對應類型時給中性藍灰色，避免點位消失。
    return QColor(160, 180, 210);
}