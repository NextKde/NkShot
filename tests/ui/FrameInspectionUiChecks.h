// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QUrl>
#include <functional>
class QApplication;
class ViewerWindow;
class QObject;
class FrameInspectionUiChecks {
public:
    static bool run(QApplication &,ViewerWindow *,QObject *recording,const QString &source,const QString &evidence,const std::function<void(const QUrl &)> &setTestSource,QString *failure);
};
