// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
class QApplication;
class ImageCopyEditor;
class TextCoverUiChecks {
public:
    static bool run(QApplication &, ImageCopyEditor *, const QString &prefix, QString *failure);
};
