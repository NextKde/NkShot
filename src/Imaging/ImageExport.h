// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QString>

namespace NkShot::Imaging {
enum class ImageExportFormat { Jpeg, WebpLossless };
bool imageExportAvailable(ImageExportFormat format);
// Writes a separate, atomic sRGB sharing copy at the original physical size.
// Untagged input is assumed sRGB. JPEG flattens transparency onto white.
// WebP uses Qt's quality 100 lossless mode. Metadata preservation is not promised.
bool saveImageCopy(const QImage &source, const QString &path, ImageExportFormat format,
                   int quality, QString *error = nullptr);
}
