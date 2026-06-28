#include "database/MigrationManager.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

MigrationManager::MigrationManager(QSqlDatabase database)
    : m_database(database)
{
}

bool MigrationManager::migrate(QString *errorMessage)
{
    if (!m_database.isValid() || !m_database.isOpen()) {
        if (errorMessage) {
            *errorMessage = "Database is not open.";
        }
        return false;
    }

    if (!ensureMigrationTable(errorMessage)) {
        return false;
    }

    const int version = currentVersion(errorMessage);
    if (version < 0) {
        return false;
    }

    // Phase 2 目前只有 v1 schema；之後新增表格就依序加 v2/v3。
    if (version < 1) {
        if (!m_database.transaction()) {
            if (errorMessage) {
                *errorMessage = m_database.lastError().text();
            }
            return false;
        }

        if (!applyVersion1(errorMessage)) {
            m_database.rollback();
            return false;
        }

        if (!m_database.commit()) {
            if (errorMessage) {
                *errorMessage = m_database.lastError().text();
            }
            return false;
        }
    }

    return true;
}

bool MigrationManager::ensureMigrationTable(QString *errorMessage)
{
    return execStatement(
        "CREATE TABLE IF NOT EXISTS schema_migrations ("
        "version INTEGER PRIMARY KEY, "
        "applied_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ")",
        errorMessage);
}

int MigrationManager::currentVersion(QString *errorMessage) const
{
    QSqlQuery query(m_database);

    // MAX(version) 取得目前最新 schema 版本；沒有資料時回傳 0。
    if (!query.exec("SELECT COALESCE(MAX(version), 0) FROM schema_migrations")) {
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

bool MigrationManager::applyVersion1(QString *errorMessage)
{
    // SQLite 一次 exec 一個 statement，這裡故意拆開，錯誤也比較好追。
    const QString createHeritageSitesSql =
        "CREATE TABLE IF NOT EXISTS heritage_sites ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "title TEXT NOT NULL, "
        "category TEXT NOT NULL, "
        "description TEXT, "
        "latitude REAL NOT NULL DEFAULT 0, "
        "longitude REAL NOT NULL DEFAULT 0, "
        "map_x REAL NOT NULL DEFAULT 0, "
        "map_y REAL NOT NULL DEFAULT 0, "
        "address TEXT, "
        "city TEXT, "
        "district TEXT, "
        "start_year INTEGER DEFAULT 0, "
        "end_year INTEGER DEFAULT 0, "
        "period_label TEXT, "
        "source_name TEXT, "
        "source_url TEXT, "
        "preservation_status TEXT, "
        "tags TEXT, "
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP, "
        "updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ")";

    if (!execStatement(createHeritageSitesSql, errorMessage)) {
        return false;
    }

    if (!execStatement("CREATE INDEX IF NOT EXISTS idx_heritage_sites_title ON heritage_sites(title)", errorMessage)) {
        return false;
    }

    if (!execStatement("CREATE INDEX IF NOT EXISTS idx_heritage_sites_category ON heritage_sites(category)", errorMessage)) {
        return false;
    }

    if (!execStatement("CREATE INDEX IF NOT EXISTS idx_heritage_sites_period ON heritage_sites(start_year, end_year)", errorMessage)) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare("INSERT INTO schema_migrations(version) VALUES(:version)");
    query.bindValue(":version", 1);

    if (!query.exec()) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    return true;
}

bool MigrationManager::execStatement(const QString &sql, QString *errorMessage) const
{
    QSqlQuery query(m_database);

    if (!query.exec(sql)) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    return true;
}