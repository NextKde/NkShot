/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#pragma once

#include "VideoPlatform.h"
#include "CaptureStreamLease.h"
#include "RecordingBackend.h"
#include "RecordingStartGuard.h"
#include <functional>
#include <PipeWireRecord>
#include <memory>
#include <QPointer>
#include <QTimer>

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
    VideoPlatformWayland(NkShot::RecordingScreencast *screencast,
        std::function<std::unique_ptr<NkShot::RecordingRecorder>()> recorderFactory, QObject *parent = nullptr, int startTimeoutMs = 30000);
    ~VideoPlatformWayland() override;

    RecordingModes supportedRecordingModes() const override;
    Formats supportedFormats() const override;
    bool isRecordingAudio() const override;

    void startRecording(const QUrl &fileUrl, RecordingMode recordingMode, const QVariantMap &options, bool includePointer) override;
    void finishRecording() override;
    void cancelPendingRecording() override;
    bool readyForRecording() const override;
    void provideRecordingRegion(const QRectF &rect, quint64 token) override;

    Format formatForEncoder(PipeWireBaseEncodedStream::Encoder encoder) const;
    PipeWireBaseEncodedStream::Encoder encoderForFormat(Format format) const;

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void initialize();
    void failRecording(const QString &message);
    bool mkDirPath(const QUrl &fileUrl);
    void selectAndRecord(quint64 token);
    void resolveTarget(quint64 token, const QVariantMap &options);
    void commitRecording(quint64 token);
    bool targetStillValid() const;
    QUrl m_requestFile;
    QVariantMap m_requestOptions;
    QPointer<QScreen> m_requestScreen;
    QString m_filenameTemplate;
    QUrl m_saveLocation;
    bool m_requestPointer = false;
    bool m_requestSystemAudio = false;
    bool m_requestMicrophone = false;
    Format m_requestFormat = NoFormat;
    quint64 m_requestToken = 0;
    QTimer m_startWatchdog;
    QList<QMetaObject::Connection> m_targetConnections;
    QString m_ownedPreparedOutput;
    bool m_discardPreparedOutput = false;
    void discardOwnedPreparedOutput();

    NkShot::RecordingScreencast *const m_screencasting;
    std::function<std::unique_ptr<NkShot::RecordingRecorder>()> m_recorderFactory;
    std::unique_ptr<NkShot::RecordingRecorder> m_recorder;
    NkShot::RecordingStartGuard m_startGuard;
    quint64 m_recorderGeneration = 0;
    NkShot::CaptureStreamLease m_streamLease{this};
    bool m_startPending = false;
    bool m_attemptFailed = false;
    bool m_stopRequested = false;
    bool m_resetRecorder = false;
    RecordingMode m_requestedMode = NoRecordingModes;
    int m_frameBytes;
    QBasicTimer m_memoryTimer;
};
