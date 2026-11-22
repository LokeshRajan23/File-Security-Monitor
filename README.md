# FileMonitorCPP
Advanced Real Time File Security Monitor written using C++

## How to run

1. Linux – VS Code WSL Ubuntu terminal

Linux monitoring
Navigate to project:
cd /mnt/d/Project/FileMonitorCPP


Configure (only when creating or resetting the build directory):
cmake -S . -B build-linux


Build / Compile:
cmake --build build-linux


Run:
./build-linux/FileMonitor


2. Windows – VS Code MSYS2 UCRT64 terminal

Native Windows monitoring
Navigate to project:
cd /d/Project/FileMonitorCPP


Configure (only when creating or resetting the build directory):
cmake -S . -B build-windows -G Ninja -DCMAKE_BUILD_TYPE=Release


Build / Compile:
cmake --build build-windows


Run:
./build-windows/FileMonitor.exe
