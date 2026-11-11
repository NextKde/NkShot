// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QUrl>
namespace NkShot::Imaging {
// Bounded local still-image decode; null means use a placeholder. No writes or
// network. Guards the opened file, not hostile concurrent parent-directory edits.
QImage mediaThumbnail(const QUrl &url, int edge = 160);
}
