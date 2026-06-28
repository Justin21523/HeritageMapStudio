#include "repositories/HeritageSiteRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
// 查詢欄位集中管理，避免 findAll/findById 欄位順序不一致。
const char *kHeritageSiteColumns =
    "id, title, category, description, latitude, longitude, map_x, map_y, "
    "address, city, district, start_year, end_year, period_label, "
    "source_name, source_url, preservation_status, tags, created_at, updated_at";
}

HeritageSiteRepository::HeritageSiteRepository(QSqlDatabase database)
    : m_database(database)
{
}

QVector<HeritageSite> HeritageSiteRepository::findAll(QString *errorMessage) const
{
    QVector<HeritageSite> sites;

    QSqlQuery query(m_database);
    const QString sql = QString("SELECT %1 FROM heritage_sites ORDER BY start_year, title")
                            .arg(kHeritageSiteColumns);

    if (!query.exec(sql)) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return sites;
    }

    while (query.next()) {
        sites.append(siteFromQuery(query));
    }

    return sites;
}

std::optional<HeritageSite> HeritageSiteRepository::findById(int id, QString *errorMessage) const
{
    QSqlQuery query(m_database);
    const QString sql = QString("SELECT %1 FROM heritage_sites WHERE id = :id")
                            .arg(kHeritageSiteColumns);

    query.prepare(sql);
    query.bindValue(":id", id);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return std::nullopt;
    }

    if (!query.next()) {
        return std::nullopt;
    }

    return siteFromQuery(query);
}

bool HeritageSiteRepository::insert(HeritageSite &site, QString *errorMessage) const
{
    QSqlQuery query(m_database);

    // 使用 named placeholders，避免自己手動拼 SQL 字串造成 escaping 問題。
    query.prepare(
        "INSERT INTO heritage_sites ("
        "title, category, description, latitude, longitude, map_x, map_y, "
        "address, city, district, start_year, end_year, period_label, "
        "source_name, source_url, preservation_status, tags"
        ") VALUES ("
        ":title, :category, :description, :latitude, :longitude, :map_x, :map_y, "
        ":address, :city, :district, :start_year, :end_year, :period_label, "
        ":source_name, :source_url, :preservation_status, :tags"
        ")");

    query.bindValue(":title", site.title);
    query.bindValue(":category", site.category);
    query.bindValue(":description", site.description);
    query.bindValue(":latitude", site.latitude);
    query.bindValue(":longitude", site.longitude);
    query.bindValue(":map_x", site.mapX);
    query.bindValue(":map_y", site.mapY);
    query.bindValue(":address", site.address);
    query.bindValue(":city", site.city);
    query.bindValue(":district", site.district);
    query.bindValue(":start_year", site.startYear);
    query.bindValue(":end_year", site.endYear);
    query.bindValue(":period_label", site.periodLabel);
    query.bindValue(":source_name", site.sourceName);
    query.bindValue(":source_url", site.sourceUrl);
    query.bindValue(":preservation_status", site.preservationStatus);
    query.bindValue(":tags", site.tags);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    // SQLite AUTOINCREMENT 產生的新 id，寫回 model，方便後續選取/更新。
    site.id = query.lastInsertId().toInt();
    return true;
}

bool HeritageSiteRepository::update(const HeritageSite &site, QString *errorMessage) const
{
    if (site.id < 0) {
        if (errorMessage) {
            *errorMessage = "Cannot update heritage site: invalid id.";
        }
        return false;
    }

    QSqlQuery query(m_database);

    // 使用 prepare + bindValue，避免直接拼接 SQL 字串。
    query.prepare(
        "UPDATE heritage_sites SET "
        "title = :title, "
        "category = :category, "
        "description = :description, "
        "latitude = :latitude, "
        "longitude = :longitude, "
        "map_x = :map_x, "
        "map_y = :map_y, "
        "address = :address, "
        "city = :city, "
        "district = :district, "
        "start_year = :start_year, "
        "end_year = :end_year, "
        "period_label = :period_label, "
        "source_name = :source_name, "
        "source_url = :source_url, "
        "preservation_status = :preservation_status, "
        "tags = :tags, "
        "updated_at = CURRENT_TIMESTAMP "
        "WHERE id = :id");

    query.bindValue(":title", site.title);
    query.bindValue(":category", site.category);
    query.bindValue(":description", site.description);
    query.bindValue(":latitude", site.latitude);
    query.bindValue(":longitude", site.longitude);
    query.bindValue(":map_x", site.mapX);
    query.bindValue(":map_y", site.mapY);
    query.bindValue(":address", site.address);
    query.bindValue(":city", site.city);
    query.bindValue(":district", site.district);
    query.bindValue(":start_year", site.startYear);
    query.bindValue(":end_year", site.endYear);
    query.bindValue(":period_label", site.periodLabel);
    query.bindValue(":source_name", site.sourceName);
    query.bindValue(":source_url", site.sourceUrl);
    query.bindValue(":preservation_status", site.preservationStatus);
    query.bindValue(":tags", site.tags);
    query.bindValue(":id", site.id);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    if (query.numRowsAffected() == 0) {
        if (errorMessage) {
            *errorMessage = QString("No heritage site found with id %1.").arg(site.id);
        }
        return false;
    }

    return true;
}

bool HeritageSiteRepository::insertMany(QVector<HeritageSite> &sites, QString *errorMessage) const
{
    if (sites.isEmpty()) {
        return true;
    }

    // QSqlDatabase 是 connection handle；複製一份仍然指向同一個 connection。
    // 這樣可以在 const member function 裡使用 transaction / commit / rollback。
    QSqlDatabase database = m_database;

    if (!database.transaction()) {
        if (errorMessage) {
            *errorMessage = database.lastError().text();
        }
        return false;
    }

    for (HeritageSite &site : sites) {
        QString insertError;

        if (!insert(site, &insertError)) {
            database.rollback();

            if (errorMessage) {
                *errorMessage = QString("Failed to insert site '%1': %2")
                                    .arg(site.title, insertError);
            }

            return false;
        }
    }

    if (!database.commit()) {
        if (errorMessage) {
            *errorMessage = database.lastError().text();
        }
        return false;
    }

    return true;
}

int HeritageSiteRepository::count(QString *errorMessage) const
{
    QSqlQuery query(m_database);

    if (!query.exec("SELECT COUNT(*) FROM heritage_sites")) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return -1;
    }

    if (!query.next()) {
        return 0;
    }

    return query.value(0).toInt();
}

bool HeritageSiteRepository::ensureSeedData(QString *errorMessage) const
{
    const int existingCount = count(errorMessage);
    if (existingCount < 0) {
        return false;
    }

    // 只有空 database 才塞 demo data，避免每次開程式都重複新增。
    if (existingCount > 0) {
        return true;
    }

    QVector<HeritageSite> seedSites;

    // Phase 2 先用台北文化地點樣本；Phase 3 再加入 CSV/JSON 匯入。
    HeritageSite northGate;
    northGate.title = "North Gate (Beimen)";
    northGate.category = "Historic Site";
    northGate.description = "A remaining gate of the old Taipei city wall and an important landmark of late Qing urban history.";
    northGate.latitude = 25.0478;
    northGate.longitude = 121.5119;
    northGate.mapX = 120.0;
    northGate.mapY = 80.0;
    northGate.address = "Zhongzheng District";
    northGate.city = "Taipei";
    northGate.district = "Zhongzheng";
    northGate.startYear = 1884;
    northGate.periodLabel = "Late Qing Dynasty";
    northGate.sourceName = "Portfolio sample dataset";
    northGate.preservationStatus = "Preserved";
    northGate.tags = "city gate; Qing dynasty; Taipei wall";
    seedSites.append(northGate);

    HeritageSite bopiliao;
    bopiliao.title = "Bopiliao Historic Block";
    bopiliao.category = "Historical Building";
    bopiliao.description = "A preserved historic street block in Wanhua, showing urban texture from Qing and Japanese periods.";
    bopiliao.latitude = 25.0369;
    bopiliao.longitude = 121.5033;
    bopiliao.mapX = -180.0;
    bopiliao.mapY = -90.0;
    bopiliao.address = "Kangding Road";
    bopiliao.city = "Taipei";
    bopiliao.district = "Wanhua";
    bopiliao.startYear = 1800;
    bopiliao.periodLabel = "Qing / Japanese Period";
    bopiliao.sourceName = "Portfolio sample dataset";
    bopiliao.preservationStatus = "Preserved";
    bopiliao.tags = "historic block; Wanhua; urban heritage";
    seedSites.append(bopiliao);

    HeritageSite dihua;
    dihua.title = "Dihua Street";
    dihua.category = "Historic District";
    dihua.description = "A major commercial street in Dadaocheng with shophouses, tea trade history, and adaptive reuse cases.";
    dihua.latitude = 25.0555;
    dihua.longitude = 121.5094;
    dihua.mapX = -260.0;
    dihua.mapY = 150.0;
    dihua.address = "Dihua Street";
    dihua.city = "Taipei";
    dihua.district = "Datong";
    dihua.startYear = 1850;
    dihua.periodLabel = "Qing / Japanese Period";
    dihua.sourceName = "Portfolio sample dataset";
    dihua.preservationStatus = "Active Reuse";
    dihua.tags = "Dadaocheng; commerce; tea trade; shophouse";
    seedSites.append(dihua);

    HeritageSite redHouse;
    redHouse.title = "Ximending Red House";
    redHouse.category = "Cultural Facility";
    redHouse.description = "A Japanese-period market building that has been transformed into a cultural and creative venue.";
    redHouse.latitude = 25.0421;
    redHouse.longitude = 121.5069;
    redHouse.mapX = 40.0;
    redHouse.mapY = -180.0;
    redHouse.address = "Chengdu Road";
    redHouse.city = "Taipei";
    redHouse.district = "Wanhua";
    redHouse.startYear = 1908;
    redHouse.periodLabel = "Japanese Period";
    redHouse.sourceName = "Portfolio sample dataset";
    redHouse.preservationStatus = "Reused";
    redHouse.tags = "market; red brick; cultural venue";
    seedSites.append(redHouse);

    HeritageSite guestHouse;
    guestHouse.title = "Taipei Guest House";
    guestHouse.category = "National Historic Site";
    guestHouse.description = "A representative official building from the Japanese period, now used for state functions and heritage education.";
    guestHouse.latitude = 25.0399;
    guestHouse.longitude = 121.5151;
    guestHouse.mapX = 210.0;
    guestHouse.mapY = -40.0;
    guestHouse.address = "Ketagalan Boulevard";
    guestHouse.city = "Taipei";
    guestHouse.district = "Zhongzheng";
    guestHouse.startYear = 1901;
    guestHouse.periodLabel = "Japanese Period";
    guestHouse.sourceName = "Portfolio sample dataset";
    guestHouse.preservationStatus = "Preserved";
    guestHouse.tags = "official building; national historic site";
    seedSites.append(guestHouse);

    for (HeritageSite &site : seedSites) {
        if (!insert(site, errorMessage)) {
            return false;
        }
    }

    return true;
}

HeritageSite HeritageSiteRepository::siteFromQuery(const QSqlQuery &query) const
{
    HeritageSite site;

    // 欄位順序必須和 kHeritageSiteColumns 保持一致。
    site.id = query.value(0).toInt();
    site.title = query.value(1).toString();
    site.category = query.value(2).toString();
    site.description = query.value(3).toString();
    site.latitude = query.value(4).toDouble();
    site.longitude = query.value(5).toDouble();
    site.mapX = query.value(6).toDouble();
    site.mapY = query.value(7).toDouble();
    site.address = query.value(8).toString();
    site.city = query.value(9).toString();
    site.district = query.value(10).toString();
    site.startYear = query.value(11).toInt();
    site.endYear = query.value(12).toInt();
    site.periodLabel = query.value(13).toString();
    site.sourceName = query.value(14).toString();
    site.sourceUrl = query.value(15).toString();
    site.preservationStatus = query.value(16).toString();
    site.tags = query.value(17).toString();
    site.createdAt = query.value(18).toString();
    site.updatedAt = query.value(19).toString();

    return site;
}