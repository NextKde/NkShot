// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
class ViewerWindow; class QVideoSink;
QString runVideoAnnotationsUiChecks(ViewerWindow *viewer, QVideoSink *sink, const QString &evidence);
