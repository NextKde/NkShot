// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
#include <QStringList>
#include <QImage>
class QApplication;
class ViewerWindow;
class ImageCombineUiChecks {
public:
    static bool run(QApplication &,ViewerWindow *,const QStringList &paths,const QImage &expected,bool keepAll,bool horizontal,const QString &evidence,QString *failure);
    static bool checkError(ViewerWindow *,const QStringList &paths,const QString &expectedError,const QString &evidence,QString *failure);
};
