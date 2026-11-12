// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
class QApplication;
class RecordingAutoStopUiChecks {
public:
    static bool run(QApplication &, const QString &screenshot, QString *failure);
};
