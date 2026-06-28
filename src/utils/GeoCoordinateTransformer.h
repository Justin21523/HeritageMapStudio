#pragma once

#include "models/HeritageSite.h"

#include <QString>
#include <QVector>

// GeoProjectionInfo 記錄這次資料集投影的基本資訊。
// 這些資訊可以顯示在 log / status bar，讓使用者知道目前地圖是怎麼被投影的。
struct GeoProjectionInfo {
    bool hasGeoCoordinates = false;

    double originLatitude = 0.0;
    double originLongitude = 0.0;

    int projectedSiteCount = 0;
    int fallbackSiteCount = 0;

    QString summary() const;
};

// GeoCoordinateTransformer 負責把 latitude / longitude 轉成 OpenGL 可畫的 local meter coordinate。
// 重點：這個類別不碰 UI、不碰 database，只做座標轉換。
class GeoCoordinateTransformer final
{
public:
    GeoCoordinateTransformer() = delete;

    // 判斷一筆 HeritageSite 是否有可用經緯度。
    static bool hasValidGeoCoordinate(const HeritageSite &site);

    // 將整批文化地點投影到本地座標。
    // 若 site 有 latitude/longitude，會用投影後的 meter coordinate 覆蓋 mapX/mapY。
    // 若沒有有效經緯度，保留原本 mapX/mapY 作為 fallback。
    static QVector<HeritageSite> projectSitesToLocalMeters(const QVector<HeritageSite> &sites,
                                                           GeoProjectionInfo *info = nullptr);

private:
    // 角度轉弧度。
    static double degreesToRadians(double degrees);

    // 計算資料集中心點，作為 local projection origin。
    static void computeOrigin(const QVector<HeritageSite> &sites,
                              double *originLatitude,
                              double *originLongitude,
                              int *validCoordinateCount);

    // 單筆經緯度轉成本地 meter 座標。
    static void projectSinglePoint(double latitude,
                                   double longitude,
                                   double originLatitude,
                                   double originLongitude,
                                   double *xMeters,
                                   double *yMeters);
};