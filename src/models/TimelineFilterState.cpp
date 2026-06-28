#include "models/TimelineFilterState.h"

#include <algorithm>

namespace {

// 判斷年份是否可用。
// 目前 HeritageSite 使用 0 表示未填年份。
bool isValidYear(int year)
{
    return year != 0;
}

} // namespace

QString TimelineRangeInfo::summary() const
{
    if (!hasDatedSites) {
        return QString("Timeline: no dated heritage records. Undated records: %1.")
            .arg(undatedSiteCount);
    }

    return QString("Timeline range: %1–%2. Dated records: %3, undated records: %4.")
        .arg(minYear)
        .arg(maxYear)
        .arg(datedSiteCount)
        .arg(undatedSiteCount);
}

TimelineRangeInfo TimelineRangeInfo::fromSites(const QVector<HeritageSite> &sites)
{
    TimelineRangeInfo info;

    bool firstDatedSite = true;

    for (const HeritageSite &site : sites) {
        const bool hasStartYear = isValidYear(site.startYear);
        const bool hasEndYear = isValidYear(site.endYear);

        if (!hasStartYear && !hasEndYear) {
            ++info.undatedSiteCount;
            continue;
        }

        int siteStart = hasStartYear ? site.startYear : site.endYear;
        int siteEnd = hasEndYear ? site.endYear : site.startYear;

        // 如果資料不小心 start/end 寫反，這裡先修正為合理區間。
        if (siteEnd < siteStart) {
            std::swap(siteStart, siteEnd);
        }

        if (firstDatedSite) {
            info.minYear = siteStart;
            info.maxYear = siteEnd;
            firstDatedSite = false;
        } else {
            info.minYear = std::min(info.minYear, siteStart);
            info.maxYear = std::max(info.maxYear, siteEnd);
        }

        ++info.datedSiteCount;
    }

    info.hasDatedSites = info.datedSiteCount > 0;
    return info;
}

TimelineFilterState TimelineFilterState::createDisabled()
{
    TimelineFilterState state;
    state.enabled = false;
    state.startYear = 0;
    state.endYear = 0;
    state.showUndatedSites = true;
    return state;
}

TimelineFilterState TimelineFilterState::createForRange(int startYear,
                                                        int endYear,
                                                        bool showUndatedSites)
{
    TimelineFilterState state;
    state.enabled = true;
    state.startYear = std::min(startYear, endYear);
    state.endYear = std::max(startYear, endYear);
    state.showUndatedSites = showUndatedSites;
    return state;
}

bool TimelineFilterState::matchesSite(const HeritageSite &site) const
{
    if (!enabled) {
        return true;
    }

    return matchesYearRange(site.startYear, site.endYear);
}

bool TimelineFilterState::matchesYearRange(int siteStartYear, int siteEndYear) const
{
    if (!enabled) {
        return true;
    }

    const bool hasStartYear = isValidYear(siteStartYear);
    const bool hasEndYear = isValidYear(siteEndYear);

    if (!hasStartYear && !hasEndYear) {
        return showUndatedSites;
    }

    int siteStart = hasStartYear ? siteStartYear : siteEndYear;
    int siteEnd = hasEndYear ? siteEndYear : siteStartYear;

    if (siteEnd < siteStart) {
        std::swap(siteStart, siteEnd);
    }

    const int filterStart = std::min(startYear, endYear);
    const int filterEnd = std::max(startYear, endYear);

    // 區間有交集就顯示。
    // 例如 site = 1895–1945，filter = 1930–1960，兩者有交集。
    return siteEnd >= filterStart && siteStart <= filterEnd;
}

QString TimelineFilterState::displayLabel() const
{
    if (!enabled) {
        return "Timeline filter disabled";
    }

    return QString("Timeline: %1–%2%3")
        .arg(std::min(startYear, endYear))
        .arg(std::max(startYear, endYear))
        .arg(showUndatedSites ? " + undated" : "");
}