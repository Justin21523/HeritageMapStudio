#include "models/SearchFilterState.h"

#include <QRegularExpression>
#include <QSet>

namespace {

QString normalizedText(const QString &text)
{
    return text.trimmed().toLower();
}

bool textEqualsCaseInsensitive(const QString &left, const QString &right)
{
    return normalizedText(left) == normalizedText(right);
}

bool facetMatches(const QString &facetValue, const QString &siteValue)
{
    // facet 沒選時代表 All。
    if (facetValue.trimmed().isEmpty()) {
        return true;
    }

    return textEqualsCaseInsensitive(facetValue, siteValue);
}

bool keywordMatches(const SearchFilterState &state, const HeritageSite &site)
{
    const QString keyword = state.keyword.trimmed();

    if (keyword.isEmpty()) {
        return true;
    }

    QStringList searchableParts = {
        site.title,
        site.category,
        site.displayPeriod(),
        site.city,
        site.district,
        site.address,
        site.preservationStatus,
        site.sourceName,
        site.tags
    };

    if (state.searchDescription) {
        searchableParts << site.description;
    }

    const QString searchableText = normalizedText(searchableParts.join(" "));

    // 支援多關鍵字：例如 "Taipei temple" 需要兩個 token 都命中。
    const QStringList tokens = keyword.split(QRegularExpression("\\s+"),
                                             Qt::SkipEmptyParts);

    for (const QString &token : tokens) {
        if (!searchableText.contains(normalizedText(token))) {
            return false;
        }
    }

    return true;
}

bool tagMatches(const QString &selectedTag, const QString &siteTags)
{
    if (selectedTag.trimmed().isEmpty()) {
        return true;
    }

    const QString normalizedSelectedTag = normalizedText(selectedTag);

    for (const QString &tag : splitHeritageTags(siteTags)) {
        if (normalizedText(tag) == normalizedSelectedTag) {
            return true;
        }
    }

    return false;
}

} // namespace

SearchFilterState SearchFilterState::createDefault()
{
    SearchFilterState state;
    state.searchDescription = true;
    return state;
}

bool SearchFilterState::isDefault() const
{
    return keyword.trimmed().isEmpty() &&
           category.trimmed().isEmpty() &&
           district.trimmed().isEmpty() &&
           periodLabel.trimmed().isEmpty() &&
           preservationStatus.trimmed().isEmpty() &&
           sourceName.trimmed().isEmpty() &&
           tag.trimmed().isEmpty() &&
           searchDescription;
}

bool SearchFilterState::matchesSite(const HeritageSite &site) const
{
    if (!keywordMatches(*this, site)) {
        return false;
    }

    if (!facetMatches(category, site.category)) {
        return false;
    }

    if (!facetMatches(district, site.district)) {
        return false;
    }

    if (!facetMatches(periodLabel, site.periodLabel)) {
        return false;
    }

    if (!facetMatches(preservationStatus, site.preservationStatus)) {
        return false;
    }

    if (!facetMatches(sourceName, site.sourceName)) {
        return false;
    }

    if (!tagMatches(tag, site.tags)) {
        return false;
    }

    return true;
}

QString SearchFilterState::displayLabel() const
{
    if (isDefault()) {
        return "Search filters cleared.";
    }

    QStringList activeFilters;

    if (!keyword.trimmed().isEmpty()) {
        activeFilters << QString("keyword \"%1\"").arg(keyword.trimmed());
    }

    if (!category.trimmed().isEmpty()) {
        activeFilters << QString("category \"%1\"").arg(category);
    }

    if (!district.trimmed().isEmpty()) {
        activeFilters << QString("district \"%1\"").arg(district);
    }

    if (!periodLabel.trimmed().isEmpty()) {
        activeFilters << QString("period \"%1\"").arg(periodLabel);
    }

    if (!preservationStatus.trimmed().isEmpty()) {
        activeFilters << QString("status \"%1\"").arg(preservationStatus);
    }

    if (!sourceName.trimmed().isEmpty()) {
        activeFilters << QString("source \"%1\"").arg(sourceName);
    }

    if (!tag.trimmed().isEmpty()) {
        activeFilters << QString("tag \"%1\"").arg(tag);
    }

    return QString("Search filters: %1").arg(activeFilters.join(", "));
}

QStringList splitHeritageTags(const QString &tags)
{
    QStringList result;
    QSet<QString> seen;

    // 支援常見分隔符：; , # ｜ 、 ，
    const QStringList rawTags = tags.split(QRegularExpression("[;,#|、，]+"),
                                           Qt::SkipEmptyParts);

    for (const QString &rawTag : rawTags) {
        const QString tag = rawTag.trimmed();

        if (tag.isEmpty()) {
            continue;
        }

        const QString key = normalizedText(tag);

        if (seen.contains(key)) {
            continue;
        }

        seen.insert(key);
        result.append(tag);
    }

    return result;
}