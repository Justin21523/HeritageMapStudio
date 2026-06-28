#include "ui/LayerPanel.h"

#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {
constexpr int kDefaultOpacityPercent = 100;
constexpr int kMinimumOpacityPercent = 20;
constexpr int kMaximumOpacityPercent = 100;
}

LayerPanel::LayerPanel(QWidget *parent)
    : QWidget(parent)
{
    createLayout();
}

void LayerPanel::setHeritageSites(const QVector<HeritageSite> &sites)
{
    m_categoryCounts.clear();

    // 先把所有預設 category count 歸零。
    for (const HeritageCategoryLayerInfo &layerInfo : defaultHeritageCategoryLayers()) {
        m_categoryCounts.insert(layerInfo.key, 0);
    }

    // 依資料中的 category 計算每個 layer 有幾筆。
    for (const HeritageSite &site : sites) {
        const QString key = heritageCategoryLayerKey(site.category);
        m_categoryCounts[key] = m_categoryCounts.value(key, 0) + 1;
    }

    // 更新 UI 顯示文字，但不要觸發 itemChanged。
    const QSignalBlocker blocker(m_layerTree);
    Q_UNUSED(blocker)

    for (auto it = m_categoryItems.begin(); it != m_categoryItems.end(); ++it) {
        const QString key = it.key();
        QTreeWidgetItem *item = it.value();

        const QString baseLabel = m_categoryBaseLabels.value(key, key);
        const int count = m_categoryCounts.value(key, 0);

        item->setText(0, categoryItemLabel(baseLabel, count));
    }
}

MapLayerState LayerPanel::currentState() const
{
    MapLayerState state;
    state.heritageSitesVisible = m_heritageRootItem &&
                                 m_heritageRootItem->checkState(0) != Qt::Unchecked;

    state.heritageSitesOpacity =
        static_cast<float>(m_opacitySlider->value()) / 100.0f;

    state.visibleHeritageCategoryKeys.clear();

    // 只有 checked 的 category 才加入可見集合。
    for (auto it = m_categoryItems.constBegin(); it != m_categoryItems.constEnd(); ++it) {
        const QString key = it.key();
        const QTreeWidgetItem *item = it.value();

        if (item && item->checkState(0) == Qt::Checked) {
            state.visibleHeritageCategoryKeys.insert(key);
        }
    }

    // Planned layers 目前只是 state placeholder，Phase 7+ 會逐步接上。
    state.historicalEventsVisible = false;
    state.archivePhotosVisible = false;
    state.culturalRoutesVisible = false;
    state.heatmapVisible = false;

    return state;
}

void LayerPanel::createLayout()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    QLabel *titleLabel = new QLabel("Map Layers", this);
    titleLabel->setObjectName("LayerPanelTitleLabel");

    QLabel *hintLabel = new QLabel(
        "Toggle heritage categories and adjust point opacity.",
        this);
    hintLabel->setWordWrap(true);

    createLayerTree();

    QLabel *opacityLabel = new QLabel("Heritage Sites Opacity", this);

    m_opacitySlider = new QSlider(Qt::Horizontal, this);
    m_opacitySlider->setRange(kMinimumOpacityPercent, kMaximumOpacityPercent);
    m_opacitySlider->setValue(kDefaultOpacityPercent);
    m_opacitySlider->setTickInterval(10);
    m_opacitySlider->setTickPosition(QSlider::TicksBelow);

    m_opacityValueLabel = new QLabel(this);
    updateOpacityLabel();

    connect(m_opacitySlider, &QSlider::valueChanged,
            this, &LayerPanel::handleOpacityChanged);

    m_resetButton = new QPushButton("Reset Layers", this);
    connect(m_resetButton, &QPushButton::clicked,
            this, &LayerPanel::resetLayers);

    layout->addWidget(titleLabel);
    layout->addWidget(hintLabel);
    layout->addWidget(m_layerTree, 1);
    layout->addWidget(opacityLabel);
    layout->addWidget(m_opacitySlider);
    layout->addWidget(m_opacityValueLabel);
    layout->addWidget(m_resetButton);
}

void LayerPanel::createLayerTree()
{
    m_layerTree = new QTreeWidget(this);
    m_layerTree->setColumnCount(1);
    m_layerTree->setHeaderLabels({"Layer Visibility"});
    m_layerTree->header()->setStretchLastSection(true);

    // Heritage Sites 是目前真正會影響 MapCanvas 的主圖層。
    m_heritageRootItem = new QTreeWidgetItem(m_layerTree, {"Heritage Sites"});
    m_heritageRootItem->setFlags(m_heritageRootItem->flags() | Qt::ItemIsUserCheckable);
    m_heritageRootItem->setCheckState(0, Qt::Checked);
    m_heritageRootItem->setToolTip(0, "Main layer for cultural heritage point records.");

    for (const HeritageCategoryLayerInfo &layerInfo : defaultHeritageCategoryLayers()) {
        QTreeWidgetItem *item = createCheckableItem(m_heritageRootItem,
                                                    categoryItemLabel(layerInfo.label, 0),
                                                    layerInfo.key,
                                                    true,
                                                    true);

        item->setToolTip(0, layerInfo.description);

        m_categoryItems.insert(layerInfo.key, item);
        m_categoryBaseLabels.insert(layerInfo.key, layerInfo.label);
        m_categoryCounts.insert(layerInfo.key, 0);
    }

    // Planned Layers 是後續功能入口，先顯示但 disabled。
    m_plannedRootItem = new QTreeWidgetItem(m_layerTree, {"Planned Layers"});
    m_plannedRootItem->setFlags(m_plannedRootItem->flags() & ~Qt::ItemIsSelectable);
    m_plannedRootItem->setToolTip(0, "These layers will be implemented in later phases.");

    createCheckableItem(m_plannedRootItem,
                        "Historical Events (Phase 7+)",
                        "historical_events",
                        false,
                        false);
    createCheckableItem(m_plannedRootItem,
                        "Archive Photos (Phase 10)",
                        "archive_photos",
                        false,
                        false);
    createCheckableItem(m_plannedRootItem,
                        "Cultural Routes (Phase 9)",
                        "cultural_routes",
                        false,
                        false);
    createCheckableItem(m_plannedRootItem,
                        "Timeline Heatmap (Phase 11)",
                        "timeline_heatmap",
                        false,
                        false);

    m_layerTree->expandAll();

    connect(m_layerTree, &QTreeWidget::itemChanged,
            this, &LayerPanel::handleItemChanged);
}

QTreeWidgetItem *LayerPanel::createCheckableItem(QTreeWidgetItem *parent,
                                                 const QString &label,
                                                 const QString &layerKey,
                                                 bool checked,
                                                 bool enabled)
{
    QTreeWidgetItem *item = new QTreeWidgetItem(parent, {label});

    item->setData(0, Qt::UserRole, layerKey);
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    if (!enabled) {
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
    }

    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);

    return item;
}

void LayerPanel::handleItemChanged(QTreeWidgetItem *item, int column)
{
    if (m_isUpdating || column != 0 || !item) {
        return;
    }

    m_isUpdating = true;
    const QSignalBlocker blocker(m_layerTree);
    Q_UNUSED(blocker)

    if (item == m_heritageRootItem) {
        // 使用者切換主圖層時，所有子 category 跟著切換。
        const Qt::CheckState rootState = m_heritageRootItem->checkState(0);
        const Qt::CheckState childState =
            rootState == Qt::Unchecked ? Qt::Unchecked : Qt::Checked;

        setCategoryChildrenChecked(childState);
    } else if (item->parent() == m_heritageRootItem) {
        // 使用者切換 category 時，主圖層變成 checked / partial / unchecked。
        updateHeritageRootCheckState();
    }

    m_isUpdating = false;

    emitCurrentState();
}

void LayerPanel::handleOpacityChanged(int value)
{
    Q_UNUSED(value)

    updateOpacityLabel();
    emitCurrentState();
}

void LayerPanel::resetLayers()
{
    m_isUpdating = true;
    const QSignalBlocker blocker(m_layerTree);
    Q_UNUSED(blocker)

    m_heritageRootItem->setCheckState(0, Qt::Checked);
    setCategoryChildrenChecked(Qt::Checked);

    m_opacitySlider->setValue(kDefaultOpacityPercent);
    updateOpacityLabel();

    m_isUpdating = false;

    emitCurrentState();
}

void LayerPanel::updateOpacityLabel()
{
    m_opacityValueLabel->setText(QString("Opacity: %1%").arg(m_opacitySlider->value()));
}

void LayerPanel::updateHeritageRootCheckState()
{
    int checkedCount = 0;
    int totalCount = 0;

    for (auto it = m_categoryItems.constBegin(); it != m_categoryItems.constEnd(); ++it) {
        const QTreeWidgetItem *item = it.value();

        if (!item) {
            continue;
        }

        ++totalCount;

        if (item->checkState(0) == Qt::Checked) {
            ++checkedCount;
        }
    }

    if (checkedCount == 0) {
        m_heritageRootItem->setCheckState(0, Qt::Unchecked);
    } else if (checkedCount == totalCount) {
        m_heritageRootItem->setCheckState(0, Qt::Checked);
    } else {
        m_heritageRootItem->setCheckState(0, Qt::PartiallyChecked);
    }
}

void LayerPanel::setCategoryChildrenChecked(Qt::CheckState checkState)
{
    for (auto it = m_categoryItems.begin(); it != m_categoryItems.end(); ++it) {
        QTreeWidgetItem *item = it.value();

        if (item) {
            item->setCheckState(0, checkState);
        }
    }
}

void LayerPanel::emitCurrentState()
{
    emit layerStateChanged(currentState());
}

QString LayerPanel::categoryItemLabel(const QString &baseLabel, int count) const
{
    return QString("%1 (%2)").arg(baseLabel).arg(count);
}