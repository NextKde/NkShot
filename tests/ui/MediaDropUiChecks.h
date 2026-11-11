// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
class QApplication;
class ViewerWindow;
class MediaDropUiChecks {
public:
    static bool run(QApplication &, ViewerWindow *, const QString &prefix, QString *failure);
};
