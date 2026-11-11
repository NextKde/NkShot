// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QList>
#include <QUrl>

namespace NkShot {
// Lexical acceptance only: no filesystem access or network requests. The actual
// open operation must still validate readability, file type and media contents.
bool isSupportedMediaDrop(const QList<QUrl> &urls);
}
