// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
class QApplication;
class QQuickWindow;
class QObject;
class FreeCropUiChecks {
public:
    static bool run(QApplication &, QQuickWindow *, QObject *job, const QString &evidence, QString *failure);
    static bool checkLocked(QQuickWindow *, QObject *job, QString *failure);
};
