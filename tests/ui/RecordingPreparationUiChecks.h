// SPDX-License-Identifier: GPL-3.0-only
#pragma once
class QApplication;
class QString;
class RecordingPreparationUiChecks
{
public:
    static bool run(QApplication &application, const QString &screenshotPath, QString *failure);
};
