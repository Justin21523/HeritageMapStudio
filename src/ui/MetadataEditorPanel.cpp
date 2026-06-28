#include "ui/MetadataEditorPanel.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>

#include <cmath>

namespace {
constexpr double kCoordinateEpsilon = 0.000000001;
constexpr double kMinLatitude = -85.05112878;
constexpr double kMaxLatitude = 85.05112878;
constexpr double kMinLongitude = -180.0;
constexpr double kMaxLongitude = 180.0;
}

MetadataEditorPanel::MetadataEditorPanel(QWidget *parent)
    : QWidget(parent)
{
    createLayout();
    connectDirtySignals();
    clearSite();
}

void MetadataEditorPanel::setSite(const HeritageSite &site,
                                  double localX,
                                  double localY,
                                  const QString &projectionSummary)
{
    m_currentSite = site;
    m_currentLocalX = localX;
    m_currentLocalY = localY;
    m_currentProjectionSummary = projectionSummary;

    m_hasSite = true;
    m_isEditing = false;
    m_isDirty = false;

    fillFields(site, localX, localY, projectionSummary);
    setEditorReadOnly(true);

    setStatusMessage("Ready. Click Edit to modify metadata.", false);
}

void MetadataEditorPanel::clearSite()
{
    m_hasSite = false;
    m_isEditing = false;
    m_isDirty = false;

    HeritageSite emptySite;
    fillFields(emptySite, 0.0, 0.0, "No projection available.");

    m_headerLabel->setText("No Site Selected");
    setEditorReadOnly(true);

    m_editButton->setEnabled(false);
    m_saveButton->setEnabled(false);
    m_cancelButton->setEnabled(false);

    setStatusMessage("Select a heritage point on the map or double-click a row in Search Results.", false);
}

int MetadataEditorPanel::currentSiteId() const
{
    return m_hasSite ? m_currentSite.id : -1;
}

void MetadataEditorPanel::createLayout()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    m_headerLabel = new QLabel("No Site Selected", this);
    m_headerLabel->setObjectName("MetadataEditorHeaderLabel");

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);

    m_projectionLabel = new QLabel(this);
    m_projectionLabel->setWordWrap(true);
    m_projectionLabel->setObjectName("ProjectionSummaryLabel");

    // 使用 ScrollArea，避免欄位變多時右側 panel 塞爆。
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    QWidget *formContainer = new QWidget(scrollArea);
    QFormLayout *formLayout = new QFormLayout(formContainer);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_titleEdit = new QLineEdit(formContainer);
    m_categoryEdit = new QLineEdit(formContainer);
    m_latitudeEdit = new QLineEdit(formContainer);
    m_longitudeEdit = new QLineEdit(formContainer);
    m_localCoordinateEdit = new QLineEdit(formContainer);

    m_addressEdit = new QLineEdit(formContainer);
    m_cityEdit = new QLineEdit(formContainer);
    m_districtEdit = new QLineEdit(formContainer);

    m_startYearEdit = new QLineEdit(formContainer);
    m_endYearEdit = new QLineEdit(formContainer);
    m_periodLabelEdit = new QLineEdit(formContainer);

    m_preservationStatusEdit = new QLineEdit(formContainer);
    m_tagsEdit = new QLineEdit(formContainer);
    m_sourceNameEdit = new QLineEdit(formContainer);
    m_sourceUrlEdit = new QLineEdit(formContainer);

    m_descriptionEdit = new QTextEdit(formContainer);
    m_descriptionEdit->setMinimumHeight(180);

    // Local coordinate 是投影結果，不直接讓使用者手動編輯。
    m_localCoordinateEdit->setReadOnly(true);

    m_titleEdit->setPlaceholderText("Required");
    m_categoryEdit->setPlaceholderText("Required");
    m_latitudeEdit->setPlaceholderText("Example: 25.047800");
    m_longitudeEdit->setPlaceholderText("Example: 121.511900");
    m_startYearEdit->setPlaceholderText("Example: 1884");
    m_endYearEdit->setPlaceholderText("Optional");
    m_tagsEdit->setPlaceholderText("tag1; tag2; tag3");
    m_descriptionEdit->setPlaceholderText("Describe the historical context, preservation status, and metadata notes.");

    formLayout->addRow("Title", m_titleEdit);
    formLayout->addRow("Category", m_categoryEdit);
    formLayout->addRow("Latitude", m_latitudeEdit);
    formLayout->addRow("Longitude", m_longitudeEdit);
    formLayout->addRow("Local Coordinate", m_localCoordinateEdit);
    formLayout->addRow("Address", m_addressEdit);
    formLayout->addRow("City", m_cityEdit);
    formLayout->addRow("District", m_districtEdit);
    formLayout->addRow("Start Year", m_startYearEdit);
    formLayout->addRow("End Year", m_endYearEdit);
    formLayout->addRow("Period", m_periodLabelEdit);
    formLayout->addRow("Preservation Status", m_preservationStatusEdit);
    formLayout->addRow("Tags", m_tagsEdit);
    formLayout->addRow("Source Name", m_sourceNameEdit);
    formLayout->addRow("Source URL", m_sourceUrlEdit);
    formLayout->addRow("Description", m_descriptionEdit);

    scrollArea->setWidget(formContainer);

    QHBoxLayout *buttonLayout = new QHBoxLayout();

    m_editButton = new QPushButton("Edit", this);
    m_saveButton = new QPushButton("Save Changes", this);
    m_cancelButton = new QPushButton("Cancel", this);

    connect(m_editButton, &QPushButton::clicked,
            this, &MetadataEditorPanel::enterEditMode);
    connect(m_saveButton, &QPushButton::clicked,
            this, &MetadataEditorPanel::saveChanges);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &MetadataEditorPanel::cancelEdit);

    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_cancelButton);

    mainLayout->addWidget(m_headerLabel);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_projectionLabel);
    mainLayout->addWidget(scrollArea, 1);
    mainLayout->addLayout(buttonLayout);
}

void MetadataEditorPanel::connectDirtySignals()
{
    const auto connectLineEditDirty = [this](QLineEdit *lineEdit) {
        connect(lineEdit, &QLineEdit::textChanged, this, [this]() {
            markDirty();
        });
    };

    connectLineEditDirty(m_titleEdit);
    connectLineEditDirty(m_categoryEdit);
    connectLineEditDirty(m_latitudeEdit);
    connectLineEditDirty(m_longitudeEdit);
    connectLineEditDirty(m_addressEdit);
    connectLineEditDirty(m_cityEdit);
    connectLineEditDirty(m_districtEdit);
    connectLineEditDirty(m_startYearEdit);
    connectLineEditDirty(m_endYearEdit);
    connectLineEditDirty(m_periodLabelEdit);
    connectLineEditDirty(m_preservationStatusEdit);
    connectLineEditDirty(m_tagsEdit);
    connectLineEditDirty(m_sourceNameEdit);
    connectLineEditDirty(m_sourceUrlEdit);

    connect(m_descriptionEdit, &QTextEdit::textChanged,
            this, &MetadataEditorPanel::markDirty);
}

void MetadataEditorPanel::setEditorReadOnly(bool readOnly)
{
    // 表單欄位 read-only 控制。
    // Local Coordinate 永遠唯讀，因為它是投影計算結果。
    m_titleEdit->setReadOnly(readOnly);
    m_categoryEdit->setReadOnly(readOnly);
    m_latitudeEdit->setReadOnly(readOnly);
    m_longitudeEdit->setReadOnly(readOnly);
    m_localCoordinateEdit->setReadOnly(true);

    m_addressEdit->setReadOnly(readOnly);
    m_cityEdit->setReadOnly(readOnly);
    m_districtEdit->setReadOnly(readOnly);

    m_startYearEdit->setReadOnly(readOnly);
    m_endYearEdit->setReadOnly(readOnly);
    m_periodLabelEdit->setReadOnly(readOnly);

    m_preservationStatusEdit->setReadOnly(readOnly);
    m_tagsEdit->setReadOnly(readOnly);
    m_sourceNameEdit->setReadOnly(readOnly);
    m_sourceUrlEdit->setReadOnly(readOnly);

    m_descriptionEdit->setReadOnly(readOnly);

    m_editButton->setEnabled(m_hasSite && readOnly);
    m_saveButton->setEnabled(m_hasSite && !readOnly && m_isDirty);
    m_cancelButton->setEnabled(m_hasSite && !readOnly);
}

void MetadataEditorPanel::fillFields(const HeritageSite &site,
                                     double localX,
                                     double localY,
                                     const QString &projectionSummary)
{
    m_isPopulating = true;

    m_headerLabel->setText(site.id >= 0 ? site.title : "No Site Selected");

    m_titleEdit->setText(site.title);
    m_categoryEdit->setText(site.category);

    m_latitudeEdit->setText(doubleToTextOrEmpty(site.latitude, 6));
    m_longitudeEdit->setText(doubleToTextOrEmpty(site.longitude, 6));
    m_localCoordinateEdit->setText(QString("X %1 m, Y %2 m")
                                       .arg(localX, 0, 'f', 1)
                                       .arg(localY, 0, 'f', 1));

    m_addressEdit->setText(site.address);
    m_cityEdit->setText(site.city);
    m_districtEdit->setText(site.district);

    m_startYearEdit->setText(intToTextOrEmpty(site.startYear));
    m_endYearEdit->setText(intToTextOrEmpty(site.endYear));
    m_periodLabelEdit->setText(site.periodLabel);

    m_preservationStatusEdit->setText(site.preservationStatus);
    m_tagsEdit->setText(site.tags);
    m_sourceNameEdit->setText(site.sourceName);
    m_sourceUrlEdit->setText(site.sourceUrl);

    m_descriptionEdit->setPlainText(site.description);

    m_projectionLabel->setText(projectionSummary);

    m_isPopulating = false;
}

bool MetadataEditorPanel::buildEditedSite(HeritageSite *site,
                                          QStringList *validationErrors) const
{
    if (!site) {
        if (validationErrors) {
            validationErrors->append("Internal error: edited site output is null.");
        }
        return false;
    }

    if (validationErrors) {
        validationErrors->clear();
    }

    HeritageSite editedSite = m_currentSite;

    editedSite.title = m_titleEdit->text().trimmed();
    editedSite.category = m_categoryEdit->text().trimmed();

    if (editedSite.title.isEmpty()) {
        validationErrors->append("Title is required.");
    }

    if (editedSite.category.isEmpty()) {
        validationErrors->append("Category is required.");
    }

    double latitude = 0.0;
    double longitude = 0.0;
    bool hasLatitude = false;
    bool hasLongitude = false;

    const bool latitudeOk = parseOptionalDouble(m_latitudeEdit->text(),
                                                "Latitude",
                                                kMinLatitude,
                                                kMaxLatitude,
                                                &latitude,
                                                &hasLatitude,
                                                validationErrors);

    const bool longitudeOk = parseOptionalDouble(m_longitudeEdit->text(),
                                                 "Longitude",
                                                 kMinLongitude,
                                                 kMaxLongitude,
                                                 &longitude,
                                                 &hasLongitude,
                                                 validationErrors);

    if (latitudeOk && longitudeOk) {
        if (hasLatitude != hasLongitude) {
            validationErrors->append("Latitude and Longitude must be provided together.");
        } else if (hasLatitude && hasLongitude) {
            editedSite.latitude = latitude;
            editedSite.longitude = longitude;
        } else {
            // 清空經緯度時，保留 map_x/map_y fallback。
            editedSite.latitude = 0.0;
            editedSite.longitude = 0.0;
        }
    }

    int startYear = 0;
    int endYear = 0;
    bool hasStartYear = false;
    bool hasEndYear = false;

    parseOptionalInt(m_startYearEdit->text(),
                     "Start Year",
                     -9999,
                     9999,
                     &startYear,
                     &hasStartYear,
                     validationErrors);

    parseOptionalInt(m_endYearEdit->text(),
                     "End Year",
                     -9999,
                     9999,
                     &endYear,
                     &hasEndYear,
                     validationErrors);

    if (hasStartYear) {
        editedSite.startYear = startYear;
    } else {
        editedSite.startYear = 0;
    }

    if (hasEndYear) {
        editedSite.endYear = endYear;
    } else {
        editedSite.endYear = 0;
    }

    if (hasStartYear && hasEndYear && endYear < startYear) {
        validationErrors->append("End Year cannot be earlier than Start Year.");
    }

    editedSite.address = m_addressEdit->text().trimmed();
    editedSite.city = m_cityEdit->text().trimmed();
    editedSite.district = m_districtEdit->text().trimmed();
    editedSite.periodLabel = m_periodLabelEdit->text().trimmed();
    editedSite.preservationStatus = m_preservationStatusEdit->text().trimmed();
    editedSite.tags = m_tagsEdit->text().trimmed();
    editedSite.sourceName = m_sourceNameEdit->text().trimmed();
    editedSite.sourceUrl = m_sourceUrlEdit->text().trimmed();
    editedSite.description = m_descriptionEdit->toPlainText().trimmed();

    if (validationErrors && !validationErrors->isEmpty()) {
        return false;
    }

    *site = editedSite;
    return true;
}

void MetadataEditorPanel::enterEditMode()
{
    if (!m_hasSite) {
        return;
    }

    m_isEditing = true;
    m_isDirty = false;

    setEditorReadOnly(false);
    setStatusMessage("Edit mode enabled. Modify fields and click Save Changes.", false);
}

void MetadataEditorPanel::saveChanges()
{
    if (!m_hasSite || !m_isEditing) {
        return;
    }

    if (!m_isDirty) {
        setStatusMessage("No changes to save.", false);
        return;
    }

    HeritageSite editedSite;
    QStringList validationErrors;

    if (!buildEditedSite(&editedSite, &validationErrors)) {
        setStatusMessage(QString("Validation failed:\n%1").arg(validationErrors.join("\n")), true);
        return;
    }

    emit saveRequested(editedSite);
}

void MetadataEditorPanel::cancelEdit()
{
    if (!m_hasSite) {
        return;
    }

    m_isEditing = false;
    m_isDirty = false;

    fillFields(m_currentSite, m_currentLocalX, m_currentLocalY, m_currentProjectionSummary);
    setEditorReadOnly(true);

    setStatusMessage("Edit canceled. Metadata restored.", false);
}

void MetadataEditorPanel::markDirty()
{
    if (m_isPopulating || !m_hasSite || !m_isEditing) {
        return;
    }

    m_isDirty = true;
    m_saveButton->setEnabled(true);

    setStatusMessage("Unsaved changes.", false);
}

void MetadataEditorPanel::setStatusMessage(const QString &message, bool isError)
{
    m_statusLabel->setText(message);

    if (isError) {
        m_statusLabel->setStyleSheet("color: #ff8a8a;");
    } else {
        m_statusLabel->setStyleSheet("color: #9ee6a8;");
    }
}

QString MetadataEditorPanel::doubleToTextOrEmpty(double value, int precision)
{
    // 目前資料庫用 0 表示未填座標。
    // 如果未來要支援赤道/本初子午線資料，會改成 nullable 欄位或 has_coordinate flag。
    if (std::abs(value) < kCoordinateEpsilon) {
        return {};
    }

    return QString::number(value, 'f', precision);
}

QString MetadataEditorPanel::intToTextOrEmpty(int value)
{
    if (value == 0) {
        return {};
    }

    return QString::number(value);
}

bool MetadataEditorPanel::parseOptionalDouble(const QString &text,
                                              const QString &fieldName,
                                              double minValue,
                                              double maxValue,
                                              double *value,
                                              bool *hasValue,
                                              QStringList *validationErrors)
{
    if (value) {
        *value = 0.0;
    }

    if (hasValue) {
        *hasValue = false;
    }

    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty()) {
        return true;
    }

    bool ok = false;
    const double parsedValue = trimmed.toDouble(&ok);

    if (!ok) {
        if (validationErrors) {
            validationErrors->append(QString("%1 must be a valid number.").arg(fieldName));
        }
        return false;
    }

    if (parsedValue < minValue || parsedValue > maxValue) {
        if (validationErrors) {
            validationErrors->append(QString("%1 must be between %2 and %3.")
                                         .arg(fieldName)
                                         .arg(minValue)
                                         .arg(maxValue));
        }
        return false;
    }

    if (value) {
        *value = parsedValue;
    }

    if (hasValue) {
        *hasValue = true;
    }

    return true;
}

bool MetadataEditorPanel::parseOptionalInt(const QString &text,
                                           const QString &fieldName,
                                           int minValue,
                                           int maxValue,
                                           int *value,
                                           bool *hasValue,
                                           QStringList *validationErrors)
{
    if (value) {
        *value = 0;
    }

    if (hasValue) {
        *hasValue = false;
    }

    const QString trimmed = text.trimmed();

    if (trimmed.isEmpty()) {
        return true;
    }

    bool ok = false;
    const int parsedValue = trimmed.toInt(&ok);

    if (!ok) {
        if (validationErrors) {
            validationErrors->append(QString("%1 must be a valid integer.").arg(fieldName));
        }
        return false;
    }

    if (parsedValue < minValue || parsedValue > maxValue) {
        if (validationErrors) {
            validationErrors->append(QString("%1 must be between %2 and %3.")
                                         .arg(fieldName)
                                         .arg(minValue)
                                         .arg(maxValue));
        }
        return false;
    }

    if (value) {
        *value = parsedValue;
    }

    if (hasValue) {
        *hasValue = true;
    }

    return true;
}