// SPDX-License-Identifier: GPL-3.0-only
#pragma once
class QJsonObject;
// Runs the real VideoPlatformWayland orchestration with explicit fake providers.
// Does not establish native compositor/PipeWire acceptance.
bool runRecordingAdapterChecks(QJsonObject *report);
