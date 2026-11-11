/*
    SPDX-FileCopyrightText: 2023 Aleix Pol Gonzalez <aleixpol@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "VideoPlatformWayland.h"
#include "RecordingOutputPath.h"
#include "ExportManager.h"
#include "Platforms/VideoPlatform.h"
#include "screencasting.h"
#include "settings.h"
#include <KLocalizedString>
#include <KMemoryInfo>
#include <QGuiApplication>
#include <QWindow>
#include <QScreen>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <sys/stat.h>

using namespace Qt::StringLiterals;

using Format = VideoPlatform::Format;
using Formats = VideoPlatform::Formats;
using Encoder = PipeWireBaseEncodedStream::Encoder;

static const auto screenKey = u"screen"_s;
static const auto windowIdKey = u"uuid"_s;
static const auto rectKey = u"rect"_s;
static const auto xKey = u"x"_s;
static const auto yKey = u"y"_s;
static const auto widthKey = u"width"_s;
static const auto heightKey = u"height"_s;
static const auto captionKey = u"caption"_s;
static const auto desktopFileKey = u"desktopFile"_s;

static constexpr inline int frameBytes(const QSize &frameSize)
{
    // It is currently not possible to do 10 or 12 bit per color channel
    // recording so we always use 4 bytes (8 bits * 4 channels).
    return std::max(0, frameSize.width() * frameSize.height() * 4);
}

static inline int availableFrames(int frameBytes){
    KMemoryInfo info;
    if (info.isNull() || frameBytes <= 0) {
        // The default max frame buffer is 50.
        return 50;
    }
    // Reduce the amount of available memory we try to use to 75% because we
    // really don't want to freeze the user's computer.
    // The minimum max frame buffer is 3. It's used automatically when the buffer
    // is less than 3, but that sends out warning messages, so we explicitly set
    // 3 to prevent warning spam.
    return std::max(3.0, 0.75 * info.availablePhysical() / frameBytes);
}

VideoPlatform::Format VideoPlatformWayland::formatForEncoder(Encoder encoder) const
{
    switch (encoder) {
    case Encoder::VP9: return WebM_VP9;
    case Encoder::H264Main: return MP4_H264;
    case Encoder::H264Baseline: return MP4_H264;
    case Encoder::WebP: return WebP;
    case Encoder::Gif: return Gif;
    default: return NoFormat;
    }
}

PipeWireBaseEncodedStream::Encoder VideoPlatformWayland::encoderForFormat(Format format) const
{
    const auto encoders = m_recorder ? m_recorder->suggestedEncoders() : QList<Encoder>{};
    if (format == WebM_VP9 && encoders.contains(Encoder::VP9)) {
        return Encoder::VP9;
    }
    if (format == MP4_H264) {
        if (encoders.contains(Encoder::H264Main)) {
            return Encoder::H264Main;
        }
        if (encoders.contains(Encoder::H264Baseline)) {
            return Encoder::H264Baseline;
        }
    }
    if (format == WebP && encoders.contains(Encoder::WebP)) {
        return Encoder::WebP;
    }
    if (format == Gif && encoders.contains(Encoder::Gif)) {
        return Encoder::Gif;
    }
    return Encoder::NoEncoder;
}

static void minimizeIfWindowsIntersect(const QRectF &rect) {
    if (rect.isEmpty()) {
        return;
    }
    const auto &windows = qGuiApp->allWindows();
    for (auto window : windows) {
        if (rect.intersects(window->frameGeometry())
            && window->isVisible() && window->visibility() != QWindow::Minimized) {
            if (window->visibility() == QWindow::FullScreen) {
                window->setVisible(false);
            }
            window->showMinimized();
        }
    }
}

VideoPlatformWayland::VideoPlatformWayland(QObject *parent)
    : VideoPlatformWayland(NkShot::makeNativeRecordingScreencast(), NkShot::makeNativeRecordingRecorder, parent)
{
}

VideoPlatformWayland::VideoPlatformWayland(NkShot::RecordingScreencast *screencast,
    std::function<std::unique_ptr<NkShot::RecordingRecorder>()> recorderFactory, QObject *parent, int startTimeoutMs)
    : VideoPlatform(parent), m_screencasting(screencast), m_recorderFactory(std::move(recorderFactory))
{
    m_screencasting->setParent(this);
    const auto capabilitiesChanged=[this] {
        Q_EMIT supportedRecordingModesChanged();
        Q_EMIT supportedFormatsChanged();
        if (!m_screencasting->isAvailable() && preparation()->phase()!=NkShot::RecordingPreparation::Idle
            && preparation()->phase()!=NkShot::RecordingPreparation::Recording)
            failRecording(i18nc("@info", "The recording service is no longer available."));
    };
    connect(m_screencasting,&NkShot::RecordingScreencast::initialized,this,capabilitiesChanged);
    connect(m_screencasting,&NkShot::RecordingScreencast::removed,this,capabilitiesChanged);
    connect(m_screencasting,&NkShot::RecordingScreencast::sourcesChanged,this,capabilitiesChanged);
    QMetaObject::invokeMethod(this, &VideoPlatformWayland::initialize, Qt::QueuedConnection);
    m_startWatchdog.setSingleShot(true);
    connect(&m_startWatchdog,&QTimer::timeout,this,[this] {
        if (preparation()->phase()==NkShot::RecordingPreparation::Starting)
            failRecording(i18nc("@info", "Recording did not start in time. Select the target and try again."));
    });
    connect(preparation(),&NkShot::RecordingPreparation::readyToStart,this,[this,startTimeoutMs](quint64 token) {
        // Core observes the preceding phase change and hides the countdown first.
        QMetaObject::invokeMethod(this,[this,token,startTimeoutMs] {
            if (!preparation()->isCurrent(token) || preparation()->phase()!=NkShot::RecordingPreparation::Starting) return;
            m_startWatchdog.start(qMax(1,startTimeoutMs));
            commitRecording(token);
        },Qt::QueuedConnection);
    });
    const auto finishPreparation=[this] {
        m_startWatchdog.stop();
        preparation()->fail(m_requestToken);
        for(const auto &connection:std::as_const(m_targetConnections)) disconnect(connection);
        m_targetConnections.clear();
    };
    connect(this,&VideoPlatform::recordingFailed,this,finishPreparation);
    connect(this,&VideoPlatform::recordingCanceled,this,finishPreparation);
    auto ownerWatcher=new QDBusServiceWatcher(QStringLiteral("org.kde.KWin"),QDBusConnection::sessionBus(),QDBusServiceWatcher::WatchForOwnerChange,this);
    connect(ownerWatcher,&QDBusServiceWatcher::serviceOwnerChanged,this,[this](const QString &,const QString &,const QString &) {
        if(preparation()->phase()!=NkShot::RecordingPreparation::Idle)
            failRecording(i18nc("@info", "The compositor changed. Select the recording target again."));
    });
}

VideoPlatformWayland::~VideoPlatformWayland()
{
    // Member destructors can emit recorder signals; disconnect before the lease dies.
    if(m_recorder) m_recorder->disconnect(this);
    const bool pending=preparation()->phase()!=NkShot::RecordingPreparation::Recording
        && (preparation()->phase()!=NkShot::RecordingPreparation::Idle || m_discardPreparedOutput);
    // Native recorder destruction stops and joins its producer before output cleanup.
    m_recorder.reset();
    if(pending && m_ownedPreparedOutput) m_ownedPreparedOutput->cleanup();
    m_streamLease.retire();m_memoryTimer.stop();
}

void VideoPlatformWayland::initialize()
{
    m_recorder = m_recorderFactory();
    // Exactly one state/error subscription per recorder, never one per attempt.
    const auto generation=++m_recorderGeneration;
    const auto recorder=m_recorder.get();
    connect(recorder, &NkShot::RecordingRecorder::errorFound, this, [this,generation,recorder](const QString &message) {
        if(generation==m_recorderGeneration && m_recorder.get()==recorder) failRecording(message);
    });
    connect(recorder, &NkShot::RecordingRecorder::stateChanged, this, [this,generation,recorder] {
        if(generation!=m_recorderGeneration || m_recorder.get()!=recorder) return;
        const auto state = m_recorder->state();
        if (state == PipeWireRecord::Idle) {
            // Idle retires this producer generation immediately. Queued errors
            // from it must not cancel a later target-selection/countdown while
            // the next fresh recorder is waiting for commit.
            ++m_recorderGeneration;
            m_startGuard.recorderIdleObserved(m_startGuard.token());
            if(m_discardPreparedOutput) discardOwnedPreparedOutput();
            else if(m_ownedPreparedOutput) m_ownedPreparedOutput->release();
            m_explicitOutputReservation.reset();
            m_memoryTimer.stop();m_startPending=false;m_resetRecorder=true;
            m_streamLease.retire();
            Q_EMIT readyForRecordingChanged();
            if (m_attemptFailed) {
                setRecordingState(RecordingState::NotRecording);
                return;
            }
            if (recordingState() == RecordingState::Recording || recordingState() == RecordingState::Rendering) {
                const auto output=QUrl::fromLocalFile(m_recorder->output());
                setRecordingState(RecordingState::Finished);
                preparation()->finish(m_requestToken);
                Q_EMIT recordingSaved(output);
            }
        } else if (!m_attemptFailed && !m_stopRequested && state == PipeWireRecord::Recording) {
            m_startGuard.recordingObserved(m_startGuard.token());
            if (!preparation()->markRecording(m_requestToken)) return;
            m_startWatchdog.stop();
            m_startPending=false;
            m_memoryTimer.start(5000, Qt::CoarseTimer, this);
            setRecordingMode(m_requestedMode);
            setRecordingState(RecordingState::Recording);
        } else if (!m_attemptFailed && state == PipeWireRecord::Rendering) {
            m_memoryTimer.stop();
            setRecordingState(RecordingState::Rendering);
        }
    });
    Q_EMIT supportedRecordingModesChanged();
    Q_EMIT supportedFormatsChanged();
}

VideoPlatform::RecordingModes VideoPlatformWayland::supportedRecordingModes() const
{
    if (m_screencasting->isAvailable() && m_recorder)
        return Screen | Window | Region;
    else
        return {};
}

VideoPlatform::Formats VideoPlatformWayland::supportedFormats() const
{
    Formats formats;
    if (m_screencasting->isAvailable() && m_recorder) {
        const auto encoders = m_recorder->suggestedEncoders();
        for (auto encoder : encoders) {
            formats |= formatForEncoder(encoder);
        }
    }
    return formats;
}

bool VideoPlatformWayland::isRecordingAudio() const
{
    return m_recorder && (m_recorder->recordMicrophone() || m_recorder->recordSystemAudio());
}

void VideoPlatformWayland::startRecording(const QUrl &fileUrl, RecordingMode recordingMode, const QVariantMap &options, bool includePointer)
{
    using Prep = NkShot::RecordingPreparation;
    if (!m_startGuard.allowsNewAttempt() || preparation()->phase() != Prep::Idle || m_startPending
        || (m_recorder && m_recorder->state() != PipeWireRecord::Idle)) return;
    if (recordingMode == NoRecordingModes || !supportedRecordingModes().testFlag(recordingMode)) {
        Q_EMIT recordingFailed(i18nc("@info", "The requested recording mode is unavailable."));
        return;
    }
    if (!fileUrl.isEmpty() && !fileUrl.isLocalFile()) {
        Q_EMIT recordingFailed(i18nc("@info", "Recording requires a local output file."));
        return;
    }
    const int seconds = options.value(QStringLiteral("startDelay"), 0).toInt();
    if (seconds != 0 && seconds != 3 && seconds != 5 && seconds != 10) {
        Q_EMIT recordingFailed(i18nc("@info", "Choose a recording delay of 0, 3, 5 or 10 seconds."));
        return;
    }
    m_ownedPreparedOutput.reset();m_explicitOutputReservation.reset();m_discardPreparedOutput=false;
    m_requestFile = fileUrl;
    m_requestedMode = recordingMode;
    m_requestPointer = includePointer;
    m_requestFormat = fileUrl.isValid() ? formatForPath(fileUrl.toLocalFile()) : static_cast<Format>(Settings::preferredVideoFormat());
    m_requestSystemAudio = Settings::videoRecordSystemAudio();
    m_requestMicrophone = Settings::videoRecordMicrophone();
    m_filenameTemplate = Settings::videoFilenameTemplate();
    m_saveLocation = Settings::videoSaveLocation();
    m_requestOptions.clear();m_requestScreen.clear();
    m_attemptFailed=false;m_stopRequested=false;
    if (encoderForFormat(m_requestFormat) == Encoder::NoEncoder) {
        Q_EMIT recordingFailed(i18nc("@info", "No supported encoder is available for this recording format."));
        return;
    }
    const auto token = preparation()->begin(seconds * 1000);
    if (!token || !preparation()->isCurrent(token)) return;
    m_requestToken = token;
    const QPointer<QScreen> suppliedScreen=options.value(screenKey).value<QScreen *>();
    const bool hadSuppliedScreen=bool(suppliedScreen);
    QMetaObject::invokeMethod(this, [this, token, options, suppliedScreen, hadSuppliedScreen] {
        if (!preparation()->isCurrent(token)) return;
        if(hadSuppliedScreen && !suppliedScreen) {failRecording(i18nc("@info", "The selected screen is no longer available."));return;}
        if ((m_requestedMode == Screen && !options.value(screenKey).value<QScreen *>())
            || (m_requestedMode == Window && options.value(windowIdKey).toString().isEmpty())
            || (m_requestedMode == Region && options.value(rectKey).toRectF().isEmpty())) {
            selectAndRecord(token);
        } else resolveTarget(token, options);
    }, Qt::QueuedConnection);
}

void VideoPlatformWayland::provideRecordingRegion(const QRectF &rect, quint64 token)
{
    if (m_requestedMode != Region || preparation()->phase() != NkShot::RecordingPreparation::Selecting
        || !preparation()->isCurrent(token) || rect.isEmpty()) return;
    resolveTarget(token, {{rectKey, rect}});
}

bool VideoPlatformWayland::targetStillValid() const
{
    if (!m_screencasting->isAvailable()) return false;
    if (m_requestedMode == Screen) return m_requestScreen && qGuiApp->screens().contains(m_requestScreen);
    if (m_requestedMode == Window) return !m_requestOptions.value(windowIdKey).toString().isEmpty();
    if (m_requestedMode == Region) {
        const auto rect=m_requestOptions.value(rectKey).toRectF();
        if (rect.isEmpty()) return false;
        for (auto screen : qGuiApp->screens()) if (rect.intersects(screen->geometry())) return true;
    }
    return false;
}

void VideoPlatformWayland::resolveTarget(quint64 token, const QVariantMap &options)
{
    if (!preparation()->isCurrent(token) || preparation()->phase()!=NkShot::RecordingPreparation::Selecting) return;
    m_requestOptions=options;
    m_requestScreen=options.value(screenKey).value<QScreen *>();
    if (!targetStillValid()) { failRecording(i18nc("@info", "The selected recording target is no longer available."));return; }
    for (const auto &connection : std::as_const(m_targetConnections)) disconnect(connection);
    m_targetConnections.clear();
    // A display topology change invalidates the prepared coordinates instead of
    // silently recording a different screen or region after the countdown.
    const auto invalidate=[this,token] {
        if (preparation()->isCurrent(token) && preparation()->phase()!=NkShot::RecordingPreparation::Recording)
            failRecording(i18nc("@info", "Display configuration changed. Select the recording target again."));
    };
    m_targetConnections << connect(qGuiApp,&QGuiApplication::screenRemoved,this,[invalidate](QScreen *) {invalidate();});
    m_targetConnections << connect(qGuiApp,&QGuiApplication::screenAdded,this,[invalidate](QScreen *) {invalidate();});
    for(auto screen:qGuiApp->screens()) m_targetConnections << connect(screen,&QScreen::geometryChanged,this,[invalidate](const QRect &) {invalidate();});
    preparation()->targetReady(token);
}

void VideoPlatformWayland::commitRecording(quint64 token)
{
    if (!preparation()->isCurrent(token) || preparation()->phase()!=NkShot::RecordingPreparation::Starting) return;
    if (!targetStillValid()) {failRecording(i18nc("@info", "The selected recording target is no longer available."));return;}
    const auto fileUrl=m_requestFile;
    const auto recordingMode=m_requestedMode;
    const auto options=m_requestOptions;
    const bool includePointer=m_requestPointer;
    // Start tasks run queued, outside a recorder signal's emission. A fresh
    // recorder cannot inherit producer-thread cleanup or errors from a prior attempt.
    if (m_resetRecorder) {
        m_recorder->disconnect(this);
        m_recorder.reset();
        initialize();
        m_resetRecorder=false;
    }
    m_streamLease.retire();
    m_attemptFailed=false;m_stopRequested=false;
    // No compositor stream can start against a previous attempt's file.
    m_recorder->setNodeId(0);
    m_recorder->setOutput(QString());
    // Validate and configure output BEFORE requesting a compositor stream.
    auto format = m_requestFormat;
    if (!fileUrl.isValid()) {
        ExportManager::instance()->updateTimestamp();
        const auto filename = ExportManager::formattedFilename(m_filenameTemplate,
                                                               ExportManager::instance()->timestamp(),
                                                               options[captionKey].toString(),
                                                               m_saveLocation);
        auto tempUrl = ExportManager::instance()->tempVideoUrl(filename,extensionForFormat(m_requestFormat),m_saveLocation);
        if (!tempUrl.isLocalFile()) {
            Q_EMIT recordingFailed(
                i18nc("@info:shell, %1 is the temporary URL", "Failed to record: Temporary file URL is not a local file (%1)", tempUrl.toString()));
            return;
        }
        const auto encoder = encoderForFormat(format);
        if (encoder == Encoder::NoEncoder) {
            Q_EMIT recordingFailed(i18nc("@info", "No supported encoder is available for this recording format."));
            return;
        }
        const auto privateDirectory=ExportManager::instance()->temporaryDir();
        if(!privateDirectory) { failRecording(i18nc("@info", "The temporary recording folder is not available."));return; }
        m_ownedPreparedOutput=std::make_unique<NkShot::OwnedRecordingOutput>();
        if(!m_ownedPreparedOutput->reserve(privateDirectory->path(),tempUrl.toLocalFile())) {
            const auto error=m_ownedPreparedOutput->errorString();m_ownedPreparedOutput.reset();
            failRecording(i18nc("@info", "Could not reserve a new temporary recording: %1",error));return;
        }
        m_recorder->setEncoder(encoder);
        m_recorder->setOutput(tempUrl.toLocalFile());
    } else {
        if (!fileUrl.isLocalFile()) {
            Q_EMIT recordingFailed(
                i18nc("@info:shell, %1 is the output file URL", "Failed to record: Output file URL is not a local file (%1)", fileUrl.toString()));
            return;
        }
        const auto localFile = fileUrl.toLocalFile();
        format = formatForPath(localFile);
        const auto encoder = encoderForFormat(format);
        if (encoder == Encoder::NoEncoder) {
            Q_EMIT recordingFailed(i18nc("@info", "No supported encoder is available for this recording format."));
            return;
        }
        if (QFileInfo::exists(localFile) || QFileInfo(localFile).isSymLink()) {
            failRecording(i18nc("@info", "The recording output already exists. Choose a new filename."));return;
        }
        if (!mkDirPath(fileUrl)) { preparation()->fail(token);return; }
        m_explicitOutputReservation=std::make_unique<QFile>(localFile);
        if(!m_explicitOutputReservation->open(QIODevice::WriteOnly|QIODevice::NewOnly)) {
            m_explicitOutputReservation.reset();
            failRecording(i18nc("@info", "Could not reserve the recording output. Choose a new writable filename."));return;
        }
        m_recorder->setEncoder(encoder);
        m_recorder->setOutput(localFile);
    }
    // The stored settings stay enabled when the checkboxes are disabled
    // for a format without audio support, don't forward them in that case.
    const bool audioSupported = formatSupportsAudio(format);
    m_recorder->setRecordSystemAudio(m_requestSystemAudio && audioSupported);
    m_recorder->setRecordMicrophone(m_requestMicrophone && audioSupported);

    Screencasting::CursorMode mode = includePointer ? Screencasting::CursorMode::Embedded : Screencasting::Hidden;
    NkShot::RecordingSource *stream = nullptr;

    switch (recordingMode) {
    case Screen: {
        auto screen = options.value(screenKey).value<QScreen *>();
        if (!screen) {
            failRecording(i18nc("@info", "The selected recording target is no longer available."));
            return;
        }
        Q_ASSERT(screen != nullptr);
        minimizeIfWindowsIntersect(screen->geometry());
        m_frameBytes = frameBytes(screen->size() * screen->devicePixelRatio());
        stream = m_screencasting->createOutputStream(screen, mode);
        break;
    }
    case Window: {
        auto windowId = options.value(windowIdKey).toString();
        if (windowId.isEmpty()) {
            failRecording(i18nc("@info", "The selected recording target is no longer available."));
            return;
        }
        Q_ASSERT(!windowId.isEmpty());
        // HACK: Window geometry from queryWindowInfo is from KWin's
        // Window::frameGeometry(), which may not be the same as
        // QWindow::frameGeometry() on Wayland. Hopefully this is good enough most
        // of the time.
        const QRectF windowRect{
            options[xKey].toDouble(),
            options[yKey].toDouble(),
            options[widthKey].toDouble(),
            options[heightKey].toDouble(),
        };
        const bool isSpectacle = options[desktopFileKey].toString() == qGuiApp->desktopFileName();
        if (!windowRect.isEmpty() && !isSpectacle) {
            minimizeIfWindowsIntersect(windowRect);
        }
        auto screen = qGuiApp->screenAt(windowRect.center().toPoint());
        m_frameBytes = frameBytes(windowRect.size().toSize() * (screen ? screen->devicePixelRatio() : 1));
        stream = m_screencasting->createWindowStream(windowId, mode);
        break;
    }
    case Region: {
        auto rect = options.value(rectKey).toRectF();
        if (rect.isEmpty()) {
            failRecording(i18nc("@info", "The selected recording target is no longer available."));
            return;
        }
        qreal scaling = 1;
        const auto screens = qGuiApp->screens();
        // Don't make the resolution larger than it needs to be.
        for (auto screen : screens) {
            if (rect.intersects(screen->geometry())) {
                scaling = std::max(scaling, screen->devicePixelRatio());
            }
        }
        // Round to pixels that fit within the original rect.
        // We do this to make it easier to keep the selection outline outside of
        // the recording.
        int x = std::ceil(rect.x());
        int y = std::ceil(rect.y());
        // We calculate size using floor(right) - x and floor(bottom) - y so that
        // the rect doesn't shift too much. Ensure size is at least 1x1 to keep
        // the final rect valid.
        int w = std::max(1.0, std::floor(rect.right()) - x);
        int h = std::max(1.0, std::floor(rect.bottom()) - y);
        m_frameBytes = frameBytes(QSize{w, h} * scaling);
        scaling = m_screencasting->isRegionAutoScaleSupported() ? 0 : scaling;
        stream = m_screencasting->createRegionStream({x, y, w, h}, scaling, mode);
        break;
    }
    default:
        break; // This shouldn't happen
    }

    if (!stream) {
        m_recorder->setOutput(QString());
        failRecording(i18nc("@info", "The selected capture source is unavailable."));
        return;
    }
    const auto generation=m_streamLease.adopt(stream);
    m_startPending=true;m_requestedMode=recordingMode;
    m_recorder->setMaxPendingFrames(availableFrames(m_frameBytes));
    m_recorder->setNodeId(0);
    connect(stream, &NkShot::RecordingSource::created, this, [this, stream, generation, token] {
        if (!m_streamLease.accepts(stream,generation) || m_attemptFailed || !preparation()->isCurrent(token)) return;
        if(m_ownedPreparedOutput && !m_ownedPreparedOutput->identityMatches()) {
            failRecording(i18nc("@info", "The temporary recording path changed. Recording was not started."));return;
        }
        if(m_explicitOutputReservation) {
            struct stat held{}, entry{};
            const auto path=QFile::encodeName(m_explicitOutputReservation->fileName());
            if(::fstat(m_explicitOutputReservation->handle(),&held)!=0 || ::lstat(path.constData(),&entry)!=0
                || !S_ISREG(entry.st_mode) || entry.st_nlink!=1 || held.st_dev!=entry.st_dev || held.st_ino!=entry.st_ino) {
                failRecording(i18nc("@info", "The recording output changed. Choose a new filename."));return;
            }
        }
        m_startPending=false;
        m_recorder->setNodeId(stream->nodeId());
        if (!m_recorder->output().isEmpty()) {
            if (!m_startGuard.markStartIssued()) return;
            m_recorder->start();
        }
        // Recording state/timing comes only from the recorder state signal.
    });
    connect(stream, &NkShot::RecordingSource::failed, this, [this,stream,generation](const QString &error) {
        if(m_streamLease.accepts(stream,generation)) failRecording(error);
    });
    connect(stream, &NkShot::RecordingSource::closed, this, [this,stream,generation] {
        if(m_streamLease.accepts(stream,generation))
            failRecording(i18nc("@info", "Recording stopped because its capture source became unavailable."));
    });
}

void VideoPlatformWayland::failRecording(const QString &message)
{
    if(m_attemptFailed) return;
    m_discardPreparedOutput=preparation()->phase()==NkShot::RecordingPreparation::Starting;
    m_attemptFailed=true;m_startPending=false;m_stopRequested=true;
    m_startGuard.cancel(m_startGuard.token());
    if(m_startGuard.outstanding()) m_streamLease.quiesce(); else m_streamLease.retire();
    m_memoryTimer.stop();
    setRecordingState(RecordingState::NotRecording);
    if(m_recorder) m_recorder->stop();
    if(!m_startGuard.outstanding()) discardOwnedPreparedOutput();
    Q_EMIT recordingFailed(message);
}

void VideoPlatformWayland::cancelPendingRecording()
{
    using Prep=NkShot::RecordingPreparation;
    if(preparation()->phase()!=Prep::Selecting && preparation()->phase()!=Prep::Countdown && preparation()->phase()!=Prep::Starting) return;
    const bool wasStarting=preparation()->phase()==Prep::Starting;
    m_discardPreparedOutput=wasStarting;
    m_attemptFailed=true;m_stopRequested=true;m_startPending=false;
    m_startWatchdog.stop();m_memoryTimer.stop();
    preparation()->cancel();
    m_startGuard.cancel(m_startGuard.token());
    if(m_startGuard.outstanding()) m_streamLease.quiesce(); else m_streamLease.retire();
    if(m_recorder && m_startGuard.outstanding()) m_recorder->stop();
    if(!m_startGuard.outstanding()) discardOwnedPreparedOutput();
    setRecordingState(RecordingState::NotRecording);
    Q_EMIT recordingCanceled(wasStarting ? i18nc("@info", "Recording canceled while starting.")
        : i18nc("@info", "Recording canceled before capture started."));
}

void VideoPlatformWayland::finishRecording()
{
    m_stopRequested=true;
    if (preparation()->phase()!=NkShot::RecordingPreparation::Recording
        && preparation()->phase()!=NkShot::RecordingPreparation::Idle) {
        cancelPendingRecording();
        return;
    }
    if (!m_recorder) {
        return;
    }
    if(m_startPending) {
        m_startPending=false;m_attemptFailed=true;
        m_streamLease.retire();
        setRecordingState(RecordingState::NotRecording);
        Q_EMIT recordingCanceled(i18nc("@info", "Recording canceled before capture started."));
        return;
    }
    // Suppress callbacks, but retain the actual source until encoder draining ends.
    m_streamLease.quiesce();
    m_recorder->stop();
}

void VideoPlatformWayland::timerEvent(QTimerEvent *event)
{
    VideoPlatform::timerEvent(event);
    if (event->timerId() == m_memoryTimer.timerId() && m_recorder) {
        m_recorder->setMaxPendingFrames(availableFrames(m_frameBytes));
    }
}

bool VideoPlatformWayland::mkDirPath(const QUrl &fileUrl)
{
    const auto result = NkShot::prepareRecordingOutputPath(fileUrl);
    if (result == NkShot::RecordingOutputError::None) return true;
    Q_EMIT recordingFailed(result == NkShot::RecordingOutputError::InvalidFile
        ? i18nc("@info", "Failed to record: The output must be a writable local file, not a folder.")
        : i18nc("@info", "Failed to record: The output folder could not be created or is not writable."));
    return false;
}

void VideoPlatformWayland::selectAndRecord(quint64 token)
{
    const auto recordingMode=m_requestedMode;
    if (recordingMode == Region) {
        Q_EMIT regionRequested(token);
        return;
    }

    // We should probably come up with a better way of choosing outputs. This should be okay for now. #FLW
    QDBusMessage message = QDBusMessage::createMethodCall(u"org.kde.KWin"_s,
                                                          u"/KWin"_s,
                                                          u"org.kde.KWin"_s,
                                                          u"queryWindowInfo"_s);

    QDBusPendingReply<QVariantMap> asyncReply = QDBusConnection::sessionBus().asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(asyncReply, this);
    auto onFinished = [this, recordingMode, token](QDBusPendingCallWatcher *self) {
        QDBusPendingReply<QVariantMap> reply = *self;
        self->deleteLater();
        if(!preparation()->isCurrent(token) || preparation()->phase()!=NkShot::RecordingPreparation::Selecting) return;
        if (!reply.isValid()) {
            const auto &error = self->error();
            if (error.name() == u"org.kde.KWin.Error.UserCancel"_s) {
                QString message;
                if (recordingMode == Screen) {
                    message = i18nc("@info:shell", "Screen recording canceled");
                } else {
                    message = i18nc("@info:shell", "Window recording canceled");
                }
                Q_EMIT recordingCanceled(message);
            } else {
                QString message;
                if (recordingMode == Screen) {
                    message = i18nc("@info:shell, %1 is an error message", "Failed to select screen: %1", error.message());
                } else {
                    message = i18nc("@info:shell, %1 is an error message", "Failed to select window: %1", error.message());
                }
                Q_EMIT recordingFailed(message);
            }
            return;
        }
        const auto &data = reply.value();
        QVariantMap options;
        if (recordingMode == Screen) {
            QPoint pos = QCursor::pos();
            // BUG: https://bugs.kde.org/show_bug.cgi?id=480599
            // On wayland, you can't always get the cursor position from QCursor::pos().
            // However, using selected window geometry can sometimes select the wrong screen if the
            // window is between screens. We'll need to come up with a better solution someday.
            if (pos.isNull()) {
                pos = {
                    data[xKey].toInt() + data[widthKey].toInt() / 2,
                    data[yKey].toInt() + data[heightKey].toInt() / 2};
            }
            const auto &screens = qGuiApp->screens();
            QScreen *screen = nullptr;
            for (auto s : screens) {
                if (s->geometry().contains(pos)) {
                    screen = s;
                    break;
                }
            }
            if (!screen) {
                Q_EMIT recordingFailed(i18nc("@info:shell", "Failed to select screen: No screen contained the mouse cursor position"));
                return;
            }
            options[screenKey] = QVariant::fromValue(screen);
        } else {
            const auto &windowId = data.value(windowIdKey).toString();
            if (windowId.isEmpty()) {
                Q_EMIT recordingFailed(i18nc("@info:shell", "Failed to select window: No window found"));
                return;
            }
            options = data;
        }
        resolveTarget(token, options);
    };
    connect(watcher, &QDBusPendingCallWatcher::finished, this, onFinished);
}

#include "moc_VideoPlatformWayland.cpp"

bool VideoPlatformWayland::readyForRecording() const
{
    return VideoPlatform::readyForRecording() && m_startGuard.allowsNewAttempt()
        && !m_startPending && m_recorder && m_recorder->state()==PipeWireRecord::Idle;
}

void VideoPlatformWayland::discardOwnedPreparedOutput()
{
    auto output=std::move(m_ownedPreparedOutput);
    const bool discard=std::exchange(m_discardPreparedOutput,false);
    if(discard && output) {
        const auto result=output->cleanup();
        if(result==NkShot::OwnedRecordingOutput::CleanupResult::Preserved
            || result==NkShot::OwnedRecordingOutput::CleanupResult::Failed) {
            // Report the old attempt without reentering its Idle/cleanup stack.
            // This signal is informational and never invalidates a newer attempt.
            QMetaObject::invokeMethod(this,[this] {
                Q_EMIT temporaryOutputCleanupWarning(i18nc("@info", "The temporary recording could not be removed safely. The file was left untouched."));
            },Qt::QueuedConnection);
        }
    }
}
