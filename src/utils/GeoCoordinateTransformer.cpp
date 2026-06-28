#include "utils/GeoCoordinateTransformer.h"

#include <QtMath>

namespace {
constexpr double kEarthRadiusMeters = 6378137.0;
constexpr double kMaxMercatorLatitude = 85.05112878;
}

QString GeoProjectionInfo::summary() const
{
    if (!hasGeoCoordinates) {
        return QString("Projection: fallback map_x/map_y only. Projected sites: 0, fallback sites: %1.")
            .arg(fallbackSiteCount);
    }

    return QString("Projection: local meters. Origin lat/lng: %1, %2. Projected sites: %3, fallback sites: %4.")
        .arg(originLatitude, 0, 'f', 6)
        .arg(originLongitude, 0, 'f', 6)
        .arg(projectedSiteCount)
        .arg(fallbackSiteCount);
}

bool GeoCoordinateTransformer::hasValidGeoCoordinate(const HeritageSite &site)
{
    // 緯度必須在合理範圍內。
    if (site.latitude < -kMaxMercatorLatitude || site.latitude > kMaxMercatorLatitude) {
        return false;
    }

    // 經度必須在 WGS84 常見範圍內。
    if (site.longitude < -180.0 || site.longitude > 180.0) {
        return false;
    }

    // Phase 2/3 預設空值是 0,0；這通常不是台灣資料，所以視為未填。
    // 如果未來真的要支援赤道附近資料，再改成 nullable 欄位或 has_coordinate flag。
    if (qFuzzyIsNull(site.latitude) && qFuzzyIsNull(site.longitude)) {
        return false;
    }

    return true;
}

QVector<HeritageSite> GeoCoordinateTransformer::projectSitesToLocalMeters(const QVector<HeritageSite> &sites,
                                                                          GeoProjectionInfo *info)
{
    QVector<HeritageSite> projectedSites = sites;

    double originLatitude = 0.0;
    double originLongitude = 0.0;
    int validCoordinateCount = 0;

    computeOrigin(sites, &originLatitude, &originLongitude, &validCoordinateCount);

    GeoProjectionInfo localInfo;
    localInfo.hasGeoCoordinates = validCoordinateCount > 0;
    localInfo.originLatitude = originLatitude;
    localInfo.originLongitude = originLongitude;

    for (HeritageSite &site : projectedSites) {
        if (!hasValidGeoCoordinate(site)) {
            // 沒有經緯度時，保留原本 mapX/mapY。
            // 這讓舊 CSV 或手動測試資料仍然可以顯示。
            ++localInfo.fallbackSiteCount;
            continue;
        }

        double xMeters = 0.0;
        double yMeters = 0.0;

        projectSinglePoint(site.latitude,
                           site.longitude,
                           originLatitude,
                           originLongitude,
                           &xMeters,
                           &yMeters);

        site.mapX = xMeters;
        site.mapY = yMeters;

        ++localInfo.projectedSiteCount;
    }

    if (info) {
        *info = localInfo;
    }

    return projectedSites;
}

double GeoCoordinateTransformer::degreesToRadians(double degrees)
{
    return degrees * M_PI / 180.0;
}

void GeoCoordinateTransformer::computeOrigin(const QVector<HeritageSite> &sites,
                                             double *originLatitude,
                                             double *originLongitude,
                                             int *validCoordinateCount)
{
    double minLatitude = 0.0;
    double maxLatitude = 0.0;
    double minLongitude = 0.0;
    double maxLongitude = 0.0;

    bool first = true;
    int count = 0;

    for (const HeritageSite &site : sites) {
        if (!hasValidGeoCoordinate(site)) {
            continue;
        }

        if (first) {
            minLatitude = maxLatitude = site.latitude;
            minLongitude = maxLongitude = site.longitude;
            first = false;
        } else {
            minLatitude = qMin(minLatitude, site.latitude);
            maxLatitude = qMax(maxLatitude, site.latitude);
            minLongitude = qMin(minLongitude, site.longitude);
            maxLongitude = qMax(maxLongitude, site.longitude);
        }

        ++count;
    }

    if (count == 0) {
        if (originLatitude) {
            *originLatitude = 0.0;
        }
        if (originLongitude) {
            *originLongitude = 0.0;
        }
        if (validCoordinateCount) {
            *validCoordinateCount = 0;
        }
        return;
    }

    // 使用 bounding box 中心當 projection origin。
    // 對城市尺度資料來說，這能讓 OpenGL 座標分布在原點附近，互動比較穩。
    if (originLatitude) {
        *originLatitude = (minLatitude + maxLatitude) * 0.5;
    }
    if (originLongitude) {
        *originLongitude = (minLongitude + maxLongitude) * 0.5;
    }
    if (validCoordinateCount) {
        *validCoordinateCount = count;
    }
}

void GeoCoordinateTransformer::projectSinglePoint(double latitude,
                                                  double longitude,
                                                  double originLatitude,
                                                  double originLongitude,
                                                  double *xMeters,
                                                  double *yMeters)
{
    // 本地 equirectangular projection：
    // x = R * Δlon * cos(originLat)
    // y = R * Δlat
    //
    // 這不是全球精準投影，但對台北/城市尺度的文化地點視覺化很夠用。
    // 好處是公式直覺、單位是 meter、OpenGL 互動容易控制。
    const double latRad = degreesToRadians(latitude);
    const double lonRad = degreesToRadians(longitude);
    const double originLatRad = degreesToRadians(originLatitude);
    const double originLonRad = degreesToRadians(originLongitude);

    if (xMeters) {
        *xMeters = kEarthRadiusMeters * (lonRad - originLonRad) * qCos(originLatRad);
    }

    if (yMeters) {
        *yMeters = kEarthRadiusMeters * (latRad - originLatRad);
    }
}