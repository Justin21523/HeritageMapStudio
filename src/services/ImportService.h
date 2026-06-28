#pragma once

#include "models/HeritageSite.h"

#include <QString>
#include <QStringList>
#include <QVector>

class HeritageSiteRepository;

// ImportResult 用來回報匯入結果。
// UI 可以拿它顯示匯入幾筆、跳過幾筆、有什麼警告。
struct ImportResult {
    int parsedRows = 0;
    int insertedRows = 0;
    int skippedRows = 0;
    QStringList warnings;
};

// ImportService 是 CSV → HeritageSite → SQLite 的中介層。
// 重點：Dialog 不直接碰 Repository，Repository 不負責 parse CSV。
class ImportService final
{
public:
    explicit ImportService(HeritageSiteRepository &repository);

    // 從 CSV 匯入文化地點資料。
    bool importHeritageSitesFromCsv(const QString &filePath,
                                    ImportResult *result,
                                    QString *errorMessage = nullptr) const;

    // 給 ImportCsvDialog 做預先檢查用。
    static bool validateRequiredHeaders(const QStringList &headers,
                                        QStringList *missingHeaders = nullptr);

private:
    // 將 CSV 一列轉成 HeritageSite。
    bool mapRowToHeritageSite(const QStringList &headers,
                              const QStringList &row,
                              int csvLineNumber,
                              HeritageSite *site,
                              QStringList *warnings) const;

    // Header 工具：讓 CSV 欄位大小寫、底線、空白比較有容錯。
    static QString normalizeHeader(const QString &header);
    static int findHeaderIndex(const QStringList &headers, const QStringList &aliases);
    static QString valueByAliases(const QStringList &headers,
                                  const QStringList &row,
                                  const QStringList &aliases);

    // 型別轉換工具：失敗時回報 warning，不讓 UI 偷偷吞錯。
    static bool parseRequiredDouble(const QString &text,
                                    const QString &fieldName,
                                    int csvLineNumber,
                                    double *value,
                                    QStringList *warnings);

    static int parseOptionalInt(const QString &text,
                                const QString &fieldName,
                                int csvLineNumber,
                                QStringList *warnings);

    static double parseOptionalDouble(const QString &text,
                                      const QString &fieldName,
                                      int csvLineNumber,
                                      QStringList *warnings);

private:
    HeritageSiteRepository &m_repository;
};