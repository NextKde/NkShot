// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

namespace NkShot::Imaging
{

struct CodeScanResult {
    enum Status { Text, Binary, NotFound, InvalidImage };
    Status status = NotFound;
    QString text;
    QByteArray binary;
};

// Synchronous worker-thread helper. Scans a private image snapshot, limited to
// 20,000,000 physical pixels. Payloads are returned literally; this never opens
// links, uses the network, writes the clipboard, or changes application state.
[[nodiscard]] CodeScanResult scanImageCode(const QImage &image);

}
