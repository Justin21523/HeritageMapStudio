#pragma once

#include "models/HeritageSite.h"
#include "models/MapLayerState.h"
#include "models/TimelineFilterState.h"
#include "models/SearchFilterState.h"

#include <QColor>
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPoint>
#include <QString>
#include <QVector>
#include <QRectF>
#include <QVector2D>

class MapCanvas final : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit MapCanvas(QWidget *parent = nullptr);
    ~MapCanvas() override;

    // 提供給 MainWindow 的控制入口。
    void resetView();

    // Phase 4：meter-based GIS coordinate 可能範圍很大，Reset View 應該回到資料範圍，而不是固定 0,0。
    void fitToContent();

    void setHeritageSites(const QVector<HeritageSite> &sites);

    // Phase 6：套用 layer visibility / opacity。
    void setLayerState(const MapLayerState &layerState);
    // Phase 7：套用時間軸篩選。
    void setTimelineFilter(const TimelineFilterState &timelineState);
    // Phase 8：套用搜尋與 facet 篩選。
    void setSearchFilter(const SearchFilterState &searchState);

    // Phase 5：讓 MainWindow 在資料刷新後重新指定目前選取點位。
    void selectSiteById(int siteId);
    void clearSelection();


signals:
    // 這些訊號讓 OpenGL canvas 不需要知道 MainWindow 的細節。
    void cursorWorldPositionChanged(double worldX, double worldY);
    void viewChanged(float zoom, float centerX, float centerY);

    // Phase 2 改成傳 site id；完整 metadata 由 MainWindow 透過 Repository 查詢。
    void heritageSiteSelected(int siteId);

protected:
    // QOpenGLWidget lifecycle：OpenGL 初始化、視窗尺寸變化、每次重繪。
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;

    // 滑鼠互動：pan / zoom / click selection。
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct HeritagePoint {
        int siteId = -1;
        QString title;
        QString category;

        int startYear = 0;
        int endYear = 0;

        // Phase 8：保留完整 HeritageSite，讓 SearchFilterState 可以在 MapCanvas 內同步過濾。
        HeritageSite site;

        QVector2D position;
        QColor color;
    };

    // 初始化與繪圖管線。
    bool createShaderProgram();
    void updateVertexLayout();
    void drawGrid();
    void drawPoints();
    void drawSelectedPoint();
    void uploadAndDraw(const QVector<float> &vertices,
                       GLenum primitiveType,
                       float pointSize,
                       bool circularPoint);

    // 座標換算：後續 GIS 經緯度投影會接在這一層。
    QMatrix4x4 projectionMatrix() const;
    QRectF visibleWorldRect() const;
    float chooseGridSpacing() const;
    static float niceGridSpacing(float roughSpacing);

    QVector2D screenToWorld(const QPoint &screenPoint) const;
    QPointF worldToScreen(const QVector2D &worldPoint) const;
    int findPointAtScreenPosition(const QPoint &screenPoint) const;
    bool isPointVisible(const HeritagePoint &point) const;

    // 小工具：把 QColor 轉成 shader 需要的 0~1 float。
    static void appendVertex(QVector<float> &vertices,
                             float x,
                             float y,
                             const QColor &color);

private:
    // OpenGL objects：Phase 1/2 使用單一 VAO/VBO + shader 畫 grid 和 points。
    QOpenGLShaderProgram m_shaderProgram;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo;

    // Phase 2：點位由 SQLite → Repository → MainWindow → MapCanvas 載入。
    QVector<HeritagePoint> m_points;
    // Phase 6：目前地圖圖層狀態。
    MapLayerState m_layerState = MapLayerState::createDefault();
    // Phase 7：目前時間軸篩選狀態。
    TimelineFilterState m_timelineState = TimelineFilterState::createDisabled();
    // Phase 8：目前搜尋與 facet 篩選狀態。
    SearchFilterState m_searchState = SearchFilterState::createDefault();
    
    // 2D camera 狀態。
    QVector2D m_cameraCenter = QVector2D(0.0f, 0.0f);
    float m_zoom = 1.0f;

    // 滑鼠狀態。
    QPoint m_lastMousePosition;
    bool m_isPanning = false;
    bool m_hasDragged = false;

    // Hover / selection 狀態。
    int m_hoveredPointIndex = -1;
    int m_selectedPointIndex = -1;
};