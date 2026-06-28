#pragma once

#include "models/HeritageSite.h"
#include "models/SearchFilterState.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// SearchPanel 是底部搜尋與結果面板。
// 重點：它只負責搜尋 UI、facet UI、結果表格；不直接操作 SQLite，也不直接碰 MapCanvas。
class SearchPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPanel(QWidget *parent = nullptr);

    // 匯入或更新資料後，更新 facet 下拉選單來源。
    void setAvailableSites(const QVector<HeritageSite> &sites);

    // MainWindow 計算完 Layer + Timeline + Search 後，把結果丟回面板顯示。
    void setResults(const QVector<HeritageSite> &results, int totalSiteCount);

    SearchFilterState currentFilter() const;

signals:
    void searchFilterChanged(const SearchFilterState &state);
    void siteActivated(int siteId);

private slots:
    void emitFilterChanged();
    void clearFilters();
    void handleResultDoubleClicked(int row, int column);

private:
    void createLayout();
    void populateFacetCombos();

    QString currentComboValue(const QComboBox *comboBox) const;

    void populateFacetCombo(QComboBox *comboBox,
                            const QString &allLabel,
                            const QStringList &values,
                            const QString &previousValue);

    QStringList uniqueNonEmptyValues(const QString &fieldName) const;
    QStringList uniqueTags() const;

    static QString yearRangeLabel(const HeritageSite &site);

private:
    bool m_isUpdating = false;

    QVector<HeritageSite> m_availableSites;

    QLineEdit *m_keywordInput = nullptr;

    QComboBox *m_categoryComboBox = nullptr;
    QComboBox *m_districtComboBox = nullptr;
    QComboBox *m_periodComboBox = nullptr;
    QComboBox *m_statusComboBox = nullptr;
    QComboBox *m_sourceComboBox = nullptr;
    QComboBox *m_tagComboBox = nullptr;

    QCheckBox *m_searchDescriptionCheckBox = nullptr;
    QPushButton *m_clearFiltersButton = nullptr;

    QLabel *m_resultCountLabel = nullptr;
    QTableWidget *m_resultTable = nullptr;
};