#include <QApplication>
#include "Version.h"
#include "ui/MainWindow.h"
#include "LaunchFiles.h"
#include <windows.h>

int main(int argc, char* argv[])
{
    const QStringList paths=vision::launchFiles(QString::fromWCharArray(GetCommandLineW()));
    QApplication app(argc, argv);
    QApplication::setApplicationName(vision::kName);
    QApplication::setApplicationVersion(vision::kVersion);
    QApplication::setStyle("Fusion");

    HANDLE running = CreateMutexW(nullptr, FALSE, L"Local\\vision-running");

    vision::MainWindow window;
    window.openPaths(paths);
    window.show();
    int result = app.exec();
    if (running) CloseHandle(running);
    return result;
}
