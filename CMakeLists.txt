cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json")

# Conda toolchain variables can make desktop Qt/OpenGL discovery and linking
# use the conda sysroot instead of the system libraries installed by apt. This
# project is a native Linux Qt6 desktop app, so isolate it from those variables.
if (UNIX AND NOT APPLE AND DEFINED ENV{CONDA_PREFIX})
    set(ENV{CFLAGS} "")
    set(ENV{CXXFLAGS} "")
    set(ENV{CPPFLAGS} "")
    set(ENV{LDFLAGS} "")
    set(ENV{CMAKE_ARGS} "")
    set(ENV{CMAKE_PREFIX_PATH} "")

    if (NOT DEFINED CMAKE_CXX_COMPILER AND EXISTS "/usr/bin/c++")
        set(CMAKE_CXX_COMPILER "/usr/bin/c++" CACHE FILEPATH "System C++ compiler")
    endif()
endif()

if (UNIX AND NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "" CACHE STRING "Executable linker flags" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS "" CACHE STRING "Shared library linker flags" FORCE)
    set(CMAKE_MODULE_LINKER_FLAGS "" CACHE STRING "Module linker flags" FORCE)
endif()

if (UNIX
    AND NOT APPLE
    AND NOT DEFINED CMAKE_CXX_COMPILER
    AND EXISTS "/usr/bin/c++")
    set(CMAKE_CXX_COMPILER "/usr/bin/c++" CACHE FILEPATH "System C++ compiler")
    set(ENV{CXXFLAGS} "")
    set(ENV{CFLAGS} "")
endif()

# 專案名稱固定使用作品集正式名稱，之後所有 phase 都沿用這個名稱。
project(HeritageMapStudio VERSION 0.1.0 LANGUAGES CXX)

# 使用 C++17：Qt Widgets 桌面專案穩定、VS Code / Linux 工具鏈也容易支援。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 開啟 Qt 自動處理：
# AUTOMOC：自動處理 Q_OBJECT
# AUTORCC：自動編譯 .qrc resource 檔
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 專案固定使用 Qt6。Ubuntu 24.04 目前提供 Qt 6.4.2，這裡要求 6.4+。
find_package(Qt6 6.4 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGL
    OpenGLWidgets
    Sql
)

# OpenGL::GL 用來連結 Linux 桌面 OpenGL library。
find_package(OpenGL REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/database/MigrationManager.h
    src/database/MigrationManager.cpp
    src/models/HeritageSite.h
    src/models/HeritageSite.cpp
    src/repositories/HeritageSiteRepository.h
    src/repositories/HeritageSiteRepository.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/opengl/MapCanvas.h
    src/opengl/MapCanvas.cpp
    resources/resources.qrc
    src/services/CsvReader.h
    src/services/CsvReader.cpp
    src/services/ImportService.h
    src/services/ImportService.cpp

    src/ui/ImportCsvDialog.h
    src/ui/ImportCsvDialog.cpp
    src/services/CsvReader.h
    src/services/CsvReader.cpp
    src/services/ImportService.h
    src/services/ImportService.cpp

    src/utils/GeoCoordinateTransformer.h
    src/utils/GeoCoordinateTransformer.cpp

    src/ui/ImportCsvDialog.h
    src/ui/ImportCsvDialog.cpp
    src/ui/MetadataEditorPanel.h
    src/ui/MetadataEditorPanel.cpp
    src/models/MapLayerState.h
    src/models/MapLayerState.cpp

    src/ui/LayerPanel.h
    src/ui/LayerPanel.cpp
    src/models/TimelineFilterState.h
    src/models/TimelineFilterState.cpp

    src/ui/TimelinePanel.h
    src/ui/TimelinePanel.cpp

    src/models/SearchFilterState.h
    src/models/SearchFilterState.cpp

    src/ui/SearchPanel.h
    src/ui/SearchPanel.cpp
)

# 讓 include 可以使用 "ui/MainWindow.h"、"opengl/MapCanvas.h" 這類路徑。
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Qt Widgets 會帶到 Qt Gui/Core；這裡仍明確列出，讓依賴關係更清楚。
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        Qt6::Sql
        OpenGL::GL
)

# Linux / GCC / Clang 下開啟常用警告，提早抓出作品集專案中的壞味道。
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME}
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()
