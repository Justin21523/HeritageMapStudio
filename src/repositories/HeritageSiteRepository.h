#pragma once

#include "models/HeritageSite.h"

#include <QSqlDatabase>
#include <QVector>

#include <optional>

class QSqlQuery;

// HeritageSiteRepository 專門負責 heritage_sites 資料表。
// 重點：所有 SQL 都集中在這裡，UI 不直接碰 SQL。
class HeritageSiteRepository final
{
public:
    explicit HeritageSiteRepository(QSqlDatabase database);

    QVector<HeritageSite> findAll(QString *errorMessage = nullptr) const;
    std::optional<HeritageSite> findById(int id, QString *errorMessage = nullptr) const;

    bool insert(HeritageSite &site, QString *errorMessage = nullptr) const;

    // Phase 5：更新右側 Metadata Editor 修改後的文化地點。
    bool update(const HeritageSite &site, QString *errorMessage = nullptr) const;
    
    // Phase 3：CSV 匯入會一次新增多筆資料。
    // 使用 transaction，避免匯入到一半失敗造成資料半套。
    bool insertMany(QVector<HeritageSite> &sites, QString *errorMessage = nullptr) const;

    int count(QString *errorMessage = nullptr) const;
    
    // 如果 database 是空的，就放入作品集 demo data。
    bool ensureSeedData(QString *errorMessage = nullptr) const;

private:
    HeritageSite siteFromQuery(const QSqlQuery &query) const;

private:
    QSqlDatabase m_database;
};