/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "VideoPlatform.h"
#include "CaptureStreamLease.h"
#include <PipeWireRecord>
#include <memory>

class Screencasting;

/**
 * The VideoPlatformWayland class uses the org.kde.KWin.ScreenShot2 dbus interface
 * for taking screenshots of screens and windows.
 */
class VideoPlatformWayland final : public VideoPlatform
{
    Q_OBJECT

public:
    VideoPlatformWayland(QObject *parent = nullptr);
    ~VideoPlatformWayland() override;

    RecordingModes supportedRecordingModes() const override;
    Formats supportedFormats() const override;
    bool isRecordingAudio() const override;

    void startRecording(const QUrl &fileUrl, RecordingMode recordingMode, const QVariantMap &options, bool includePointer) override;
    void finishRecording() override;

    Format formatForEncoder(PipeWireBaseEncodedStream::Encoder encoder) const;
    PipeWireBaseEncodedStream::Encoder encoderForFormat(Format format) const;

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void initialize();
    void failRecording(const QString &message);
    bool mkDirPath(const QUrl &fileUrl);
    void selectAndRecord(const QUrl &fileUrl, RecordingMode recordingMode, bool includePointer);

    Screencasting *const m_screencasting;
    std::unique_ptr<PipeWireRecord> m_recorder;
    NkShot::CaptureStreamLease m_streamLease{this};
    quint64 m_startEpoch = 0;
    bool m_startPending = false;
    bool m_attemptFailed = false;
    bool m_stopRequested = false;
    bool m_resetRecorder = false;
    RecordingMode m_requestedMode = NoRecordingModes;
    int m_frameBytes;
    QBasicTimer m_memoryTimer;
};
