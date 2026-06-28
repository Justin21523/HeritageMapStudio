#pragma once

#include "models/HeritageSite.h"

#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

// MetadataEditorPanel 是右側 metadata 編輯面板。
// 重點：這個 widget 只負責顯示、編輯、驗證表單，不直接操作 SQLite。
class MetadataEditorPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit MetadataEditorPanel(QWidget *parent = nullptr);

    // 顯示目前選取的文化地點。
    // localX/localY 是 Phase 4 投影後的本地 meter coordinate，只顯示，不直接存回 DB。
    void setSite(const HeritageSite &site,
                 double localX,
                 double localY,
                 const QString &projectionSummary);

    // 沒有選取資料時清空面板。
    void clearSite();

    int currentSiteId() const;

signals:
    // 按下 Save Changes 並通過驗證後，交給 MainWindow 呼叫 repository 更新 SQLite。
    void saveRequested(const HeritageSite &site);

private slots:
    void enterEditMode();
    void saveChanges();
    void cancelEdit();
    void markDirty();

private:
    void createLayout();
    void connectDirtySignals();

    // readOnly = true 時是檢視模式；false 時是編輯模式。
    void setEditorReadOnly(bool readOnly);

    // 將 HeritageSite 寫入 UI 欄位。
    void fillFields(const HeritageSite &site,
                    double localX,
                    double localY,
                    const QString &projectionSummary);

    // 從 UI 欄位組回 HeritageSite，並做表單驗證。
    bool buildEditedSite(HeritageSite *site,
                         QStringList *validationErrors) const;

    void setStatusMessage(const QString &message, bool isError);

    static QString doubleToTextOrEmpty(double value, int precision = 6);
    static QString intToTextOrEmpty(int value);

    static bool parseOptionalDouble(const QString &text,
                                    const QString &fieldName,
                                    double minValue,
                                    double maxValue,
                                    double *value,
                                    bool *hasValue,
                                    QStringList *validationErrors);

    static bool parseOptionalInt(const QString &text,
                                 const QString &fieldName,
                                 int minValue,
                                 int maxValue,
                                 int *value,
                                 bool *hasValue,
                                 QStringList *validationErrors);

private:
    bool m_hasSite = false;
    bool m_isEditing = false;
    bool m_isDirty = false;
    bool m_isPopulating = false;

    HeritageSite m_currentSite;
    double m_currentLocalX = 0.0;
    double m_currentLocalY = 0.0;
    QString m_currentProjectionSummary;

    QLabel *m_headerLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_projectionLabel = nullptr;

    QLineEdit *m_titleEdit = nullptr;
    QLineEdit *m_categoryEdit = nullptr;
    QLineEdit *m_latitudeEdit = nullptr;
    QLineEdit *m_longitudeEdit = nullptr;
    QLineEdit *m_localCoordinateEdit = nullptr;

    QLineEdit *m_addressEdit = nullptr;
    QLineEdit *m_cityEdit = nullptr;
    QLineEdit *m_districtEdit = nullptr;

    QLineEdit *m_startYearEdit = nullptr;
    QLineEdit *m_endYearEdit = nullptr;
    QLineEdit *m_periodLabelEdit = nullptr;

    QLineEdit *m_preservationStatusEdit = nullptr;
    QLineEdit *m_tagsEdit = nullptr;
    QLineEdit *m_sourceNameEdit = nullptr;
    QLineEdit *m_sourceUrlEdit = nullptr;

    QTextEdit *m_descriptionEdit = nullptr;

    QPushButton *m_editButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
};