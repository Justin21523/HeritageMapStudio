#include "database/DatabaseManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>

namespace {
const char *kConnectionName = "HeritageMapStudioConnection";
const char *kDatabaseFileName = "heritage_map.sqlite";
}

DatabaseManager::DatabaseManager()
    : m_connectionName(kConnectionName)
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open(const QString &databasePath)
{
    m_databasePath = databasePath;

    // 確保 database 所在資料夾存在。
    const QFileInfo fileInfo(databasePath);
    QDir dir;
    if (!dir.mkpath(fileInfo.absolutePath())) {
        m_lastError = QString("Failed to create database directory: %1")
                          .arg(fileInfo.absolutePath());
        return false;
    }

    // Qt 的 connection name 是全域管理的；重複建立前先檢查是否已存在。
    if (QSqlDatabase::contains(m_connectionName)) {
        m_database = QSqlDatabase::database(m_connectionName);
    } else {
        m_database = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    }

    // SQLite 使用單一檔案作為資料庫。
    m_database.setDatabaseName(databasePath);

    if (!m_database.open()) {
        m_lastError = m_database.lastError().text();
        return false;
    }

    m_lastError.clear();
    return true;
}

void DatabaseManager::close()
{
    if (m_database.isValid() && m_database.isOpen()) {
        m_database.close();
    }
}

QSqlDatabase DatabaseManager::database() const
{
    return m_database;
}

QString DatabaseManager::databasePath() const
{
    return m_databasePath;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

QString DatabaseManager::defaultDatabasePath()
{
    // 讓 database 跟著執行檔放在 build/data，學習時最容易找到。
    const QString dataDir = QDir(QCoreApplication::applicationDirPath()).filePath("data");
    return QDir(dataDir).filePath(kDatabaseFileName);
}