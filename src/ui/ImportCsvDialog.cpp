#include "ui/ImportCsvDialog.h"

#include "services/ImportService.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>

ImportCsvDialog::ImportCsvDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Import Heritage CSV");
    setMinimumSize(920, 620);

    createLayout();
}

QString ImportCsvDialog::selectedFilePath() const
{
    return m_selectedFilePath;
}

void ImportCsvDialog::createLayout()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *titleLabel = new QLabel("Import Heritage Sites from CSV", this);
    titleLabel->setObjectName("DialogTitleLabel");

    QLabel *hintLabel = new QLabel(
        "Required columns: title, category, and either latitude+longitude or map_x+map_y. "
        "Optional columns include description, address, city, district, start_year, period_label, source_name, tags.",
        this);
    hintLabel->setWordWrap(true);

    QHBoxLayout *fileLayout = new QHBoxLayout();

    m_filePathInput = new QLineEdit(this);
    m_filePathInput->setReadOnly(true);
    m_filePathInput->setPlaceholderText("Choose a CSV file...");

    m_browseButton = new QPushButton("Browse...", this);
    connect(m_browseButton, &QPushButton::clicked,
            this, &ImportCsvDialog::browseCsvFile);

    fileLayout->addWidget(m_filePathInput, 1);
    fileLayout->addWidget(m_browseButton);

    m_statusLabel = new QLabel("No CSV selected.", this);
    m_statusLabel->setWordWrap(true);

    m_previewTable = new QTableWidget(this);
    m_previewTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_previewTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_previewTable->horizontalHeader()->setStretchLastSection(true);
    m_previewTable->verticalHeader()->setVisible(false);

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
    m_importButton = m_buttonBox->button(QDialogButtonBox::Ok);

    // 把 OK 改成 Import，語意比較清楚。
    m_importButton->setText("Import");
    m_importButton->setEnabled(false);

    connect(m_buttonBox, &QDialogButtonBox::accepted,
            this, &ImportCsvDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected,
            this, &ImportCsvDialog::reject);

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(hintLabel);
    mainLayout->addLayout(fileLayout);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_previewTable, 1);
    mainLayout->addWidget(m_buttonBox);
}

void ImportCsvDialog::browseCsvFile()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select Heritage CSV",
        QString(),
        "CSV Files (*.csv);;All Files (*)");

    if (filePath.isEmpty()) {
        return;
    }

    loadPreview(filePath);
}

void ImportCsvDialog::loadPreview(const QString &filePath)
{
    CsvReader reader;
    CsvTable table;
    QString error;

    if (!reader.readFile(filePath, &table, &error)) {
        m_selectedFilePath.clear();
        m_filePathInput->setText(filePath);
        m_importButton->setEnabled(false);
        m_previewTable->clear();
        m_previewTable->setRowCount(0);
        m_previewTable->setColumnCount(0);
        setStatusMessage(QString("Failed to read CSV: %1").arg(error), true);
        return;
    }

    QStringList missingHeaders;
    if (!ImportService::validateRequiredHeaders(table.headers, &missingHeaders)) {
        m_selectedFilePath.clear();
        m_filePathInput->setText(filePath);
        m_importButton->setEnabled(false);
        updatePreviewTable(table);
        setStatusMessage(QString("CSV loaded, but missing required columns: %1")
                             .arg(missingHeaders.join(", ")),
                         true);
        return;
    }

    m_selectedFilePath = filePath;
    m_filePathInput->setText(filePath);
    m_importButton->setEnabled(true);
    updatePreviewTable(table);

    setStatusMessage(QString("CSV ready. Rows detected: %1. Preview shows first 20 rows.")
                         .arg(table.rows.size()),
                     false);
}

void ImportCsvDialog::updatePreviewTable(const CsvTable &table)
{
    m_previewTable->clear();
    m_previewTable->setColumnCount(table.headers.size());
    m_previewTable->setHorizontalHeaderLabels(table.headers);

    // Preview 只顯示前 20 筆，避免超大 CSV 讓 dialog 卡住。
    const int previewRows = qMin(20, static_cast<int>(table.rows.size()));
    m_previewTable->setRowCount(previewRows);

    for (int rowIndex = 0; rowIndex < previewRows; ++rowIndex) {
        const QStringList &row = table.rows[rowIndex];

        for (int colIndex = 0; colIndex < table.headers.size(); ++colIndex) {
            const QString value = colIndex < row.size() ? row[colIndex] : QString();
            QTableWidgetItem *item = new QTableWidgetItem(value);
            m_previewTable->setItem(rowIndex, colIndex, item);
        }
    }

    m_previewTable->resizeColumnsToContents();
}

void ImportCsvDialog::setStatusMessage(const QString &message, bool isError)
{
    m_statusLabel->setText(message);

    // 用簡單 stylesheet 區分成功/錯誤狀態。
    // 後面 Phase 13 會整理成統一 QSS。
    if (isError) {
        m_statusLabel->setStyleSheet("color: #ff8a8a;");
    } else {
        m_statusLabel->setStyleSheet("color: #9ee6a8;");
    }
}