#include "FileMonitor.h"

#ifdef _WIN32
#include "FileMonitorWindows.h"
#elif defined(__linux__)
#include "FileMonitorLinux.h"
#endif

std::unique_ptr<IFileMonitor> createFileMonitor()
{
#ifdef _WIN32
    return std::make_unique<FileMonitorWindows>();
#elif defined(__linux__)
    return std::make_unique<FileMonitorLinux>();
#else
    return nullptr;
#endif
}