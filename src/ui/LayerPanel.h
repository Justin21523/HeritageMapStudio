#pragma once

#include "models/HeritageSite.h"
#include "models/MapLayerState.h"

#include <QHash>
#include <QString>
#include <QWidget>
#include <QVector>

class QLabel;
class QPushButton;
class QSlider;
class QTreeWidget;
class QTreeWidgetItem;

// LayerPanel 是左側 Layers 面板。
// 重點：它只負責產生 MapLayerState，不直接碰 MapCanvas。
class LayerPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit LayerPanel(QWidget *parent = nullptr);

    // 匯入或更新資料後，用目前資料更新每個 category 的數量。
    void setHeritageSites(const QVector<HeritageSite> &sites);

    // MainWindow 可讀取目前 layer state，並傳給 MapCanvas。
    MapLayerState currentState() const;

signals:
    // 使用者切換 checkbox 或 opacity 時，通知 MainWindow。
    void layerStateChanged(const MapLayerState &state);

private slots:
    void handleItemChanged(QTreeWidgetItem *item, int column);
    void handleOpacityChanged(int value);
    void resetLayers();

private:
    void createLayout();
    void createLayerTree();
    void updateOpacityLabel();

    QTreeWidgetItem *createCheckableItem(QTreeWidgetItem *parent,
                                         const QString &label,
                                         const QString &layerKey,
                                         bool checked,
                                         bool enabled = true);

    void updateHeritageRootCheckState();
    void setCategoryChildrenChecked(Qt::CheckState checkState);
    void emitCurrentState();

    QString categoryItemLabel(const QString &baseLabel, int count) const;

private:
    bool m_isUpdating = false;

    QTreeWidget *m_layerTree = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QLabel *m_opacityValueLabel = nullptr;
    QPushButton *m_resetButton = nullptr;

    QTreeWidgetItem *m_heritageRootItem = nullptr;
    QTreeWidgetItem *m_plannedRootItem = nullptr;

    // key → item，方便更新 count 與讀取 check state。
    QHash<QString, QTreeWidgetItem *> m_categoryItems;

    // key → 原始顯示名稱，例如 historic_site → Historic Sites。
    QHash<QString, QString> m_categoryBaseLabels;

    // key → count，資料匯入後更新。
    QHash<QString, int> m_categoryCounts;
};