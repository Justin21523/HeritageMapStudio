#pragma once

#include <QSqlDatabase>
#include <QString>

// MigrationManager 負責建立/升級資料庫 schema。
// 重點：schema 版本化，之後 Phase 3/4/5 加資料表時不用重建整個 database。
class MigrationManager final
{
public:
    explicit MigrationManager(QSqlDatabase database);

    // 執行所有尚未套用的 migration。
    bool migrate(QString *errorMessage = nullptr);

private:
    bool ensureMigrationTable(QString *errorMessage);
    int currentVersion(QString *errorMessage) const;
    bool applyVersion1(QString *errorMessage);
    bool execStatement(const QString &sql, QString *errorMessage) const;

private:
    QSqlDatabase m_database;
};