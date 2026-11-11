// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
class QApplication;
class ImageCopyEditor;
class ImageExportUiChecks {
public:
    static bool run(QApplication &application, ImageCopyEditor *editor, const QString &prefix, QString *failure);
};
