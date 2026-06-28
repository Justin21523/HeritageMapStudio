#include "opengl/MapCanvas.h"

#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLShader>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {
// OpenGL shader 原始碼先寫在 cpp 內，Phase 13 polish 時再移到 resources/shaders。
const char *kVertexShaderSource = R"(
#version 330 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform float uPointSize;

out vec4 vColor;

void main()
{
    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
    gl_PointSize = uPointSize;
    vColor = aColor;
}
)";

const char *kFragmentShaderSource = R"(
#version 330 core

in vec4 vColor;
out vec4 fragColor;

uniform bool uCircularPoint;

void main()
{
    // 畫點位時，把 GL_POINTS 裁成圓形，避免看起來像粗糙方塊。
    if (uCircularPoint) {
        vec2 pointCoord = gl_PointCoord - vec2(0.5, 0.5);
        if (length(pointCoord) > 0.5) {
            discard;
        }
    }

    fragColor = vColor;
}
)";

// Phase 4：座標改成 meter-based local coordinate。
// zoom 單位可以理解為 pixels per meter。
// 城市尺度可能跨 10~30km，所以 min zoom 要比 Phase 1/2 小很多。
constexpr float kMinZoom = 0.002f;
constexpr float kMaxZoom = 80.0f;
constexpr float kTargetGridPixelSpacing = 90.0f;
constexpr float kPointPickRadiusPixels = 14.0f;
constexpr float kFitViewPaddingRatio = 0.82f;
}

MapCanvas::MapCanvas(QWidget *parent)
    : QOpenGLWidget(parent),
      m_vbo(QOpenGLBuffer::VertexBuffer)
{
    // 讓 widget 可以追蹤滑鼠移動，不必按住滑鼠才收到 mouseMoveEvent。
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

MapCanvas::~MapCanvas()
{
    // OpenGL 資源必須在 context current 的狀態下釋放。
    makeCurrent();
    m_vbo.destroy();
    m_vao.destroy();
    doneCurrent();
}

void MapCanvas::resetView()
{
    // Phase 4：Reset View 直接 fit 到目前資料範圍。
    fitToContent();
}

void MapCanvas::setHeritageSites(const QVector<HeritageSite> &sites)
{
    m_points.clear();
    m_points.reserve(sites.size());

    // 將資料模型轉成 OpenGL 繪圖用的簡化點位。
    for (const HeritageSite &site : sites) {
        HeritagePoint point;
        point.siteId = site.id;
        point.title = site.title;
        point.category = site.category;
        point.startYear = site.startYear;
        point.endYear = site.endYear;

        // Phase 8：MapCanvas 需要完整 site 來套用 SearchFilterState。
        point.site = site;

        point.position = QVector2D(static_cast<float>(site.mapX), static_cast<float>(site.mapY));
        point.color = colorForHeritageCategory(site.category);
    }

    // 換資料時清掉 hover / selection，避免舊 index 指到不存在的資料。
    m_hoveredPointIndex = -1;
    m_selectedPointIndex = -1;

    // Phase 4：資料更新後自動縮放到所有文化點位。
    fitToContent();
}

void MapCanvas::setLayerState(const MapLayerState &layerState)
{
    m_layerState = layerState;

    // Layer 改變後不自動 fitToContent，避免使用者每勾一次 checkbox 地圖就跳動。
    // 使用者可按 Reset View 重新 fit 到目前資料範圍。
    update();
}

void MapCanvas::setTimelineFilter(const TimelineFilterState &timelineState)
{
    m_timelineState = timelineState;

    // 和 layer 一樣，時間篩選改變時不自動 fit，避免播放時地圖一直跳。
    // 使用者可以按 Reset View fit 到目前可見範圍。
    update();
}

void MapCanvas::setSearchFilter(const SearchFilterState &searchState)
{
    m_searchState = searchState;

    // 搜尋條件改變時不自動 fit，避免使用者打字時地圖一直跳。
    // 使用者可以按 Reset View 重新 fit 到目前可見範圍。
    update();
}

void MapCanvas::selectSiteById(int siteId)
{
    m_selectedPointIndex = -1;

    for (int index = 0; index < m_points.size(); ++index) {
        if (m_points[index].siteId == siteId) {
            m_selectedPointIndex = index;
            break;
        }
    }

    update();
}

void MapCanvas::clearSelection()
{
    m_selectedPointIndex = -1;
    update();
}

void MapCanvas::fitToContent()
{
    QVector<HeritagePoint> visiblePoints;
    visiblePoints.reserve(m_points.size());

    for (const HeritagePoint &point : m_points) {
        if (isPointVisible(point)) {
            visiblePoints.append(point);
        }
    }

    if (visiblePoints.isEmpty()) {
        m_cameraCenter = QVector2D(0.0f, 0.0f);
        m_zoom = 1.0f;
        emit viewChanged(m_zoom, m_cameraCenter.x(), m_cameraCenter.y());
        update();
        return;
    }

    float minX = visiblePoints.first().position.x();
    float maxX = visiblePoints.first().position.x();
    float minY = visiblePoints.first().position.y();
    float maxY = visiblePoints.first().position.y();

    for (const HeritagePoint &point : visiblePoints) {
        minX = std::min(minX, point.position.x());
        maxX = std::max(maxX, point.position.x());
        minY = std::min(minY, point.position.y());
        maxY = std::max(maxY, point.position.y());
    }

    const float spanX = std::max(1.0f, maxX - minX);
    const float spanY = std::max(1.0f, maxY - minY);

    m_cameraCenter = QVector2D((minX + maxX) * 0.5f,
                               (minY + maxY) * 0.5f);

    const float safeWidth = static_cast<float>(std::max(1, width()));
    const float safeHeight = static_cast<float>(std::max(1, height()));

    const float zoomX = safeWidth / spanX;
    const float zoomY = safeHeight / spanY;

    // 加 padding，避免點位貼在視窗邊緣。
    m_zoom = std::clamp(std::min(zoomX, zoomY) * kFitViewPaddingRatio,
                        kMinZoom,
                        kMaxZoom);

    emit viewChanged(m_zoom, m_cameraCenter.x(), m_cameraCenter.y());
    update();
}

void MapCanvas::initializeGL()
{
    initializeOpenGLFunctions();

    // 背景色使用深色，符合文化地圖工具的展示風格。
    glClearColor(0.055f, 0.065f, 0.085f, 1.0f);

    // 啟用 point size，讓 shader 中的 gl_PointSize 生效。
    glEnable(GL_PROGRAM_POINT_SIZE);
    // Phase 6：啟用 alpha blending，讓 layer opacity 可以真正生效。
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        
    if (!createShaderProgram()) {
        return;
    }

    // 建立 VAO/VBO，之後每次 paintGL 只更新 vertex data。
    m_vao.create();
    m_vbo.create();
    m_vbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);

    updateVertexLayout();
    emit viewChanged(m_zoom, m_cameraCenter.x(), m_cameraCenter.y());
}

void MapCanvas::resizeGL(int width, int height)
{
    // viewport 大小跟著 widget 改變。
    glViewport(0, 0, width, height);
}

void MapCanvas::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 繪製順序：背景 grid → 選取光暈 → 正常點位。
    drawGrid();
    drawSelectedPoint();
    drawPoints();
}

void MapCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // Qt6 使用 position()，再轉成 QPoint 給目前的 picking/camera 工具。
        m_lastMousePosition = event->position().toPoint();
        m_isPanning = true;
        m_hasDragged = false;
    }

    QOpenGLWidget::mousePressEvent(event);
}

void MapCanvas::mouseMoveEvent(QMouseEvent *event)
{
    // Qt6 滑鼠位置 API 使用 position()。
    const QPoint currentPosition = event->position().toPoint();

    const QVector2D worldPosition = screenToWorld(currentPosition);
    emit cursorWorldPositionChanged(worldPosition.x(), worldPosition.y());

    // 更新 hover 狀態，讓滑鼠移過點位時可以變大顯示。
    const int previousHoveredIndex = m_hoveredPointIndex;
    m_hoveredPointIndex = findPointAtScreenPosition(currentPosition);

    if (m_isPanning && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = currentPosition - m_lastMousePosition;

        if (std::abs(delta.x()) > 1 || std::abs(delta.y()) > 1) {
            m_hasDragged = true;
        }

        // 滑鼠往右拖，camera 往左移，視覺上地圖會跟著手走。
        m_cameraCenter -= QVector2D(delta.x() / m_zoom, -delta.y() / m_zoom);
        m_lastMousePosition = currentPosition;

        emit viewChanged(m_zoom, m_cameraCenter.x(), m_cameraCenter.y());
        update();
        return;
    }

    if (previousHoveredIndex != m_hoveredPointIndex) {
        update();
    }

    QOpenGLWidget::mouseMoveEvent(event);
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isPanning = false;

        // 如果沒有拖曳，就把這次 release 當成點擊選取。
        if (!m_hasDragged) {
            const QPoint releasePosition = event->position().toPoint();
            const int pointIndex = findPointAtScreenPosition(releasePosition);

            if (pointIndex >= 0 && pointIndex < m_points.size()) {
                m_selectedPointIndex = pointIndex;
                const HeritagePoint &point = m_points[pointIndex];

                // OpenGL canvas 只回報 site id；完整 metadata 由 MainWindow / Repository 查詢。
                emit heritageSiteSelected(point.siteId);
                update();
            }
        }
    }

    QOpenGLWidget::mouseReleaseEvent(event);
}

void MapCanvas::wheelEvent(QWheelEvent *event)
{
    // Qt6 wheel event 使用 position() 取得滑鼠位置。
    const QPoint wheelPosition = event->position().toPoint();

    // 以滑鼠所在位置為縮放中心，這比單純中心縮放更像地圖工具。
    const QVector2D worldBeforeZoom = screenToWorld(wheelPosition);

    const float wheelSteps = static_cast<float>(event->angleDelta().y()) / 120.0f;
    const float zoomFactor = std::pow(1.15f, wheelSteps);
    m_zoom = std::clamp(m_zoom * zoomFactor, kMinZoom, kMaxZoom);

    const QVector2D worldAfterZoom = screenToWorld(wheelPosition);
    m_cameraCenter += worldBeforeZoom - worldAfterZoom;

    emit viewChanged(m_zoom, m_cameraCenter.x(), m_cameraCenter.y());
    update();
}

bool MapCanvas::createShaderProgram()
{
    // 建立 shader program；失敗時直接回傳 false，避免後續 OpenGL 呼叫爆炸。
    if (!m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShaderSource)) {
        return false;
    }

    if (!m_shaderProgram.addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShaderSource)) {
        return false;
    }

    if (!m_shaderProgram.link()) {
        return false;
    }

    return true;
}

void MapCanvas::updateVertexLayout()
{
    // VAO 記錄 vertex attribute layout。
    m_vao.bind();
    m_vbo.bind();
    m_shaderProgram.bind();

    // 每個 vertex：x, y, r, g, b, a，共 6 個 float。
    constexpr int stride = 6 * sizeof(float);

    m_shaderProgram.enableAttributeArray(0);
    m_shaderProgram.setAttributeBuffer(0, GL_FLOAT, 0, 2, stride);

    m_shaderProgram.enableAttributeArray(1);
    m_shaderProgram.setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 4, stride);

    m_shaderProgram.release();
    m_vbo.release();
    m_vao.release();
}

void MapCanvas::drawGrid()
{
    const QRectF visibleRect = visibleWorldRect();
    const float spacing = chooseGridSpacing();

    const float minX = std::floor(static_cast<float>(visibleRect.left()) / spacing) * spacing - spacing;
    const float maxX = std::ceil(static_cast<float>(visibleRect.right()) / spacing) * spacing + spacing;
    const float minY = std::floor(static_cast<float>(visibleRect.top()) / spacing) * spacing - spacing;
    const float maxY = std::ceil(static_cast<float>(visibleRect.bottom()) / spacing) * spacing + spacing;

    QVector<float> vertices;

    const int verticalLineCount = static_cast<int>((maxX - minX) / spacing) + 1;
    const int horizontalLineCount = static_cast<int>((maxY - minY) / spacing) + 1;
    vertices.reserve((verticalLineCount + horizontalLineCount) * 2 * 6);

    const QColor gridColor(45, 55, 72);
    const QColor axisColor(80, 95, 125);

    // 垂直線。
    for (float x = minX; x <= maxX; x += spacing) {
        const QColor lineColor = std::abs(x) < spacing * 0.5f ? axisColor : gridColor;

        appendVertex(vertices, x, minY, lineColor);
        appendVertex(vertices, x, maxY, lineColor);
    }

    // 水平線。
    for (float y = minY; y <= maxY; y += spacing) {
        const QColor lineColor = std::abs(y) < spacing * 0.5f ? axisColor : gridColor;

        appendVertex(vertices, minX, y, lineColor);
        appendVertex(vertices, maxX, y, lineColor);
    }

    uploadAndDraw(vertices, GL_LINES, 1.0f, false);
}

void MapCanvas::drawPoints()
{
    QVector<float> vertices;
    vertices.reserve(m_points.size() * 6);

    for (int index = 0; index < m_points.size(); ++index) {
        const HeritagePoint &point = m_points[index];

        if (!isPointVisible(point)) {
            continue;
        }

        // Hover 的點用亮一點的顏色，保留互動提示。
        QColor color = point.color;
        if (index == m_hoveredPointIndex) {
            color = color.lighter(135);
        }

        // Phase 6：套用 Heritage Sites layer opacity。
        color.setAlphaF(m_layerState.heritageSitesOpacity);

        appendVertex(vertices, point.position.x(), point.position.y(), color);
    }

    uploadAndDraw(vertices, GL_POINTS, 13.0f, true);
}

void MapCanvas::drawSelectedPoint()
{
    if (m_selectedPointIndex < 0 || m_selectedPointIndex >= m_points.size()) {
        return;
    }

    const HeritagePoint &point = m_points[m_selectedPointIndex];

    // 如果目前 layer 把該點隱藏，selection ring 也不要畫出來。
    if (!isPointVisible(point)) {
        return;
    }

    // 先畫一個較大的白色光暈；之後可改成真正 selection ring。
    QColor selectionColor(255, 255, 255);
    selectionColor.setAlphaF(0.85f);

    QVector<float> vertices;
    appendVertex(vertices, point.position.x(), point.position.y(), selectionColor);
    uploadAndDraw(vertices, GL_POINTS, 24.0f, true);
}

void MapCanvas::uploadAndDraw(const QVector<float> &vertices,
                              GLenum primitiveType,
                              float pointSize,
                              bool circularPoint)
{
    if (vertices.isEmpty() || !m_shaderProgram.isLinked()) {
        return;
    }

    m_shaderProgram.bind();
    m_vao.bind();
    m_vbo.bind();

    // 直接更新 VBO；Phase 4/11 大量資料時再優化成批次 renderer。
    m_vbo.allocate(vertices.constData(), vertices.size() * static_cast<int>(sizeof(float)));

    m_shaderProgram.setUniformValue("uProjection", projectionMatrix());
    m_shaderProgram.setUniformValue("uPointSize", pointSize);
    m_shaderProgram.setUniformValue("uCircularPoint", circularPoint);

    glDrawArrays(primitiveType, 0, vertices.size() / 6);

    m_vbo.release();
    m_vao.release();
    m_shaderProgram.release();
}

QMatrix4x4 MapCanvas::projectionMatrix() const
{
    QMatrix4x4 matrix;

    const float safeWidth = std::max(1, width());
    const float safeHeight = std::max(1, height());
    const float halfWidth = safeWidth * 0.5f / m_zoom;
    const float halfHeight = safeHeight * 0.5f / m_zoom;

    // 使用正交投影：適合 2D GIS / map visualization。
    matrix.ortho(m_cameraCenter.x() - halfWidth,
                 m_cameraCenter.x() + halfWidth,
                 m_cameraCenter.y() - halfHeight,
                 m_cameraCenter.y() + halfHeight,
                 -1.0f,
                 1.0f);

    return matrix;
}

QRectF MapCanvas::visibleWorldRect() const
{
    const float safeWidth = static_cast<float>(std::max(1, width()));
    const float safeHeight = static_cast<float>(std::max(1, height()));
    const float halfWidth = safeWidth * 0.5f / m_zoom;
    const float halfHeight = safeHeight * 0.5f / m_zoom;

    return QRectF(m_cameraCenter.x() - halfWidth,
                  m_cameraCenter.y() - halfHeight,
                  halfWidth * 2.0f,
                  halfHeight * 2.0f);
}

float MapCanvas::chooseGridSpacing() const
{
    // 目標：grid 線在螢幕上大約每 90px 一條。
    // zoom 是 pixels per meter，所以 roughSpacing 是 meter。
    const float roughSpacing = kTargetGridPixelSpacing / std::max(kMinZoom, m_zoom);
    return niceGridSpacing(roughSpacing);
}

float MapCanvas::niceGridSpacing(float roughSpacing)
{
    if (roughSpacing <= 0.0f) {
        return 100.0f;
    }

    // 將任意 spacing 修成 1/2/5 * 10^n。
    // 例如 73 → 100，180 → 200，760 → 1000。
    const float exponent = std::floor(std::log10(roughSpacing));
    const float base = std::pow(10.0f, exponent);
    const float fraction = roughSpacing / base;

    if (fraction <= 1.0f) {
        return 1.0f * base;
    }

    if (fraction <= 2.0f) {
        return 2.0f * base;
    }

    if (fraction <= 5.0f) {
        return 5.0f * base;
    }

    return 10.0f * base;
}

QVector2D MapCanvas::screenToWorld(const QPoint &screenPoint) const
{
    const float safeWidth = std::max(1, width());
    const float safeHeight = std::max(1, height());
    const float halfWidth = safeWidth * 0.5f / m_zoom;
    const float halfHeight = safeHeight * 0.5f / m_zoom;

    const float normalizedX = (static_cast<float>(screenPoint.x()) / safeWidth) * 2.0f - 1.0f;
    const float normalizedY = 1.0f - (static_cast<float>(screenPoint.y()) / safeHeight) * 2.0f;

    return QVector2D(m_cameraCenter.x() + normalizedX * halfWidth,
                     m_cameraCenter.y() + normalizedY * halfHeight);
}

QPointF MapCanvas::worldToScreen(const QVector2D &worldPoint) const
{
    const float safeWidth = std::max(1, width());
    const float safeHeight = std::max(1, height());
    const float halfWidth = safeWidth * 0.5f / m_zoom;
    const float halfHeight = safeHeight * 0.5f / m_zoom;

    const float normalizedX = (worldPoint.x() - m_cameraCenter.x()) / halfWidth;
    const float normalizedY = (worldPoint.y() - m_cameraCenter.y()) / halfHeight;

    const float screenX = (normalizedX + 1.0f) * 0.5f * safeWidth;
    const float screenY = (1.0f - normalizedY) * 0.5f * safeHeight;

    return QPointF(screenX, screenY);
}

int MapCanvas::findPointAtScreenPosition(const QPoint &screenPoint) const
{
    int bestIndex = -1;
    float bestDistance = kPointPickRadiusPixels;

    // 使用 screen-space 距離做 picking，避免 zoom 後點擊範圍忽大忽小。
    for (int index = 0; index < m_points.size(); ++index) {
        const HeritagePoint &point = m_points[index];

        // Phase 6：隱藏圖層中的點不能被點選。
        if (!isPointVisible(point)) {
            continue;
        }

        const QPointF pointScreenPosition = worldToScreen(point.position);
        const float dx = static_cast<float>(pointScreenPosition.x()) - screenPoint.x();
        const float dy = static_cast<float>(pointScreenPosition.y()) - screenPoint.y();
        const float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

bool MapCanvas::isPointVisible(const HeritagePoint &point) const
{
    if (!m_layerState.isHeritageCategoryVisible(point.category)) {
        return false;
    }

    if (!m_timelineState.matchesYearRange(point.startYear, point.endYear)) {
        return false;
    }

    if (!m_searchState.matchesSite(point.site)) {
        return false;
    }

    return true;
}

void MapCanvas::appendVertex(QVector<float> &vertices,
                             float x,
                             float y,
                             const QColor &color)
{
    // 將 QColor 轉成 OpenGL 常用的 0~1 float。
    vertices.append(x);
    vertices.append(y);
    vertices.append(color.redF());
    vertices.append(color.greenF());
    vertices.append(color.blueF());
    vertices.append(color.alphaF());
}