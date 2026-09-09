#pragma once
#include <QStringList>
#include <QFileInfo>
#include <QUrl>
#include <windows.h>
#include <shellapi.h>

namespace vision {
inline QStringList launchFiles(const QString& commandLine)
{
    int count=0;
    auto** args=CommandLineToArgvW(reinterpret_cast<LPCWSTR>(commandLine.utf16()),&count);
    QStringList paths;
    if(!args)return paths;
    for(int i=1;i<count;++i){
        QString path=QString::fromWCharArray(args[i]);
        if(path=="--")continue;
        const QUrl url(path);
        if(url.isLocalFile())path=url.toLocalFile();
        paths.append(QFileInfo(path).absoluteFilePath());
    }
    LocalFree(args);
    return paths;
}
}
