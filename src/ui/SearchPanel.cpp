#include "ui/SearchPanel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <QSet>

namespace {
constexpr int kSiteIdRole = Qt::UserRole + 1;
}

SearchPanel::SearchPanel(QWidget *parent)
    : QWidget(parent)
{
    createLayout();
}

void SearchPanel::setAvailableSites(const QVector<HeritageSite> &sites)
{
    m_availableSites = sites;

    // 重新建立 facet 選項，但保留目前使用者已選的條件。
    populateFacetCombos();
}

void SearchPanel::setResults(const QVector<HeritageSite> &results, int totalSiteCount)
{
    m_resultTable->setRowCount(results.size());

    for (int row = 0; row < results.size(); ++row) {
        const HeritageSite &site = results[row];

        const QStringList values = {
            site.title,
            site.category,
            site.displayPeriod(),
            site.district,
            yearRangeLabel(site),
            site.preservationStatus,
            site.sourceName,
            site.tags
        };

        for (int col = 0; col < values.size(); ++col) {
            QTableWidgetItem *item = new QTableWidgetItem(values[col]);

            // 每個 cell 都存 site id，之後雙擊任一欄都能找到該筆資料。
            item->setData(kSiteIdRole, site.id);

            m_resultTable->setItem(row, col, item);
        }
    }

    m_resultTable->resizeColumnsToContents();

    m_resultCountLabel->setText(
        QString("Showing %1 of %2 heritage records.")
            .arg(results.size())
            .arg(totalSiteCount));
}

SearchFilterState SearchPanel::currentFilter() const
{
    SearchFilterState state;

    state.keyword = m_keywordInput->text().trimmed();

    state.category = currentComboValue(m_categoryComboBox);
    state.district = currentComboValue(m_districtComboBox);
    state.periodLabel = currentComboValue(m_periodComboBox);
    state.preservationStatus = currentComboValue(m_statusComboBox);
    state.sourceName = currentComboValue(m_sourceComboBox);
    state.tag = currentComboValue(m_tagComboBox);

    state.searchDescription = m_searchDescriptionCheckBox->isChecked();

    return state;
}

void SearchPanel::createLayout()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    QLabel *titleLabel = new QLabel("Advanced Search & Facets", this);
    titleLabel->setObjectName("SearchPanelTitleLabel");

    QLabel *hintLabel = new QLabel(
        "Search heritage metadata and narrow results by category, district, period, status, source, and tags.",
        this);
    hintLabel->setWordWrap(true);

    m_keywordInput = new QLineEdit(this);
    m_keywordInput->setPlaceholderText("Search title, category, district, source, tags...");
    connect(m_keywordInput, &QLineEdit::textChanged,
            this, &SearchPanel::emitFilterChanged);

    m_categoryComboBox = new QComboBox(this);
    m_districtComboBox = new QComboBox(this);
    m_periodComboBox = new QComboBox(this);
    m_statusComboBox = new QComboBox(this);
    m_sourceComboBox = new QComboBox(this);
    m_tagComboBox = new QComboBox(this);

    const QList<QComboBox *> comboBoxes = {
        m_categoryComboBox,
        m_districtComboBox,
        m_periodComboBox,
        m_statusComboBox,
        m_sourceComboBox,
        m_tagComboBox
    };

    for (QComboBox *comboBox : comboBoxes) {
        connect(comboBox, qOverload<int>(&QComboBox::currentIndexChanged),
                this, &SearchPanel::emitFilterChanged);
    }

    m_searchDescriptionCheckBox = new QCheckBox("Include Description in keyword search", this);
    m_searchDescriptionCheckBox->setChecked(true);
    connect(m_searchDescriptionCheckBox, &QCheckBox::toggled,
            this, &SearchPanel::emitFilterChanged);

    m_clearFiltersButton = new QPushButton("Clear Filters", this);
    connect(m_clearFiltersButton, &QPushButton::clicked,
            this, &SearchPanel::clearFilters);

    QGridLayout *facetLayout = new QGridLayout();
    facetLayout->addWidget(new QLabel("Category", this), 0, 0);
    facetLayout->addWidget(m_categoryComboBox, 0, 1);
    facetLayout->addWidget(new QLabel("District", this), 0, 2);
    facetLayout->addWidget(m_districtComboBox, 0, 3);

    facetLayout->addWidget(new QLabel("Period", this), 1, 0);
    facetLayout->addWidget(m_periodComboBox, 1, 1);
    facetLayout->addWidget(new QLabel("Status", this), 1, 2);
    facetLayout->addWidget(m_statusComboBox, 1, 3);

    facetLayout->addWidget(new QLabel("Source", this), 2, 0);
    facetLayout->addWidget(m_sourceComboBox, 2, 1);
    facetLayout->addWidget(new QLabel("Tag", this), 2, 2);
    facetLayout->addWidget(m_tagComboBox, 2, 3);

    facetLayout->addWidget(m_searchDescriptionCheckBox, 3, 0, 1, 2);
    facetLayout->addWidget(m_clearFiltersButton, 3, 3);

    m_resultCountLabel = new QLabel("Showing 0 of 0 heritage records.", this);

    m_resultTable = new QTableWidget(this);
    m_resultTable->setColumnCount(8);
    m_resultTable->setHorizontalHeaderLabels({
        "Title",
        "Category",
        "Period",
        "District",
        "Years",
        "Status",
        "Source",
        "Tags"
    });

    m_resultTable->horizontalHeader()->setStretchLastSection(true);
    m_resultTable->verticalHeader()->setVisible(false);
    m_resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultTable->setAlternatingRowColors(true);

    connect(m_resultTable, &QTableWidget::cellDoubleClicked,
            this, &SearchPanel::handleResultDoubleClicked);

    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(hintLabel);
    mainLayout->addWidget(m_keywordInput);
    mainLayout->addLayout(facetLayout);
    mainLayout->addWidget(m_resultCountLabel);
    mainLayout->addWidget(m_resultTable, 1);

    populateFacetCombos();
}

void SearchPanel::populateFacetCombos()
{
    const SearchFilterState previousFilter = currentFilter();

    m_isUpdating = true;

    populateFacetCombo(m_categoryComboBox,
                       "All Categories",
                       uniqueNonEmptyValues("category"),
                       previousFilter.category);

    populateFacetCombo(m_districtComboBox,
                       "All Districts",
                       uniqueNonEmptyValues("district"),
                       previousFilter.district);

    populateFacetCombo(m_periodComboBox,
                       "All Periods",
                       uniqueNonEmptyValues("period"),
                       previousFilter.periodLabel);

    populateFacetCombo(m_statusComboBox,
                       "All Statuses",
                       uniqueNonEmptyValues("status"),
                       previousFilter.preservationStatus);

    populateFacetCombo(m_sourceComboBox,
                       "All Sources",
                       uniqueNonEmptyValues("source"),
                       previousFilter.sourceName);

    populateFacetCombo(m_tagComboBox,
                       "All Tags",
                       uniqueTags(),
                       previousFilter.tag);

    m_isUpdating = false;
}

QString SearchPanel::currentComboValue(const QComboBox *comboBox) const
{
    if (!comboBox) {
        return {};
    }

    return comboBox->currentData().toString();
}

void SearchPanel::populateFacetCombo(QComboBox *comboBox,
                                     const QString &allLabel,
                                     const QStringList &values,
                                     const QString &previousValue)
{
    if (!comboBox) {
        return;
    }

    const QSignalBlocker blocker(comboBox);
    Q_UNUSED(blocker)

    comboBox->clear();
    comboBox->addItem(allLabel, "");

    for (const QString &value : values) {
        comboBox->addItem(value, value);
    }

    const int previousIndex = comboBox->findData(previousValue);
    comboBox->setCurrentIndex(previousIndex >= 0 ? previousIndex : 0);
}

QStringList SearchPanel::uniqueNonEmptyValues(const QString &fieldName) const
{
    QSet<QString> seen;
    QStringList values;

    for (const HeritageSite &site : m_availableSites) {
        QString value;

        if (fieldName == "category") {
            value = site.category;
        } else if (fieldName == "district") {
            value = site.district;
        } else if (fieldName == "period") {
            value = site.periodLabel;
        } else if (fieldName == "status") {
            value = site.preservationStatus;
        } else if (fieldName == "source") {
            value = site.sourceName;
        }

        value = value.trimmed();

        if (value.isEmpty()) {
            continue;
        }

        const QString key = value.toLower();

        if (seen.contains(key)) {
            continue;
        }

        seen.insert(key);
        values.append(value);
    }

    values.sort(Qt::CaseInsensitive);
    return values;
}

QStringList SearchPanel::uniqueTags() const
{
    QSet<QString> seen;
    QStringList values;

    for (const HeritageSite &site : m_availableSites) {
        for (const QString &tag : splitHeritageTags(site.tags)) {
            const QString key = tag.toLower();

            if (seen.contains(key)) {
                continue;
            }

            seen.insert(key);
            values.append(tag);
        }
    }

    values.sort(Qt::CaseInsensitive);
    return values;
}

void SearchPanel::emitFilterChanged()
{
    if (m_isUpdating) {
        return;
    }

    emit searchFilterChanged(currentFilter());
}

void SearchPanel::clearFilters()
{
    m_isUpdating = true;

    m_keywordInput->clear();

    m_categoryComboBox->setCurrentIndex(0);
    m_districtComboBox->setCurrentIndex(0);
    m_periodComboBox->setCurrentIndex(0);
    m_statusComboBox->setCurrentIndex(0);
    m_sourceComboBox->setCurrentIndex(0);
    m_tagComboBox->setCurrentIndex(0);

    m_searchDescriptionCheckBox->setChecked(true);

    m_isUpdating = false;

    emitFilterChanged();
}

void SearchPanel::handleResultDoubleClicked(int row, int column)
{
    Q_UNUSED(column)

    if (row < 0 || row >= m_resultTable->rowCount()) {
        return;
    }

    const QTableWidgetItem *item = m_resultTable->item(row, 0);

    if (!item) {
        return;
    }

    const int siteId = item->data(kSiteIdRole).toInt();

    if (siteId < 0) {
        return;
    }

    emit siteActivated(siteId);
}

QString SearchPanel::yearRangeLabel(const HeritageSite &site)
{
    if (site.startYear > 0 && site.endYear > 0) {
        return QString("%1–%2").arg(site.startYear).arg(site.endYear);
    }

    if (site.startYear > 0) {
        return QString::number(site.startYear);
    }

    if (site.endYear > 0) {
        return QString::number(site.endYear);
    }

    return "Undated";
}