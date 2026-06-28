#include "ui/SearchPanel.h"
#include "ui/MainWindow.h"
#include "ui/TimelinePanel.h"
#include "ui/LayerPanel.h"
#include "utils/GeoCoordinateTransformer.h"
#include "database/DatabaseManager.h"
#include "database/MigrationManager.h"
#include "opengl/MapCanvas.h"
#include "repositories/HeritageSiteRepository.h"
#include "services/ImportService.h"
#include "ui/ImportCsvDialog.h"
#include "ui/MetadataEditorPanel.h"
#include "ui/SearchPanel.h"

#include <QAbstractItemView>
#include <QAction>
#include <QDateTime>
#include <QDockWidget>
#include <QFormLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("HeritageMap Studio");

    // 允許 dock 巢狀與 tab 化，讓整體更像專業桌面工具。
    setDockOptions(QMainWindow::AllowNestedDocks |
                   QMainWindow::AllowTabbedDocks |
                   QMainWindow::AnimatedDocks);

    // Phase 2：先初始化資料層，UI 建立時就能使用真實 database 資料。
    const bool databaseReady = initializeDatabase();
    if (databaseReady) {
        loadHeritageSitesFromDatabase();
    }

    // 中央區域放 OpenGL map canvas。
    m_mapCanvas = new MapCanvas(this);
    m_mapCanvas->setHeritageSites(m_projectedSites);
    m_mapCanvas->setLayerState(m_layerState);
    m_mapCanvas->setTimelineFilter(m_timelineState);
    m_mapCanvas->setSearchFilter(m_searchFilterState);
    setCentralWidget(m_mapCanvas);
    
    // UI 建立順序：actions → menu → toolbar → docks → status。
    createActions();
    createMenuBar();
    createToolBar();
    createDockPanels();
    createStatusBar();

    // 連接 OpenGL canvas 的互動訊號，讓狀態列與右側 inspector 同步更新。
    connect(m_mapCanvas, &MapCanvas::cursorWorldPositionChanged,
            this, &MainWindow::updateCursorPosition);
    connect(m_mapCanvas, &MapCanvas::viewChanged,
            this, &MainWindow::updateViewStatus);
    connect(m_mapCanvas, &MapCanvas::heritageSiteSelected,
            this, &MainWindow::updateSelectedSite);

    appendLog("Application started.");

    if (databaseReady) {
        appendLog(QString("Database opened: %1").arg(m_databaseManager->databasePath()));
        appendLog(QString("Loaded %1 heritage sites from SQLite.").arg(m_sites.size()));
        appendLog(m_projectionSummary);
    } else {
        appendLog("Database initialization failed. Check terminal output or Qt SQL driver installation.");
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::createActions()
{
    // File actions：Phase 2 先保留 UI 入口，後續 phase 再接 project/import/export。
    m_newProjectAction = new QAction("New Project", this);
    m_openProjectAction = new QAction("Open Project", this);
    m_saveProjectAction = new QAction("Save Project", this);
    m_importCsvAction = new QAction("Import CSV", this);
    m_exitAction = new QAction("Exit", this);

    // View actions：Reset View 控制 OpenGL camera。
    m_resetViewAction = new QAction("Reset View", this);
    m_aboutAction = new QAction("About", this);

    // 設定常用快捷鍵，讓桌面工具更有完成度。
    m_newProjectAction->setShortcut(QKeySequence::New);
    m_openProjectAction->setShortcut(QKeySequence::Open);
    m_saveProjectAction->setShortcut(QKeySequence::Save);
    m_exitAction->setShortcut(QKeySequence::Quit);

    // 尚未完成的功能先顯示 log，不要做空按鈕讓使用者迷路。
    connect(m_newProjectAction, &QAction::triggered, this, [this]() {
        appendLog("New Project clicked. Project system will be implemented in a later phase.");
    });
    connect(m_openProjectAction, &QAction::triggered, this, [this]() {
        appendLog("Open Project clicked. Project file loading will be implemented in a later phase.");
    });
    connect(m_saveProjectAction, &QAction::triggered, this, [this]() {
        appendLog("Save Project clicked. Project persistence will be implemented in a later phase.");
    });
    // Phase 3：Import CSV 進入真正匯入流程
    connect(m_importCsvAction, &QAction::triggered,
            this, &MainWindow::openImportCsvDialog);
    connect(m_exitAction, &QAction::triggered, this, &QMainWindow::close);
    connect(m_resetViewAction, &QAction::triggered, this, &MainWindow::resetMapView);
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
}

void MainWindow::createMenuBar()
{
    // File menu：專案、匯入、離開。
    QMenu *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction(m_newProjectAction);
    fileMenu->addAction(m_openProjectAction);
    fileMenu->addAction(m_saveProjectAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_importCsvAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAction);

    // View menu：目前先放 reset view，後面會加 layer/timeline panel toggle。
    QMenu *viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction(m_resetViewAction);

    // Tools menu：先預留給後續資料工具。
    QMenu *toolsMenu = menuBar()->addMenu("Tools");
    toolsMenu->addAction(m_importCsvAction);

    // Help menu：作品集軟體基本禮貌。
    QMenu *helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction(m_aboutAction);
}

void MainWindow::createToolBar()
{
    // Toolbar 放最常用的入口，文字維持英文，符合作品集展示。
    QToolBar *mainToolBar = addToolBar("Main Toolbar");
    mainToolBar->setObjectName("MainToolbar");
    mainToolBar->setMovable(false);

    mainToolBar->addAction(m_newProjectAction);
    mainToolBar->addAction(m_openProjectAction);
    mainToolBar->addAction(m_saveProjectAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(m_importCsvAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(m_resetViewAction);
}

void MainWindow::createDockPanels()
{
    // 左側上方：專案樹。
    QDockWidget *projectDock = new QDockWidget("Project Explorer", this);
    projectDock->setObjectName("ProjectExplorerDock");
    projectDock->setWidget(createProjectExplorerPanel());
    addDockWidget(Qt::LeftDockWidgetArea, projectDock);

    // 左側中段：圖層清單。
    QDockWidget *layerDock = new QDockWidget("Layers", this);
    layerDock->setObjectName("LayerDock");
    layerDock->setWidget(createLayerPanel());
    addDockWidget(Qt::LeftDockWidgetArea, layerDock);
    splitDockWidget(projectDock, layerDock, Qt::Vertical);

    // 左側下段：時間軸篩選。
    QDockWidget *timelineDock = new QDockWidget("Timeline", this);
    timelineDock->setObjectName("TimelineDock");
    timelineDock->setWidget(createTimelinePanel());
    addDockWidget(Qt::LeftDockWidgetArea, timelineDock);
    splitDockWidget(layerDock, timelineDock, Qt::Vertical);
        
    // 右側：metadata inspector。
    QDockWidget *metadataDock = new QDockWidget("Metadata Inspector", this);
    metadataDock->setObjectName("MetadataInspectorDock");
    metadataDock->setWidget(createMetadataInspectorPanel());
    addDockWidget(Qt::RightDockWidgetArea, metadataDock);

    // 下方：搜尋結果與 log。
    QDockWidget *bottomDock = new QDockWidget("Search & Logs", this);
    bottomDock->setObjectName("BottomDock");
    bottomDock->setWidget(createBottomPanel());
    addDockWidget(Qt::BottomDockWidgetArea, bottomDock);
}

void MainWindow::createStatusBar()
{
    // 狀態列顯示游標世界座標與 map camera 狀態。
    m_cursorLabel = new QLabel("Cursor: --, --", this);
    m_viewLabel = new QLabel("Zoom: 1.00 | Center: 0.0, 0.0", this);

    statusBar()->addPermanentWidget(m_cursorLabel);
    statusBar()->addPermanentWidget(m_viewLabel);
    statusBar()->showMessage("Ready");
}

QWidget *MainWindow::createProjectExplorerPanel()
{
    QWidget *panel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);

    // Project tree 由 SQLite 載入的資料產生。
    m_projectTree = new QTreeWidget(panel);
    m_projectTree->setHeaderLabel("Heritage Project");
    populateProjectTree();

    layout->addWidget(m_projectTree);
    return panel;
}

QWidget *MainWindow::createLayerPanel()
{
    // Phase 6：Layers 從假的 QListWidget 改成真正的 LayerPanel。
    m_layerPanel = new LayerPanel(this);
    m_layerPanel->setHeritageSites(m_sites);

    connect(m_layerPanel, &LayerPanel::layerStateChanged,
            this, &MainWindow::updateLayerState);

    // 初始化目前 layer state，並立刻套用到 MapCanvas。
    m_layerState = m_layerPanel->currentState();

    if (m_mapCanvas) {
        m_mapCanvas->setLayerState(m_layerState);
    }

    return m_layerPanel;
}

QWidget *MainWindow::createTimelinePanel()
{
    // Phase 7：TimelinePanel 只產生 TimelineFilterState，不直接碰 MapCanvas。
    m_timelinePanel = new TimelinePanel(this);

    connect(m_timelinePanel, &TimelinePanel::timelineFilterChanged,
            this, &MainWindow::updateTimelineState);

    m_timelinePanel->setHeritageSites(m_sites);
    m_timelineState = m_timelinePanel->currentState();

    if (m_mapCanvas) {
        m_mapCanvas->setTimelineFilter(m_timelineState);
    }

    return m_timelinePanel;
}

QWidget *MainWindow::createMetadataInspectorPanel()
{
    // Phase 5：右側 panel 改成獨立 MetadataEditorPanel。
    // MainWindow 只接收 saveRequested，真正更新資料仍交給 repository。
    m_metadataEditorPanel = new MetadataEditorPanel(this);

    connect(m_metadataEditorPanel, &MetadataEditorPanel::saveRequested,
            this, &MainWindow::saveEditedSite);

    return m_metadataEditorPanel;
}

QWidget *MainWindow::createBottomPanel()
{
    QWidget *panel = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 8, 8, 8);

    QTabWidget *tabs = new QTabWidget(panel);

    // Phase 8：搜尋面板獨立成 SearchPanel，避免 MainWindow 繼續膨脹。
    m_searchPanel = new SearchPanel(tabs);
    m_searchPanel->setAvailableSites(m_sites);

    connect(m_searchPanel, &SearchPanel::searchFilterChanged,
            this, &MainWindow::updateSearchFilter);

    connect(m_searchPanel, &SearchPanel::siteActivated,
            this, &MainWindow::updateSelectedSite);

    m_logOutput = new QPlainTextEdit(tabs);
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumBlockCount(500);

    tabs->addTab(m_searchPanel, "Search Results");
    tabs->addTab(m_logOutput, "Import / System Log");

    layout->addWidget(tabs);

    refreshSearchResults();

    return panel;
}

bool MainWindow::initializeDatabase()
{
    m_databaseManager = std::make_unique<DatabaseManager>();

    const QString databasePath = DatabaseManager::defaultDatabasePath();
    if (!m_databaseManager->open(databasePath)) {
        QMessageBox::critical(this,
                              "Database Error",
                              QString("Failed to open SQLite database.\n\n%1")
                                  .arg(m_databaseManager->lastError()));
        return false;
    }

    MigrationManager migrationManager(m_databaseManager->database());
    QString migrationError;
    if (!migrationManager.migrate(&migrationError)) {
        QMessageBox::critical(this,
                              "Migration Error",
                              QString("Failed to migrate database schema.\n\n%1")
                                  .arg(migrationError));
        return false;
    }

    m_siteRepository = std::make_unique<HeritageSiteRepository>(m_databaseManager->database());

    QString seedError;
    if (!m_siteRepository->ensureSeedData(&seedError)) {
        QMessageBox::critical(this,
                              "Seed Data Error",
                              QString("Failed to insert sample heritage data.\n\n%1")
                                  .arg(seedError));
        return false;
    }

    return true;
}

void MainWindow::loadHeritageSitesFromDatabase()
{
    if (!m_siteRepository) {
        return;
    }

    QString error;
    m_sites = m_siteRepository->findAll(&error);

    if (!error.isEmpty()) {
        QMessageBox::warning(this,
                             "Query Error",
                             QString("Failed to load heritage sites.\n\n%1").arg(error));
    }

    // Phase 4：每次 database raw data 更新後，都重新建立投影資料。
    rebuildProjectedSites();
}

void MainWindow::rebuildProjectedSites()
{
    GeoProjectionInfo projectionInfo;
    m_projectedSites = GeoCoordinateTransformer::projectSitesToLocalMeters(m_sites, &projectionInfo);
    m_projectionSummary = projectionInfo.summary();
}

void MainWindow::refreshHeritageViews()
{
    // 匯入或更新資料後，同步更新 layer count。
    if (m_layerPanel) {
        m_layerPanel->setHeritageSites(m_sites);
        m_layerState = m_layerPanel->currentState();
    }

    // 匯入或更新資料後，同步更新 timeline range。
    if (m_timelinePanel) {
        m_timelinePanel->setHeritageSites(m_sites);
        m_timelineState = m_timelinePanel->currentState();
    }

    // 匯入或更新資料後，同步更新 search facet options。
    if (m_searchPanel) {
        m_searchPanel->setAvailableSites(m_sites);
        m_searchFilterState = m_searchPanel->currentFilter();
    }

    // 將最新投影資料推回 OpenGL canvas。
    if (m_mapCanvas) {
        m_mapCanvas->setHeritageSites(m_projectedSites);
        m_mapCanvas->setLayerState(m_layerState);
        m_mapCanvas->setTimelineFilter(m_timelineState);
        m_mapCanvas->setSearchFilter(m_searchFilterState);

        // Phase 5：資料刷新後，用 site id 重新指定選取點。
        // 不用舊 index，因為排序或資料重載後 index 可能不同。
        if (m_currentSelectedSiteId >= 0) {
            m_mapCanvas->selectSiteById(m_currentSelectedSiteId);
        }
    }

    // 重新建立左側專案樹。
    if (m_projectTree) {
        populateProjectTree();
    }

    refreshSearchResults();
}

std::optional<HeritageSite> MainWindow::projectedSiteById(int siteId) const
{
    for (const HeritageSite &site : m_projectedSites) {
        if (site.id == siteId) {
            return site;
        }
    }

    return std::nullopt;
}

std::optional<HeritageSite> MainWindow::siteById(int siteId) const
{
    for (const HeritageSite &site : m_sites) {
        if (site.id == siteId) {
            return site;
        }
    }

    return std::nullopt;
}

bool MainWindow::sitePassesMapFilters(const HeritageSite &site) const
{
    // Map filters = layer + timeline。
    if (!m_layerState.isHeritageCategoryVisible(site.category)) {
        return false;
    }

    if (!m_timelineState.matchesSite(site)) {
        return false;
    }

    return true;
}

bool MainWindow::sitePassesCurrentFilters(const HeritageSite &site) const
{
    // Current filters = layer + timeline + search facets。
    if (!sitePassesMapFilters(site)) {
        return false;
    }

    if (!m_searchFilterState.matchesSite(site)) {
        return false;
    }

    return true;
}

QVector<HeritageSite> MainWindow::filteredSitesForSearchPanel() const
{
    QVector<HeritageSite> results;
    results.reserve(m_sites.size());

    for (const HeritageSite &site : m_sites) {
        if (sitePassesCurrentFilters(site)) {
            results.append(site);
        }
    }

    return results;
}

void MainWindow::populateProjectTree()
{
    m_projectTree->clear();

    // Project tree：先依 category 分組，讓資料結構更像文化專案。
    QTreeWidgetItem *root = new QTreeWidgetItem({"Taipei Heritage Map"});
    QTreeWidgetItem *sitesRoot = new QTreeWidgetItem({"Heritage Sites"});
    QTreeWidgetItem *photosRoot = new QTreeWidgetItem({"Archive Photos"});
    QTreeWidgetItem *eventsRoot = new QTreeWidgetItem({"Historical Events"});
    QTreeWidgetItem *routesRoot = new QTreeWidgetItem({"Cultural Routes"});

    for (const HeritageSite &site : m_sites) {
        QTreeWidgetItem *siteItem = new QTreeWidgetItem({site.title});
        siteItem->setToolTip(0, QString("%1 | %2").arg(site.category, site.displayPeriod()));
        sitesRoot->addChild(siteItem);
    }

    // Phase 2 先保留後續功能入口。
    photosRoot->addChild(new QTreeWidgetItem({"Photo collections will be implemented in Phase 10"}));
    eventsRoot->addChild(new QTreeWidgetItem({"Historical events will be implemented in a later phase"}));
    routesRoot->addChild(new QTreeWidgetItem({"Cultural routes will be implemented in Phase 9"}));

    root->addChild(sitesRoot);
    root->addChild(photosRoot);
    root->addChild(eventsRoot);
    root->addChild(routesRoot);

    m_projectTree->addTopLevelItem(root);
    m_projectTree->expandAll();
}

void MainWindow::refreshSearchResults()
{
    if (!m_searchPanel) {
        return;
    }

    const QVector<HeritageSite> results = filteredSitesForSearchPanel();
    m_searchPanel->setResults(results, m_sites.size());
}

void MainWindow::appendLog(const QString &message)
{
    if (!m_logOutput) {
        return;
    }

    // 加上時間戳，讓 log 更像正式工具。
    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_logOutput->appendPlainText(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::resetMapView()
{
    m_mapCanvas->resetView();
    appendLog("Map view reset to visible layer extent.");
}

void MainWindow::openImportCsvDialog()
{
    if (!m_siteRepository) {
        QMessageBox::warning(this,
                             "Import CSV",
                             "Database is not ready. Please check SQLite initialization.");
        return;
    }

    ImportCsvDialog dialog(this);

    if (dialog.exec() != QDialog::Accepted) {
        appendLog("CSV import canceled.");
        return;
    }

    const QString filePath = dialog.selectedFilePath();

    if (filePath.isEmpty()) {
        appendLog("CSV import canceled: no file selected.");
        return;
    }

    ImportService importService(*m_siteRepository);
    ImportResult result;
    QString error;

    appendLog(QString("Importing CSV: %1").arg(filePath));

    if (!importService.importHeritageSitesFromCsv(filePath, &result, &error)) {
        appendLog(QString("CSV import failed: %1").arg(error));

        QMessageBox::critical(this,
                              "Import Failed",
                              QString("Failed to import CSV.\n\n%1").arg(error));
        return;
    }

    // 匯入成功後，重新從 database 載入資料。
    loadHeritageSitesFromDatabase();
    refreshHeritageViews();

    const QString summary = QString("CSV import completed. Inserted: %1, Skipped: %2, Parsed rows: %3.")
                                .arg(result.insertedRows)
                                .arg(result.skippedRows)
                                .arg(result.parsedRows);

    appendLog(summary);
    appendLog(m_projectionSummary);

    // 警告很多時只在 log 顯示前幾筆，避免 MessageBox 變成論文。
    const int warningPreviewCount = qMin(5, static_cast<int>(result.warnings.size()));
    for (int i = 0; i < warningPreviewCount; ++i) {
        appendLog(QString("Import warning: %1").arg(result.warnings[i]));
    }

    if (result.warnings.size() > warningPreviewCount) {
        appendLog(QString("Import warning: %1 more warnings not shown.")
                      .arg(result.warnings.size() - warningPreviewCount));
    }

    QMessageBox::information(this,
                             "Import Completed",
                             summary);
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this,
                       "About HeritageMap Studio",
                       "HeritageMap Studio\n\n"
                       "A C++ Qt OpenGL desktop workstation for cultural heritage mapping, "
                       "metadata exploration, and historical timeline visualization.\n\n"
                       "Phase 8: Advanced search and faceted filtering.");
}

void MainWindow::updateCursorPosition(double worldX, double worldY)
{
    m_cursorLabel->setText(QString("Cursor: %1, %2")
                               .arg(worldX, 0, 'f', 1)
                               .arg(worldY, 0, 'f', 1));
}

void MainWindow::updateViewStatus(float zoom, float centerX, float centerY)
{
    m_viewLabel->setText(QString("Zoom: %1 | Center: %2, %3")
                             .arg(zoom, 0, 'f', 2)
                             .arg(centerX, 0, 'f', 1)
                             .arg(centerY, 0, 'f', 1));
}

void MainWindow::updateSelectedSite(int siteId)
{
    if (!m_siteRepository) {
        return;
    }

    QString error;
    const std::optional<HeritageSite> siteResult = m_siteRepository->findById(siteId, &error);

    if (!error.isEmpty()) {
        appendLog(QString("Failed to load selected site: %1").arg(error));
        return;
    }

    if (!siteResult.has_value()) {
        appendLog(QString("Selected site id not found: %1").arg(siteId));
        return;
    }

    m_currentSelectedSiteId = siteId;

    const HeritageSite &site = siteResult.value();
    const std::optional<HeritageSite> projectedSite = projectedSiteById(site.id);

    const double displayMapX = projectedSite.has_value() ? projectedSite->mapX : site.mapX;
    const double displayMapY = projectedSite.has_value() ? projectedSite->mapY : site.mapY;

    // 同步 OpenGL selection highlight。
    if (m_mapCanvas) {
        m_mapCanvas->selectSiteById(siteId);
    }

    // Phase 5：交給 MetadataEditorPanel 顯示與編輯。
    if (m_metadataEditorPanel) {
        m_metadataEditorPanel->setSite(site, displayMapX, displayMapY, m_projectionSummary);
    }

    statusBar()->showMessage(QString("Selected: %1").arg(site.title), 3000);
    appendLog(QString("Selected site: %1 (%2)").arg(site.title, site.category));
}

void MainWindow::saveEditedSite(const HeritageSite &site)
{
    if (!m_siteRepository) {
        QMessageBox::warning(this,
                             "Save Changes",
                             "Database is not ready. Please check SQLite initialization.");
        return;
    }

    QString error;

    if (!m_siteRepository->update(site, &error)) {
        appendLog(QString("Failed to update metadata: %1").arg(error));

        QMessageBox::critical(this,
                              "Save Failed",
                              QString("Failed to update metadata.\n\n%1").arg(error));
        return;
    }

    appendLog(QString("Updated metadata: %1").arg(site.title));

    // 更新後重新載入 database，避免 UI 持有舊資料。
    m_currentSelectedSiteId = site.id;
    loadHeritageSitesFromDatabase();
    refreshHeritageViews();

    // 重新選取同一筆，讓 MetadataEditorPanel 顯示最新資料與 updated projection。
    updateSelectedSite(site.id);

    statusBar()->showMessage(QString("Saved: %1").arg(site.title), 3000);

    QMessageBox::information(this,
                             "Save Completed",
                             QString("Metadata updated successfully.\n\n%1").arg(site.title));
}


void MainWindow::updateLayerState(const MapLayerState &state)
{
    m_layerState = state;

    if (m_mapCanvas) {
        m_mapCanvas->setLayerState(m_layerState);
    }

    // Phase 8：Search Results 同步 layer + timeline + search facets。
    refreshSearchResults();

    const int visibleCategoryCount = m_layerState.visibleHeritageCategoryKeys.size();
    const int opacityPercent = static_cast<int>(m_layerState.heritageSitesOpacity * 100.0f);

    statusBar()->showMessage(
        QString("Layers updated: %1 visible categories, opacity %2%.")
            .arg(visibleCategoryCount)
            .arg(opacityPercent),
        2500);
}

void MainWindow::updateTimelineState(const TimelineFilterState &state)
{
    m_timelineState = state;

    if (m_mapCanvas) {
        m_mapCanvas->setTimelineFilter(m_timelineState);
    }

    // Phase 8：Search Results 同步 timeline + layer + search facets。
    refreshSearchResults();

    statusBar()->showMessage(m_timelineState.displayLabel(), 2500);
}

void MainWindow::updateSearchFilter(const SearchFilterState &state)
{
    m_searchFilterState = state;

    if (m_mapCanvas) {
        m_mapCanvas->setSearchFilter(m_searchFilterState);
    }

    refreshSearchResults();

    statusBar()->showMessage(m_searchFilterState.displayLabel(), 2500);
}
