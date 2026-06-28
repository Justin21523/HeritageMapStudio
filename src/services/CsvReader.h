#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// CsvTable 是讀取 CSV 後的中介資料格式。
// 重點：CsvReader 只負責讀 CSV，不負責轉成 HeritageSite，也不碰 database。
struct CsvTable {
    QStringList headers;
    QVector<QStringList> rows;
};

// CsvReader 是低階 CSV parser。
// 支援：逗號分隔、雙引號欄位、雙引號跳脫、UTF-8 文字。
// 不支援：Excel 複雜公式、跨行 quoted field。那些後面有需要再補。
class CsvReader final
{
public:
    CsvReader() = default;

    // 讀取整份 CSV。
    // 成功時回傳 true，失敗時 errorMessage 會有可顯示的錯誤訊息。
    bool readFile(const QString &filePath,
                  CsvTable *table,
                  QString *errorMessage = nullptr) const;

private:
    // 解析單行 CSV。
    // 中文註解重點：這裡處理 "a,b", "a""b" 這種基本 CSV 規則。
    bool parseLine(const QString &line,
                   QStringList *fields,
                   QString *errorMessage = nullptr) const;

    // 判斷一列是不是完全空白。
    bool isEmptyRow(const QStringList &fields) const;
};