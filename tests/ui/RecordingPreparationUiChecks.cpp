// SPDX-License-Identifier: GPL-3.0-only
// Synthetic confirmed region, real production Core restore path. No native recording.
#include "RecordingPreparationUiChecks.h"
#include "SpectacleCore.h"
#include "RecordingPreparation.h"
#include "CaptureModeModel.h"
#include "Gui/CaptureWindow.h"
#include "Gui/ViewerWindow.h"
#include "Gui/SelectionEditor.h"
#include "Gui/RecordingCountdownWindow.h"
#include "Gui/InlineMessageModel.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QDebug>
#include <QGuiApplication>
#include <QQuickItem>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QtTest/QTest>

namespace {
// Tests only the Core readiness preguard. It is never connected as a working
// recording adapter, and must never receive a start request.
class NotReadyVideoPlatform final : public VideoPlatform
{
public:
    bool startCalled = false;
    RecordingModes supportedRecordingModes() const override { return NoRecordingModes; }
    Formats supportedFormats() const override { return NoFormat; }
    bool isRecordingAudio() const override { return false; }
    bool readyForRecording() const override { return false; }
    void startRecording(const QUrl &, RecordingMode, const QVariantMap &, bool) override { startCalled = true; }
    void finishRecording() override {}
};
}

bool RecordingPreparationUiChecks::run(QApplication &application, const QString &path, QString *failure)
{
    auto core = SpectacleCore::instance();
    auto reject = [failure](const QString &message) { if (failure) *failure = message; return false; };
    if (!core || !ViewerWindow::instance()) return reject(QStringLiteral("Core/viewer unavailable"));
    auto platform = core->videoPlatform();
    if (platform->supportedRecordingModes() != VideoPlatform::NoRecordingModes)
        return reject(QStringLiteral("Requires real Null video platform; refusing native capture"));
    const QSize initialViewerSize = ViewerWindow::instance()->size();
    if (qEnvironmentVariableIsSet("NKSHOT_TEST_RECORDING_PREPARATION_VIDEO")) {
        const QString source = QStringLiteral("/logs/playback062.mp4");
        if (!QFileInfo::exists(source)) return reject(QStringLiteral("Synthetic video fixture missing"));
        core->setCurrentVideo(QUrl::fromLocalFile(source));
        core->setVideoMode(true);
        core->initViewerWindow(ViewerWindow::Viewer);
        // This is fixture setup only: changing the launch-only viewer to synthetic
        // video must start from a usable editor, not the new view's toolbar minimum.
        ViewerWindow::instance()->resize(initialViewerSize.expandedTo(QSize(640, 480)));
        ViewerWindow::instance()->showNormal();
    }
    const auto contentVisible = [&] {
        auto viewer = ViewerWindow::instance();
        if (!viewer || !viewer->rootObject()) return false;
        if (viewer->mode() == ViewerWindow::Dialog) return true;
        auto workbench = viewer->rootObject()->findChild<QQuickItem *>(QStringLiteral("nkshotWorkbench"));
        if (!workbench || !workbench->isVisible() || workbench->width() < 120 || workbench->height() < 120) return false;
        auto loaded = qvariant_cast<QObject *>(workbench->property("item"));
        if (!loaded) return false;
        if (core->videoMode()) {
            auto player = loaded->findChild<QObject *>(QStringLiteral("nkshotMediaPlayer"));
            return player && player->property("hasVideo").toBool() && player->property("duration").toLongLong() > 0;
        }
        return !core->annotationDocument()->baseImage().isNull();
    };
    if (!QTest::qWaitFor(contentVisible, 5000))
        return reject(QStringLiteral("Initial viewer lacks visible image/video workbench; refusing toolbar-only baseline"));
    const QRect originalGeometry = ViewerWindow::instance()->geometry();
    auto document = core->annotationDocument();
    const QImage original = document->baseImage();
    const auto depth = document->undoStackDepth();
    const auto currentVideo = core->currentVideo();
    const bool videoMode = core->videoMode();
    const auto viewerMode = ViewerWindow::instance()->mode();
    const QString clipboard = application.clipboard()->text();
    // Temporarily substitute only the readiness boundary. Keep the connected real
    // platform alive and restore it before processing any events.
    {
        const auto viewer = ViewerWindow::instance();
        const auto windows = CaptureWindow::instances();
        const bool restoreActive = core->m_recordingRestoreActive;
        const int messages = InlineMessageModel::instance()->rowCount();
        auto realPlatform = std::move(core->m_videoPlatform);
        auto notReady = std::make_unique<NotReadyVideoPlatform>();
        auto probe = notReady.get();
        core->m_videoPlatform = std::move(notReady);
        core->startRecording(VideoPlatform::Region, false, 3);
        const bool untouched = !probe->startCalled && core->videoMode() == videoMode
            && ViewerWindow::instance() == viewer && CaptureWindow::instances() == windows
            && core->m_recordingRestoreActive == restoreActive
            && document->baseImage() == original && document->undoStackDepth() == depth
            && core->currentVideo() == currentVideo;
        core->m_videoPlatform = std::move(realPlatform);
        // The readiness explanation is expected, not an image-capture failure.
        if (InlineMessageModel::instance()->rowCount() == messages + 1)
            InlineMessageModel::instance()->pop();
        if (!untouched) return reject(QStringLiteral("Core not-ready preguard mutated state or called adapter"));
    }
    QSignalSpy imageTaken(core->imagePlatform(), &ImagePlatform::newScreenshotTaken);
    QSignalSpy imageCroppable(core->imagePlatform(), &ImagePlatform::newCroppableScreenshotTaken);
    QSignalSpy imageFailed(core->imagePlatform(), &ImagePlatform::newScreenshotFailed);
    const int initialMessages = InlineMessageModel::instance()->rowCount();
    // baseImage() is not a Qt property in the actual KQuickImageEditor API.
    // Observe its canvas/image metadata signals, and compare full QImage pixels
    // plus history below (a repaint alone can be caused by recreating a view).
    QSignalSpy imageSizeChanged(document, &AnnotationDocument::imageSizeChanged);
    QSignalSpy imageDprChanged(document, &AnnotationDocument::imageDprChanged);
    QSignalSpy canvasChanged(document, &AnnotationDocument::canvasRectChanged);
    auto preparation = platform->preparation();
    const auto cleanup = qScopeGuard([&] { core->cancelPendingRecording(); core->cancelScreenshot(); });
    auto unchanged = [&] {
        return document->baseImage() == original && document->undoStackDepth() == depth
            && core->currentVideo() == currentVideo && core->videoMode() == videoMode
            && application.clipboard()->text() == clipboard
            && ViewerWindow::instance() && ViewerWindow::instance()->mode() == viewerMode
            && ViewerWindow::instance()->isVisible() && CaptureWindow::instances().isEmpty()
            && ViewerWindow::instance()->size() == originalGeometry.size() && contentVisible()
            && imageTaken.isEmpty() && imageCroppable.isEmpty() && imageFailed.isEmpty()
            && imageSizeChanged.isEmpty() && imageDprChanged.isEmpty() && canvasChanged.isEmpty()
            && InlineMessageModel::instance()->rowCount() == initialMessages;
    };
    auto begin = [&](int delay) -> bool {
        core->snapshotRecordingPreparation();
        core->setVideoMode(true);
        core->m_lastRecordingMode = VideoPlatform::Region;
        const auto token = preparation->begin(delay);
        Q_EMIT platform->regionRequested(token);
        if (!QTest::qWaitFor([&] { return !CaptureWindow::instances().isEmpty(); }, 1500)) return false;
        QTest::qWait(100);
        // Inject only the selection result, not platform capabilities or desktop pixels.
        Q_EMIT SelectionEditor::instance()->accepted(QRectF(40, 40, 240, 160), ExportManager::Actions{});
        if (!QTest::qWaitFor([&] { return core->m_pendingRegionToken == 0; }, 1500)) return false;
        return preparation->targetReady(token);
    };
    auto countdown = [&]() -> RecordingCountdownWindow * { return core->m_recordingCountdown.get(); };
    auto waitCountdown = [&] {
        return QTest::qWaitFor([&] { return countdown() && countdown()->isVisible()
            && countdown()->phase() == QStringLiteral("countdown"); }, 1500);
    };
    auto restored = [&] {
        const bool ok = QTest::qWaitFor([&] { return preparation->phase() == NkShot::RecordingPreparation::Idle && unchanged(); }, 1500);
        if (!ok) {
            auto viewer = ViewerWindow::instance();
            qWarning() << "Preparation restore diagnostic"
                << "phase" << preparation->phase() << "videoMode" << core->videoMode() << "expected" << videoMode
                << "viewer" << viewer << "mode" << (viewer ? int(viewer->mode()) : -1) << "expected" << int(viewerMode)
                << "geometry" << (viewer ? viewer->geometry() : QRect()) << "originalGeometry" << originalGeometry
                << "contentVisible" << contentVisible()
                << "visible" << (viewer && viewer->isVisible()) << "captureWindows" << CaptureWindow::instances().size()
                << "imageEqual" << (document->baseImage() == original)
                << "undoDepth" << document->undoStackDepth() << "expected" << depth
                << "videoEqual" << (core->currentVideo() == currentVideo)
                << "clipboardEqual" << (application.clipboard()->text() == clipboard)
                << "taken/croppable/failed" << imageTaken.size() << imageCroppable.size() << imageFailed.size()
                << "imageSize/dpr/canvas" << imageSizeChanged.size() << imageDprChanged.size() << canvasChanged.size()
                << "messages" << InlineMessageModel::instance()->rowCount() << "expected" << initialMessages
                << "countdownVisible" << (countdown() && countdown()->isVisible())
                << "focusWindow" << QGuiApplication::focusWindow()
                << "countdownActive" << (countdown() && countdown()->isActive());
        }
        return ok;
    };
    if (!begin(3000) || !waitCountdown()) return reject(QStringLiteral("Confirmed synthetic region did not show real countdown"));
    if (countdown()->remainingSeconds() < 1 || countdown()->remainingSeconds() > 3)
        return reject(QStringLiteral("Controller seconds outside requested range"));
    QSignalSpy canceled(countdown(), &RecordingCountdownWindow::cancelRequested);
    qInfo() << "Countdown pre-frame dimensions" << countdown()->size()
        << "root" << countdown()->rootObject()->size();
    if (!QTest::qWaitFor([&] {
            return countdown()->rootObject()->size() == QSizeF(countdown()->size());
        }, 1000)) return reject(QStringLiteral("Countdown root never filled the window after resize"));
    QSignalSpy countdownFrame(countdown(), &QQuickWindow::frameSwapped);
    countdown()->update();
    if (!QTest::qWaitFor([&] { return !countdownFrame.isEmpty(); }, 1000))
        return reject(QStringLiteral("Countdown did not render a frame at its final size"));
    const QImage countdownImage = countdown()->grabWindow();
    const auto background = qvariant_cast<QObject *>(countdown()->rootObject()->property("background"));
    const QColor expectedBackground = background ? background->property("color").value<QColor>() : QColor();
    const bool completeBackground = !countdownImage.isNull() && expectedBackground.isValid()
        && countdownImage.pixelColor(2, countdownImage.height() - 3) == expectedBackground
        && countdownImage.pixelColor(countdownImage.width() - 3, countdownImage.height() - 3) == expectedBackground;
    qInfo() << "Countdown post-frame dimensions" << countdown()->size()
        << "root" << countdown()->rootObject()->size() << "image" << countdownImage.size()
        << "expectedBackground" << expectedBackground << "completeBackground" << completeBackground;
    if (!completeBackground) return reject(QStringLiteral("Countdown bottom background does not cover the complete window"));
    if (!countdownImage.save(path + QStringLiteral(".countdown.png")))
        return reject(QStringLiteral("Could not save synthetic countdown screenshot"));
    auto button = countdown()->rootObject()->findChild<QQuickItem *>(QStringLiteral("nkshotRecordingCountdownCancel"));
    if (!button || !button->isVisible() || !button->isEnabled()) return reject(QStringLiteral("Cancel not reachable"));
    QTest::mouseClick(countdown(), Qt::LeftButton, Qt::NoModifier,
        button->mapToScene(QPointF(button->width()/2, button->height()/2)).toPoint());
    if (!restored() || canceled.size() != 1) return reject(QStringLiteral("Button cancellation did not restore original viewer/document"));
    countdown()->cancel();
    if (canceled.size() != 1) return reject(QStringLiteral("Cancellation was not idempotent"));
    // Wait for a real scene frame after the size/content contract, not resize here.
    QSignalSpy restoredFrame(ViewerWindow::instance(), &QQuickWindow::frameSwapped);
    ViewerWindow::instance()->update();
    if (!QTest::qWaitFor([&] { return !restoredFrame.isEmpty(); }, 1500))
        return reject(QStringLiteral("Restored viewer did not render a frame"));
    if (!ViewerWindow::instance()->grabWindow().save(path + QStringLiteral(".cancelled.png")))
        return reject(QStringLiteral("Could not save restored viewer screenshot"));
    if (!begin(3000) || !waitCountdown()) return reject(QStringLiteral("Repeat countdown failed"));
    // Shortcut dispatch needs actual window activation, unlike direct mouse
    // delivery. requestActivate() on show is asynchronous even offscreen.
    countdown()->requestActivate();
    if (!QTest::qWaitForWindowActive(countdown(), 1000))
        return reject(QStringLiteral("Countdown could not obtain keyboard activation for Escape check"));
    const auto beforeEscape = canceled.size();
    QTest::keyClick(countdown(), Qt::Key_Escape);
    qInfo() << "Preparation Escape dispatch" << "cancelSignalsBefore" << beforeEscape
        << "after" << canceled.size() << "focusWindow" << QGuiApplication::focusWindow();
    if (!restored()) return reject(QStringLiteral("Escape did not restore prior mode"));
    if (!begin(3000) || !waitCountdown()) return reject(QStringLiteral("Close setup failed"));
    countdown()->close();
    if (!restored()) return reject(QStringLiteral("Window close did not restore prior mode"));
    const int beforeStarting = canceled.size();
    QSignalSpy zeroVisibility(countdown(), &QWindow::visibleChanged);
    if (!begin(0)) return reject(QStringLiteral("Zero-delay target setup failed"));
    if (!QTest::qWaitFor([&] { return preparation->phase() == NkShot::RecordingPreparation::Starting; }, 1000))
        return reject(QStringLiteral("Zero delay did not enter Starting"));
    if (countdown()->isVisible() || !zeroVisibility.isEmpty() || canceled.size() != beforeStarting)
        return reject(QStringLiteral("Starting did not dismiss countdown silently"));
    core->cancelPendingRecording();
    if (!restored()) return reject(QStringLiteral("Starting cancellation did not restore prior mode"));
    // A queued global selection signal must retain the identity at emission.
    // Cancel and replace without delivering events: no direct provide(oldToken)
    // shortcut here, so the real SelectionEditor -> Core connection is exercised.
    const auto queueOldAcceptance = [&] {
        core->snapshotRecordingPreparation();
        core->setVideoMode(true);
        core->m_lastRecordingMode = VideoPlatform::Region;
        const auto token = preparation->begin(3000);
        Q_EMIT platform->regionRequested(token);
        Q_EMIT SelectionEditor::instance()->accepted(QRectF(40, 40, 240, 160), ExportManager::Actions{});
        core->cancelPendingRecording();
    };
    queueOldAcceptance();
    core->snapshotRecordingPreparation();
    core->setVideoMode(true);
    core->m_lastRecordingMode = VideoPlatform::Region;
    const auto replacement = preparation->begin(3000);
    Q_EMIT platform->regionRequested(replacement);
    QTest::qWait(100);
    if (preparation->phase() != NkShot::RecordingPreparation::Selecting
        || core->m_pendingRegionToken != replacement
        || document->baseImage() != original || document->undoStackDepth() != depth
        || core->currentVideo() != currentVideo)
        return reject(QStringLiteral("Queued old acceptance consumed replacement region or changed document"));
    core->cancelPendingRecording();
    if (!restored()) return reject(QStringLiteral("Replacement region cancellation failed"));
    queueOldAcceptance();
    core->takeNewScreenshot(int(CaptureModeModel::AllScreens), 99000, false, true, true);
    QTest::qWait(100);
    if (preparation->phase() != NkShot::RecordingPreparation::Idle
        || core->m_pendingRegionToken != 0
        || document->baseImage() != original || document->undoStackDepth() != depth
        || core->currentVideo() != currentVideo)
        return reject(QStringLiteral("Queued old acceptance affected replacement screenshot"));
    core->cancelScreenshot();
    if (!QTest::qWaitFor(unchanged, 1500)) return reject(QStringLiteral("Queued acceptance screenshot recovery failed"));
    if (!begin(3000) || !waitCountdown()) return reject(QStringLiteral("Cross-flow countdown setup failed"));
    core->takeNewScreenshot(int(CaptureModeModel::AllScreens), 99000, false, true, true);
    if (preparation->phase() != NkShot::RecordingPreparation::Idle || countdown()->isVisible())
        return reject(QStringLiteral("Screenshot did not invalidate pending recording"));
    core->cancelScreenshot();
    if (!QTest::qWaitFor(unchanged, 1500)) return reject(QStringLiteral("Screenshot interruption changed source/history/return mode"));
    return true;
}
