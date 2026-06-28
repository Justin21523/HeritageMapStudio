#pragma once

#include "models/TimelineFilterState.h"
#include "models/HeritageSite.h"
#include "models/MapLayerState.h"
#include "ui/LayerPanel.h"
#include "models/SearchFilterState.h"

#include <QMainWindow>
#include <QString>
#include <QVector>
#include <optional>
#include <memory>

class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QTableWidget;
class QTextEdit;
class QTreeWidget;
class QListWidget;

class DatabaseManager;
class HeritageSiteRepository;
class LayerPanel;
class MapCanvas;
class MetadataEditorPanel;
class TimelinePanel;
class SearchPanel;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // 這些 slot 處理 UI 互動，資料存取則交給 repository。
    void resetMapView();
    void openImportCsvDialog();
    void showAboutDialog();
    void updateCursorPosition(double worldX, double worldY);
    void updateViewStatus(float zoom, float centerX, float centerY);
    void updateSelectedSite(int siteId);
    void saveEditedSite(const HeritageSite &site);
    void updateLayerState(const MapLayerState &state);
    void updateTimelineState(const TimelineFilterState &state);
    void updateSearchFilter(const SearchFilterState &state);

private:
    // UI 建立流程拆成小函式，避免 MainWindow 建構子變成一大坨義大利麵。
    void createActions();
    void createMenuBar();
    void createToolBar();
    void createDockPanels();
    void createStatusBar();

    // 每個 Dock panel 都獨立建立，之後要替換成正式 widget 會比較容易。
    QWidget *createProjectExplorerPanel();
    QWidget *createLayerPanel();
    QWidget *createTimelinePanel();
    QWidget *createMetadataInspectorPanel();
    QWidget *createBottomPanel();

    bool initializeDatabase();
    void loadHeritageSitesFromDatabase();

    // Phase 4：將 database raw sites 投影成 OpenGL local meter coordinate。
    void rebuildProjectedSites();

    // Phase 3/4：匯入資料或投影更新後，重新整理所有跟 heritage site 有關的 UI。
    void refreshHeritageViews();

    // 依 site id 取得投影後的顯示資料，給 metadata inspector 顯示 local coordinate。
    std::optional<HeritageSite> projectedSiteById(int siteId) const;
    std::optional<HeritageSite> siteById(int siteId) const;

    // Layer + Timeline filter，不含 Search。
    bool sitePassesMapFilters(const HeritageSite &site) const;

    // Layer + Timeline + Search filter。
    bool sitePassesCurrentFilters(const HeritageSite &site) const;

    QVector<HeritageSite> filteredSitesForSearchPanel() const;
    
    // Phase 2：由 SQLite 載入的 m_sites 產生 UI 內容。
    void refreshSearchResults();
    void populateSearchResults();
    void appendLog(const QString &message);
    void populateProjectTree();
    
private:
    // Central OpenGL canvas。
    MapCanvas *m_mapCanvas = nullptr;

    // Phase 2 資料層：MainWindow 持有 manager/repository，但 UI 不直接寫 SQL。
    std::unique_ptr<DatabaseManager> m_databaseManager;
    std::unique_ptr<HeritageSiteRepository> m_siteRepository;
    QVector<HeritageSite> m_sites;

    // Phase 4：給 OpenGL 顯示用的資料。
    // 若 site 有 lat/lng，mapX/mapY 會是投影後的 local meter coordinate。
    QVector<HeritageSite> m_projectedSites;
    QString m_projectionSummary;
    // Actions：menu / toolbar 共用同一批 QAction，避免功能重複寫。
    QAction *m_newProjectAction = nullptr;
    QAction *m_openProjectAction = nullptr;
    QAction *m_saveProjectAction = nullptr;
    QAction *m_importCsvAction = nullptr;
    QAction *m_resetViewAction = nullptr;
    QAction *m_exitAction = nullptr;
    QAction *m_aboutAction = nullptr;

    // Dock widgets 內部元件。
    QTreeWidget *m_projectTree = nullptr;
    // Phase 6：左側 layer panel 會產生 MapLayerState，控制 MapCanvas 顯示。
    LayerPanel *m_layerPanel = nullptr;
    MapLayerState m_layerState = MapLayerState::createDefault();
    // Phase 7：左側 timeline panel 產生 TimelineFilterState，控制 MapCanvas 與 Search Results。
    TimelinePanel *m_timelinePanel = nullptr;
    TimelineFilterState m_timelineState = TimelineFilterState::createDisabled();
    
    // Phase 8：底部 advanced search panel。
    SearchPanel *m_searchPanel = nullptr;
    SearchFilterState m_searchFilterState = SearchFilterState::createDefault();
    QPlainTextEdit *m_logOutput = nullptr;

    // Phase 5：右側 metadata editor，負責顯示與編輯文化地點 metadata。
    MetadataEditorPanel *m_metadataEditorPanel = nullptr;
    int m_currentSelectedSiteId = -1;

    // Status bar labels。
    QLabel *m_cursorLabel = nullptr;
    QLabel *m_viewLabel = nullptr;
};