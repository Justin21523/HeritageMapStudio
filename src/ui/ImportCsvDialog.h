#pragma once

#include "services/CsvReader.h"

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QDialogButtonBox;

// ImportCsvDialog 負責 CSV 檔案選擇與 preview。
// 重點：它不寫 database，只讓使用者確認「我要匯入哪個檔案」。
class ImportCsvDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ImportCsvDialog(QWidget *parent = nullptr);

    // MainWindow 會在 dialog accepted 後拿這個路徑交給 ImportService。
    QString selectedFilePath() const;

private slots:
    void browseCsvFile();

private:
    void createLayout();
    void loadPreview(const QString &filePath);
    void updatePreviewTable(const CsvTable &table);
    void setStatusMessage(const QString &message, bool isError);

private:
    QLineEdit *m_filePathInput = nullptr;
    QPushButton *m_browseButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_previewTable = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    QPushButton *m_importButton = nullptr;

    QString m_selectedFilePath;
};