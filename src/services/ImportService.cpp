#include "services/ImportService.h"
#include "services/CsvReader.h"
#include "repositories/HeritageSiteRepository.h"

#include <QSet>

namespace {

// 必填基本欄位 aliases。
const QStringList kTitleAliases = {"title", "name", "site_title"};
const QStringList kCategoryAliases = {"category", "type", "site_type"};

// Phase 4：座標可以走兩條路。
// 1. latitude + longitude：正式 GIS 資料優先使用。
// 2. map_x + map_y：保留給舊資料或手動測試資料。
const QStringList kLatitudeAliases = {"latitude", "lat"};
const QStringList kLongitudeAliases = {"longitude", "lng", "lon"};
const QStringList kMapXAliases = {"map_x", "mapx", "x"};
const QStringList kMapYAliases = {"map_y", "mapy", "y"};

// 選填欄位 aliases。
const QStringList kDescriptionAliases = {"description", "desc", "summary"};
const QStringList kAddressAliases = {"address"};
const QStringList kCityAliases = {"city"};
const QStringList kDistrictAliases = {"district"};
const QStringList kStartYearAliases = {"start_year", "startyear", "year", "built_year"};
const QStringList kEndYearAliases = {"end_year", "endyear"};
const QStringList kPeriodLabelAliases = {"period_label", "period", "era"};
const QStringList kSourceNameAliases = {"source_name", "source"};
const QStringList kSourceUrlAliases = {"source_url", "url"};
const QStringList kPreservationStatusAliases = {"preservation_status", "status"};
const QStringList kTagsAliases = {"tags", "keywords"};

} // namespace

ImportService::ImportService(HeritageSiteRepository &repository)
    : m_repository(repository)
{
}

bool ImportService::importHeritageSitesFromCsv(const QString &filePath,
                                               ImportResult *result,
                                               QString *errorMessage) const
{
    ImportResult localResult;
    ImportResult &output = result ? *result : localResult;
    output = ImportResult{};

    CsvReader reader;
    CsvTable table;
    QString readError;

    if (!reader.readFile(filePath, &table, &readError)) {
        if (errorMessage) {
            *errorMessage = readError;
        }
        return false;
    }

    output.parsedRows = table.rows.size();

    QStringList missingHeaders;
    if (!validateRequiredHeaders(table.headers, &missingHeaders)) {
        if (errorMessage) {
            *errorMessage = QString("Missing required CSV headers: %1")
                                .arg(missingHeaders.join(", "));
        }
        return false;
    }

    QVector<HeritageSite> sitesToInsert;
    sitesToInsert.reserve(table.rows.size());

    for (int rowIndex = 0; rowIndex < table.rows.size(); ++rowIndex) {
        const int csvLineNumber = rowIndex + 2; // +1 header，+1 因為 line number 從 1 開始。
        HeritageSite site;

        if (!mapRowToHeritageSite(table.headers,
                                  table.rows[rowIndex],
                                  csvLineNumber,
                                  &site,
                                  &output.warnings)) {
            ++output.skippedRows;
            continue;
        }

        sitesToInsert.append(site);
    }

    if (sitesToInsert.isEmpty()) {
        if (errorMessage) {
            *errorMessage = "No valid heritage sites found in CSV.";
        }
        return false;
    }

    QString insertError;
    if (!m_repository.insertMany(sitesToInsert, &insertError)) {
        if (errorMessage) {
            *errorMessage = insertError;
        }
        return false;
    }

    output.insertedRows = sitesToInsert.size();
    return true;
}

bool ImportService::validateRequiredHeaders(const QStringList &headers,
                                            QStringList *missingHeaders)
{
    if (missingHeaders) {
        missingHeaders->clear();
    }

    bool valid = true;

    // title 必須存在。
    if (findHeaderIndex(headers, kTitleAliases) < 0) {
        valid = false;
        if (missingHeaders) {
            missingHeaders->append("title");
        }
    }

    // category 必須存在。
    if (findHeaderIndex(headers, kCategoryAliases) < 0) {
        valid = false;
        if (missingHeaders) {
            missingHeaders->append("category");
        }
    }

    // Phase 4：座標欄位允許兩種格式。
    const bool hasLatLngHeaders =
        findHeaderIndex(headers, kLatitudeAliases) >= 0 &&
        findHeaderIndex(headers, kLongitudeAliases) >= 0;

    const bool hasMapXYHeaders =
        findHeaderIndex(headers, kMapXAliases) >= 0 &&
        findHeaderIndex(headers, kMapYAliases) >= 0;

    if (!hasLatLngHeaders && !hasMapXYHeaders) {
        valid = false;
        if (missingHeaders) {
            missingHeaders->append("latitude+longitude or map_x+map_y");
        }
    }

    return valid;
}

bool ImportService::mapRowToHeritageSite(const QStringList &headers,
                                         const QStringList &row,
                                         int csvLineNumber,
                                         HeritageSite *site,
                                         QStringList *warnings) const
{
    if (!site) {
        if (warnings) {
            warnings->append(QString("Line %1 skipped: internal mapping error.").arg(csvLineNumber));
        }
        return false;
    }

    // 必填文字欄位。
    site->title = valueByAliases(headers, row, kTitleAliases);
    site->category = valueByAliases(headers, row, kCategoryAliases);

    if (site->title.trimmed().isEmpty()) {
        if (warnings) {
            warnings->append(QString("Line %1 skipped: title is empty.").arg(csvLineNumber));
        }
        return false;
    }

    if (site->category.trimmed().isEmpty()) {
        if (warnings) {
            warnings->append(QString("Line %1 skipped: category is empty.").arg(csvLineNumber));
        }
        return false;
    }

    // Phase 4：優先使用 latitude / longitude。
    // 若該列沒有經緯度，再 fallback 到 map_x / map_y。
    const QString latitudeText = valueByAliases(headers, row, kLatitudeAliases);
    const QString longitudeText = valueByAliases(headers, row, kLongitudeAliases);
    const QString mapXText = valueByAliases(headers, row, kMapXAliases);
    const QString mapYText = valueByAliases(headers, row, kMapYAliases);

    const bool rowHasLatLng =
        !latitudeText.trimmed().isEmpty() &&
        !longitudeText.trimmed().isEmpty();

    const bool rowHasMapXY =
        !mapXText.trimmed().isEmpty() &&
        !mapYText.trimmed().isEmpty();

    if (rowHasLatLng) {
        // 正式 GIS 資料路線：先存 lat/lng，mapX/mapY 之後由 GeoCoordinateTransformer 計算。
        if (!parseRequiredDouble(latitudeText, "latitude", csvLineNumber, &site->latitude, warnings)) {
            return false;
        }

        if (!parseRequiredDouble(longitudeText, "longitude", csvLineNumber, &site->longitude, warnings)) {
            return false;
        }

        // 若 CSV 同時提供 map_x/map_y，仍可保留；但顯示時會優先用經緯度投影結果。
        if (rowHasMapXY) {
            site->mapX = parseOptionalDouble(mapXText, "map_x", csvLineNumber, warnings);
            site->mapY = parseOptionalDouble(mapYText, "map_y", csvLineNumber, warnings);
        }
    } else if (rowHasMapXY) {
        // fallback 路線：沒有經緯度時才使用手動 map_x/map_y。
        if (!parseRequiredDouble(mapXText, "map_x", csvLineNumber, &site->mapX, warnings)) {
            return false;
        }

        if (!parseRequiredDouble(mapYText, "map_y", csvLineNumber, &site->mapY, warnings)) {
            return false;
        }
    } else {
        if (warnings) {
            warnings->append(QString("Line %1 skipped: missing coordinate pair. Provide latitude+longitude or map_x+map_y.")
                                 .arg(csvLineNumber));
        }
        return false;
    }

    // 選填 metadata。
    site->description = valueByAliases(headers, row, kDescriptionAliases);
    site->address = valueByAliases(headers, row, kAddressAliases);
    site->city = valueByAliases(headers, row, kCityAliases);
    site->district = valueByAliases(headers, row, kDistrictAliases);
    site->startYear = parseOptionalInt(valueByAliases(headers, row, kStartYearAliases),
                                       "start_year",
                                       csvLineNumber,
                                       warnings);
    site->endYear = parseOptionalInt(valueByAliases(headers, row, kEndYearAliases),
                                     "end_year",
                                     csvLineNumber,
                                     warnings);
    site->periodLabel = valueByAliases(headers, row, kPeriodLabelAliases);
    site->sourceName = valueByAliases(headers, row, kSourceNameAliases);
    site->sourceUrl = valueByAliases(headers, row, kSourceUrlAliases);
    site->preservationStatus = valueByAliases(headers, row, kPreservationStatusAliases);
    site->tags = valueByAliases(headers, row, kTagsAliases);

    return true;
}

QString ImportService::normalizeHeader(const QString &header)
{
    QString normalized = header.trimmed().toLower();

    // 將常見分隔方式統一成底線，讓 header 比對比較寬容。
    normalized.replace("-", "_");
    normalized.replace(" ", "_");

    return normalized;
}

int ImportService::findHeaderIndex(const QStringList &headers, const QStringList &aliases)
{
    QSet<QString> normalizedAliases;

    for (const QString &alias : aliases) {
        normalizedAliases.insert(normalizeHeader(alias));
    }

    for (int index = 0; index < headers.size(); ++index) {
        if (normalizedAliases.contains(normalizeHeader(headers[index]))) {
            return index;
        }
    }

    return -1;
}

QString ImportService::valueByAliases(const QStringList &headers,
                                      const QStringList &row,
                                      const QStringList &aliases)
{
    const int index = findHeaderIndex(headers, aliases);

    if (index < 0 || index >= row.size()) {
        return {};
    }

    return row[index].trimmed();
}

bool ImportService::parseRequiredDouble(const QString &text,
                                        const QString &fieldName,
                                        int csvLineNumber,
                                        double *value,
                                        QStringList *warnings)
{
    if (!value) {
        return false;
    }

    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty()) {
        if (warnings) {
            warnings->append(QString("Line %1 skipped: required field %2 is empty.")
                                 .arg(csvLineNumber)
                                 .arg(fieldName));
        }
        return false;
    }

    bool ok = false;
    const double parsedValue = trimmed.toDouble(&ok);

    if (!ok) {
        if (warnings) {
            warnings->append(QString("Line %1 skipped: field %2 is not a valid number: %3")
                                 .arg(csvLineNumber)
                                 .arg(fieldName, trimmed));
        }
        return false;
    }

    *value = parsedValue;
    return true;
}

int ImportService::parseOptionalInt(const QString &text,
                                    const QString &fieldName,
                                    int csvLineNumber,
                                    QStringList *warnings)
{
    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty()) {
        return 0;
    }

    bool ok = false;
    const int value = trimmed.toInt(&ok);

    if (!ok) {
        if (warnings) {
            warnings->append(QString("Line %1: optional field %2 is not a valid integer. Value ignored: %3")
                                 .arg(csvLineNumber)
                                 .arg(fieldName, trimmed));
        }
        return 0;
    }

    return value;
}

double ImportService::parseOptionalDouble(const QString &text,
                                          const QString &fieldName,
                                          int csvLineNumber,
                                          QStringList *warnings)
{
    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty()) {
        return 0.0;
    }

    bool ok = false;
    const double value = trimmed.toDouble(&ok);

    if (!ok) {
        if (warnings) {
            warnings->append(QString("Line %1: optional field %2 is not a valid number. Value ignored: %3")
                                 .arg(csvLineNumber)
                                 .arg(fieldName, trimmed));
        }
        return 0.0;
    }

    return value;
}