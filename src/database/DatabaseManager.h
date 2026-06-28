#pragma once

#include <QSqlDatabase>
#include <QString>

// DatabaseManager 專門管理 SQLite connection。
// 重點：MainWindow 不直接呼叫 QSqlDatabase::addDatabase，避免 UI 層混入連線細節。
class DatabaseManager final
{
public:
    DatabaseManager();
    ~DatabaseManager();

    // 開啟 SQLite database；若檔案不存在，SQLite 會自動建立。
    bool open(const QString &databasePath);

    // 關閉 connection；目前不移除 connection，避免 Qt handle 生命週期踩雷。
    void close();

    // Repository 會透過這個函式取得 database handle。
    QSqlDatabase database() const;

    QString databasePath() const;
    QString lastError() const;

    // 統一規劃預設 database 放置位置。
    static QString defaultDatabasePath();

private:
    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
    QSqlDatabase m_database;
};