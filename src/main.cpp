#include "ui/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QSurfaceFormat>
#include <QTextStream>

namespace {

// 套用 QSS 深色樣式。
// UI 顯示文字維持英文；程式碼註解用中文說明重點。
void applyApplicationStyle(QApplication &app)
{
    QFile styleFile(":/styles/dark.qss");

    if (!styleFile.open(QFile::ReadOnly | QFile::Text)) {
        return;
    }

    QTextStream stream(&styleFile);
    app.setStyleSheet(stream.readAll());
}

} // namespace

int main(int argc, char *argv[])
{
    // Qt Widgets application 入口。
    QApplication app(argc, argv);

    // 必須在建立任何 QOpenGLWidget 前設定 OpenGL format。
    // HeritageMapStudio 標準使用 OpenGL 3.3 Core Profile。
    QSurfaceFormat glFormat;
    glFormat.setRenderableType(QSurfaceFormat::OpenGL);
    glFormat.setVersion(3, 3);
    glFormat.setProfile(QSurfaceFormat::CoreProfile);
    glFormat.setDepthBufferSize(24);
    glFormat.setStencilBufferSize(8);
    glFormat.setSamples(4);
    QSurfaceFormat::setDefaultFormat(glFormat);

    // 套用全域 UI 樣式。
    applyApplicationStyle(app);

    // 建立主視窗。
    MainWindow window;
    window.resize(1440, 900);
    window.show();

    return app.exec();
}