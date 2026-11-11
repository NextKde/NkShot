// SPDX-License-Identifier: GPL-3.0-only
#pragma once
class QApplication;
class QString;
class ScrollSessionUiChecks
{
public:
    static bool run(QApplication &, const QString &screenshot, QString *failure);
};
