# HeritageMap Studio

A C++ Qt OpenGL desktop workstation for cultural heritage mapping, metadata exploration, and historical timeline visualization.

## Phase 1 Scope

This phase builds the application shell on Qt6:

- Qt Widgets `QMainWindow`
- Dock panels for project explorer, layers, metadata inspector, search, and logs
- `QOpenGLWidget` central map canvas
- OpenGL 3.3 shader pipeline
- Grid rendering
- Demo heritage points
- Mouse pan / wheel zoom
- Click selection and metadata inspector update
- Linux + VS Code + CMake setup

## Runtime Stack

- Qt 6.4+ (`Core`, `Gui`, `Widgets`, `OpenGL`, `OpenGLWidgets`, `Sql`)
- Qt6 SQLite driver (`QSQLITE`)
- SQLite 3
- Desktop OpenGL

## Linux Dependencies

Ubuntu / Debian:

```bash
sudo apt update
sudo apt install -y build-essential cmake gdb qt6-base-dev libqt6sql6-sqlite sqlite3 libsqlite3-dev libgl-dev libglx-dev libopengl-dev
```

Optional, if you prefer Ninja:

```bash
sudo apt install -y ninja-build
```

## Build and Run

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
./build/HeritageMapStudio
```

If you are inside a conda environment that exports a conda C++ toolchain, the
project automatically prefers `/usr/bin/c++` so Qt/OpenGL are resolved from the
system packages above.

With Ninja:

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/HeritageMapStudio
```

## VS Code Workflow

Recommended extensions:

- C/C++
- CMake Tools

Useful commands:

- `Ctrl+Shift+B`: build
- Run task: `Run HeritageMap Studio`
- Debug configuration: `Debug HeritageMap Studio`

## Next Phase

Phase 2 will add:

- SQLite database
- database migrations
- `HeritageSite` model
- repository layer
- table view backed by real data
