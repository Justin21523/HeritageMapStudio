#include "services/CsvReader.h"

#include <QFile>
#include <QTextStream>
#include <QStringConverter>

bool CsvReader::readFile(const QString &filePath,
                         CsvTable *table,
                         QString *errorMessage) const
{
    if (!table) {
        if (errorMessage) {
            *errorMessage = "Internal error: CSV table output is null.";
        }
        return false;
    }

    table->headers.clear();
    table->rows.clear();

    QFile file(filePath);

    // QFile 負責開檔；QTextStream 負責用文字方式逐行讀取。
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = QString("Failed to open CSV file: %1").arg(file.errorString());
        }
        return false;
    }

    QTextStream stream(&file);

    // HeritageMapStudio 標準採 UTF-8，避免中文文化資料亂碼。
    stream.setEncoding(QStringConverter::Utf8);

    int lineNumber = 0;
    bool headerLoaded = false;

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        ++lineNumber;

        QStringList fields;
        QString parseError;

        if (!parseLine(line, &fields, &parseError)) {
            if (errorMessage) {
                *errorMessage = QString("CSV parse error at line %1: %2")
                                    .arg(lineNumber)
                                    .arg(parseError);
            }
            return false;
        }

        // 完全空白列直接略過，讓使用者的 CSV 比較有容錯空間。
        if (isEmptyRow(fields)) {
            continue;
        }

        if (!headerLoaded) {
            // 第一個非空白列視為 header。
            for (QString &header : fields) {
                header = header.trimmed();
            }

            table->headers = fields;
            headerLoaded = true;

            if (table->headers.isEmpty()) {
                if (errorMessage) {
                    *errorMessage = "CSV header row is empty.";
                }
                return false;
            }

            continue;
        }

        // 欄位數必須跟 header 一致。
        // 如果 description 裡有逗號，CSV 裡應該用雙引號包起來。
        if (fields.size() != table->headers.size()) {
            if (errorMessage) {
                *errorMessage = QString("Column count mismatch at line %1. Expected %2 columns, got %3.")
                                    .arg(lineNumber)
                                    .arg(table->headers.size())
                                    .arg(fields.size());
            }
            return false;
        }

        for (QString &field : fields) {
            field = field.trimmed();
        }

        table->rows.append(fields);
    }

    if (!headerLoaded) {
        if (errorMessage) {
            *errorMessage = "CSV file does not contain a header row.";
        }
        return false;
    }

    return true;
}

bool CsvReader::parseLine(const QString &line,
                          QStringList *fields,
                          QString *errorMessage) const
{
    if (!fields) {
        if (errorMessage) {
            *errorMessage = "Internal error: fields output is null.";
        }
        return false;
    }

    fields->clear();

    QString currentField;
    bool insideQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);

        if (insideQuotes) {
            if (ch == '"') {
                // 兩個連續雙引號代表欄位內容裡的一個雙引號。
                if (i + 1 < line.size() && line.at(i + 1) == '"') {
                    currentField.append('"');
                    ++i;
                } else {
                    insideQuotes = false;
                }
            } else {
                currentField.append(ch);
            }

            continue;
        }

        if (ch == ',') {
            fields->append(currentField);
            currentField.clear();
            continue;
        }

        if (ch == '"') {
            // 只有欄位開頭遇到雙引號時，才切入 quoted mode。
            // 如果使用者把雙引號放在欄位中間，這裡仍然盡量容錯。
            if (currentField.trimmed().isEmpty()) {
                currentField.clear();
                insideQuotes = true;
            } else {
                currentField.append(ch);
            }
            continue;
        }

        currentField.append(ch);
    }

    if (insideQuotes) {
        if (errorMessage) {
            *errorMessage = "Unclosed quoted field.";
        }
        return false;
    }

    fields->append(currentField);
    return true;
}

bool CsvReader::isEmptyRow(const QStringList &fields) const
{
    for (const QString &field : fields) {
        if (!field.trimmed().isEmpty()) {
            return false;
        }
    }

    return true;
}