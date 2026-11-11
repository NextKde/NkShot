// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QString>
namespace NkShot::Imaging {
// Plain text only. A null result means empty, oversized or unsupported input.
QImage renderNoteCard(const QString &text,int style);
}
