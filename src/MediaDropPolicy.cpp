// SPDX-License-Identifier: GPL-3.0-only
#include "MediaDropPolicy.h"
#include <QDir>
#include <QStringList>

namespace NkShot {
bool isSupportedMediaDrop(const QList<QUrl> &urls)
{
    if (urls.size() != 1) return false;
    const auto &url = urls.first();
    if (!url.isValid() || url.isEmpty() || !url.isLocalFile()
        || url.scheme() != QStringLiteral("file") || !url.host().isEmpty()
        || !url.userInfo().isEmpty() || url.port() != -1
        || url.hasQuery() || url.hasFragment()) return false;
    const auto path = url.toLocalFile();
    if (path.isEmpty() || !QDir::isAbsolutePath(path) || path.contains(QChar(u'\0'))
        || path.startsWith(QStringLiteral("//")) || path.endsWith(QChar(u'/'))) return false;
    const auto filename = path.mid(path.lastIndexOf(QChar(u'/')) + 1);
    const auto dot = filename.lastIndexOf(QChar(u'.'));
    if (dot < 0) return false;
    const auto suffix = filename.mid(dot + 1).toLower();
    static const QStringList supported{QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                       QStringLiteral("webp"), QStringLiteral("mp4"), QStringLiteral("webm")};
    return supported.contains(suffix);
}
}
