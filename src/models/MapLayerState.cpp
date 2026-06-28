#include "models/MapLayerState.h"

MapLayerState MapLayerState::createDefault()
{
    MapLayerState state;
    state.heritageSitesVisible = true;
    state.heritageSitesOpacity = 1.0f;

    // 預設所有 heritage category 都顯示。
    for (const HeritageCategoryLayerInfo &layerInfo : defaultHeritageCategoryLayers()) {
        state.visibleHeritageCategoryKeys.insert(layerInfo.key);
    }

    return state;
}

bool MapLayerState::isHeritageCategoryVisible(const QString &category) const
{
    if (!heritageSitesVisible) {
        return false;
    }

    const QString key = heritageCategoryLayerKey(category);
    return visibleHeritageCategoryKeys.contains(key);
}

QString heritageCategoryLayerKey(const QString &category)
{
    const QString normalized = category.trimmed().toLower();

    // 注意順序：National Historic Site 也包含 historic site 字樣，
    // 所以必須先判斷 national，再判斷一般 historic site。
    if (normalized.contains("national historic site") || normalized.contains("national")) {
        return "national_historic_site";
    }

    if (normalized.contains("historical building")) {
        return "historical_building";
    }

    if (normalized.contains("historic district")) {
        return "historic_district";
    }

    if (normalized.contains("historic site")) {
        return "historic_site";
    }

    if (normalized.contains("cultural facility")) {
        return "cultural_facility";
    }

    return "other";
}

QVector<HeritageCategoryLayerInfo> defaultHeritageCategoryLayers()
{
    return {
        {
            "historic_site",
            "Historic Sites",
            "Registered or recognized cultural heritage sites."
        },
        {
            "historical_building",
            "Historical Buildings",
            "Buildings with historical, architectural, or urban memory value."
        },
        {
            "historic_district",
            "Historic Districts",
            "Street blocks, neighborhoods, or cultural landscape areas."
        },
        {
            "cultural_facility",
            "Cultural Facilities",
            "Museums, cultural venues, and adaptive reuse facilities."
        },
        {
            "national_historic_site",
            "National Historic Sites",
            "National-level heritage sites and official historic monuments."
        },
        {
            "other",
            "Other Categories",
            "Imported records that do not match the predefined categories."
        }
    };
}