// SPDX-License-Identifier: GPL-3.0-only
#include "ImageExport.h"
#include <QColorSpace>
#include <QFileInfo>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>

namespace NkShot::Imaging {
static QByteArray codec(ImageExportFormat format)
{
    switch (format) {
    case ImageExportFormat::Jpeg: return "jpeg";
    case ImageExportFormat::WebpLossless: return "webp";
    }
    return {};
}
bool imageExportAvailable(ImageExportFormat format)
{
    const auto name = codec(format);
    return !name.isEmpty() && QImageWriter::supportedImageFormats().contains(name);
}
bool saveImageCopy(const QImage &source, const QString &path, ImageExportFormat format,
                   int quality, QString *error)
{
    if (error) error->clear();
    const auto fail = [error](const QString &message) { if (error) *error = message; return false; };
    if (source.isNull() || source.width() > 8192 || source.height() > 32000
        || qint64(source.width()) * source.height() > 40000000)
        return fail(QStringLiteral("Image must be nonempty, at most 8192 × 32000 pixels and 40 megapixels."));
    const auto name = codec(format);
    if (name.isEmpty()) return fail(QStringLiteral("Unsupported image export format."));
    if (quality < 1 || quality > 100)
        return fail(QStringLiteral("Image quality must be between 1 and 100."));
    const QFileInfo destination(path);
    const auto suffix = destination.suffix().toLower();
    if (path.isEmpty() || !destination.isAbsolute()
        || (format == ImageExportFormat::Jpeg ? suffix != QStringLiteral("jpg") && suffix != QStringLiteral("jpeg") : suffix != QStringLiteral("webp")))
        return fail(QStringLiteral("Choose an absolute local path with the matching .jpg/.jpeg or .webp extension."));
    if (destination.isSymLink() || (destination.exists() && !destination.isFile()))
        return fail(QStringLiteral("The destination must be a regular file, not a link or directory."));
    if (!imageExportAvailable(format))
        return fail(QStringLiteral("The requested Qt image encoder is not installed."));
    QImage image = source.colorSpace().isValid() ? source.convertedToColorSpace(QColorSpace::SRgb) : source;
    if (image.isNull()) return fail(QStringLiteral("Could not convert the image to sRGB."));
    // Metadata setters detach shared QImage data; never mutate the caller's image.
    image.setDevicePixelRatio(1);
    image.setColorSpace(QColorSpace::SRgb);
    if (format == ImageExportFormat::Jpeg) {
        QImage flattened(image.size(), QImage::Format_RGB32);
        if (flattened.isNull()) return fail(QStringLiteral("Not enough memory to flatten the image."));
        flattened.setColorSpace(QColorSpace::SRgb);
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(0, 0, image);
        painter.end();
        image = flattened;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return fail(file.errorString());
    QImageWriter writer(&file, name);
    writer.setQuality(format == ImageExportFormat::Jpeg ? quality : 100);
    if (!writer.write(image)) {
        file.cancelWriting();
        return fail(QStringLiteral("Image encoding failed: %1").arg(writer.errorString()));
    }
    if (!file.commit()) return fail(QStringLiteral("Could not save the image copy: %1").arg(file.errorString()));
    return true;
}
}
