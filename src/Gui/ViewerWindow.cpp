/* This file is part of Spectacle, the KDE screenshot utility
 * SPDX-FileCopyrightText: 2015 Boudhayan Gupta <bgupta@kde.org>
 * SPDX-FileCopyrightText: 2019 David Redondo <kde@david-redondo.de>
 * SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ViewerWindow.h"
#include "LocalMediaLoader.h"
#include "RecentMediaModel.h"
#include "RecordingPreparation.h"
#include <QMessageBox>
#include <QProgressDialog>
#include <QVideoSink>
#include <QVideoFrame>
#include "ScrollCaptureWindow.h"
#include "PinnedImageWindow.h"
#include "ImageCopyEditor.h"
#include "QuickNoteWindow.h"
#include "ColorPickerWindow.h"
#include "OcrTextWindow.h"
#include "Imaging/ImageTransforms.h"
#include "Imaging/CodeScanner.h"
#include "OcrManager.h"
#include "Imaging/VerticalStitcher.h"
#include "Media/VideoClipJob.h"
#include <QFileDialog>
#include <QImageReader>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <QCollator>
#include <algorithm>
#include <KQuickImageEditor/AnnotationDocument>

#include "Config.h"
#include "Gui/ExportMenu.h"
#include "InlineMessageModel.h"
#include "SpectacleCore.h"

#include <KSharedConfig>
#include <KUrlMimeData>
#include <KWindowConfig>
#include <Kirigami/Platform/Units>

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDrag>
#include <QFile>
#include <QMimeData>
#include <QShortcut>
#include <QScreen>

using namespace Qt::StringLiterals;

ViewerWindow *ViewerWindow::s_viewerWindowInstance = nullptr;

ViewerWindow::ViewerWindow(Mode mode, QQmlEngine *engine, QWindow *parent)
    : SpectacleWindow(engine, parent)
    , m_mode(mode)
{
    s_viewerWindowInstance = this;
    s_isAnnotating = false;
    m_clipJob = new VideoClipJob(this);
    connect(m_clipJob, &VideoClipJob::exported, this, [](const QUrl &url) { NkShot::RecentMediaModel::instance()->remember(url); });
    m_mediaLoader = new NkShot::LocalMediaLoader(this);
    connect(m_mediaLoader, &NkShot::LocalMediaLoader::imageReady, this,
        [this](const QImage &image, const QUrl &source) { offerOpenedMedia(source, image, false); });
    connect(m_mediaLoader, &NkShot::LocalMediaLoader::videoReady, this,
        [this](const QUrl &source, qint64, QSize, bool) { offerOpenedMedia(source, {}, true); });
    connect(m_mediaLoader, &NkShot::LocalMediaLoader::changed, this, [this] {
        if (!m_mediaLoader->busy() && !m_mediaLoader->error().isEmpty()) {
            InlineMessageModel::instance()->push(InlineMessageModel::Error, m_mediaLoader->error());
            finishOpeningMedia();
        }
    });
    connect(SpectacleCore::instance()->videoPlatform(), &VideoPlatform::recordingStateChanged,
        this, &ViewerWindow::openingMediaChanged);
    connect(SpectacleCore::instance()->videoPlatform()->preparation(), &NkShot::RecordingPreparation::changed,
        this, &ViewerWindow::openingMediaChanged);
    connect(m_clipJob, &VideoClipJob::changed, this, &ViewerWindow::openingMediaChanged);
    connect(this, &ViewerWindow::choosingClipOutputChanged, this, &ViewerWindow::openingMediaChanged);
    new QShortcut(QKeySequence::Open, this, this, &ViewerWindow::openMedia);
    new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), this, this, [this] { editClipboardImage(); });
    connect(SpectacleCore::instance(), &SpectacleCore::currentVideoChanged, this, &ViewerWindow::cancelClipExport);
    connect(SpectacleCore::instance(), &SpectacleCore::videoModeChanged, this, &ViewerWindow::cancelClipExport);
    connect(SpectacleCore::instance()->videoPlatform(), &VideoPlatform::recordingStateChanged, this, [this] {
        if (SpectacleCore::instance()->videoPlatform()->isRecording()) cancelClipExport();
    });

    // Set up shortcuts. We won't need to access these again and the memory will be managed by Qt.
    new QShortcut(QKeySequence::Save, this, this, &ViewerWindow::save);
    new QShortcut(QKeySequence::SaveAs, this, this, &ViewerWindow::saveAs);
    new QShortcut(QKeySequence::Copy, this, this, &ViewerWindow::copyImage);
    new QShortcut(QKeySequence::Print, this, this, &ViewerWindow::showPrintDialog);

    m_context->setContextObject(this); // Must be before QML is initialized

    connect(ExportMenu::instance(), &ExportMenu::imageShared, this, [this](int error, const QString &message) {
        if (error == 1 || status() != QQuickView::Ready) {
            // error == 1 means the user cancelled the sharing
            return;
        }

        if (error) {
            auto text = i18nc("@info, %1 is the error message", "There was a problem sharing the image: %1", message);
            InlineMessageModel::instance()->push(InlineMessageModel::Error, text);
        }
    });

    if (mode == Viewer) {
        connect(OcrManager::instance(), &OcrManager::textRecognized, this,
            [this](const QString &text, const QStringList &, bool success) {
                if (!success || text.trimmed().isEmpty()) return;
                auto result = new OcrTextWindow(text, this->engine());
                if (result->status() != QQuickView::Ready) { delete result; return; }
                result->setScreen(screen());
                result->show();
            });
    }

    // set up QML
    setResizeMode(QQuickView::SizeRootObjectToView);
    setMode(mode); // sets source and other stuff based on mode.
    m_oldWindowStates = windowStates();

    // Restore window state from config (only for Viewer mode - the screenshot viewer window)
    if (mode == Viewer) {
        KConfigGroup configGroup(KSharedConfig::openConfig(), u"ViewerWindow"_s);
        KWindowConfig::restoreWindowSize(this, configGroup);
    }
}

QQuickWindow *ViewerWindow::openQuickNote()
{
    if (!m_quickNote) {
        for (auto window : QGuiApplication::allWindows()) {
            if (auto note = qobject_cast<QuickNoteWindow *>(window)) {
                m_quickNote = note;
                break;
            }
        }
    }
    if(!m_quickNote) {
        auto note=new QuickNoteWindow(engine());
        if(note->status()!=QQuickView::Ready) {delete note;return nullptr;}
        m_quickNote=note;
    }
    m_quickNote->setScreen(screen());
    m_quickNote->show();
    m_quickNote->setWindowStates(m_quickNote->windowStates().setFlag(Qt::WindowMinimized, false));
    m_quickNote->requestActivate();
    return m_quickNote;
}

QQuickWindow *ViewerWindow::pinImage()
{
    if (SpectacleCore::instance()->videoMode()) return nullptr;
    const auto image = SpectacleCore::instance()->annotationDocument()->renderToImage();
    if (image.isNull()) return nullptr;
    auto window = new PinnedImageWindow(image, engine());
    if (window->status() != QQuickView::Ready) { delete window; return nullptr; }
    window->setScreen(screen());
    window->setPosition(position() + QPoint(40, 40));
    window->show();
    return window;
}

QQuickWindow *ViewerWindow::pinVideoFrame(QVideoSink *sink, const QUrl &source)
{
    const auto core = SpectacleCore::instance();
    if (!canOpenMedia() || !core->videoMode() || source.isEmpty()
        || source != core->currentVideo() || !sink) return nullptr;
    const auto frame = sink->videoFrame();
    if (!frame.isValid() || frame.width() <= 0 || frame.height() <= 0
        || qint64(frame.width()) * frame.height() > 40'000'000) return nullptr;
    // Convert on explicit request, including on GPU playback; never screenshot
    // the letterboxed player or reuse a frame from a previously opened source.
    const auto image = frame.toImage();
    if (image.isNull()) {
        InlineMessageModel::instance()->push(InlineMessageModel::Error,
            i18nc("@info", "This frame could not be extracted. Pause playback and try again."));
        return nullptr;
    }
    auto window = new PinnedImageWindow(image.copy(), engine());
    if (window->status() != QQuickView::Ready) { delete window; return nullptr; }
    window->setTitle(i18nc("@title:window", "Video frame — NkShot"));
    window->setObjectName(QStringLiteral("nkshotVideoFramePin"));
    window->setScreen(screen());
    window->setPosition(position() + QPoint(40, 40));
    window->show();
    return window;
}

QSize ViewerWindow::imageSnapshotSize() const
{
    return SpectacleCore::instance()->annotationDocument()->renderToImage().size();
}
void ViewerWindow::resizeImageCopy(int width, int quarterTurns, bool mirror)
{
    createImageCopy([width, quarterTurns, mirror](const QImage &snapshot) { return NkShot::Imaging::orientedCopy(snapshot, quarterTurns, mirror, width); },
        i18nc("@title:window", "Image Copy — NkShot"),
        i18nc("@info", "The resized copy could not be created. Choose a smaller valid width. Your original and edits are unchanged."));
}
void ViewerWindow::beautifyImageCopy(int padding, int style, const QString &watermark, bool rounded)
{
    const NkShot::Imaging::PresentationOptions options{padding, NkShot::Imaging::PresentationStyle(style), watermark, rounded};
    createImageCopy([options](const QImage &snapshot) { return NkShot::Imaging::composePresentation(snapshot, options); },
        i18nc("@title:window", "Styled Copy — NkShot"),
        i18nc("@info", "The styled copy could not be created. Use a smaller image or less padding. Your original and edits are unchanged."));
}
void ViewerWindow::createImageCopy(std::function<QImage(const QImage &)> transform, const QString &title, const QString &failure)
{
    if (m_resizingImage || SpectacleCore::instance()->videoMode()) return;
    const auto snapshot = SpectacleCore::instance()->annotationDocument()->renderToImage();
    m_resizingImage = true;
    Q_EMIT resizingImageChanged();
    auto watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, title, failure] {
        const auto image = watcher->result();
        watcher->deleteLater();
        m_resizingImage = false;
        Q_EMIT resizingImageChanged();
        if (image.isNull()) {
            InlineMessageModel::instance()->push(InlineMessageModel::Warning,
                failure);
            return;
        }
        auto preview = new PinnedImageWindow(image, engine());
        if (preview->status() != QQuickView::Ready) {
            delete preview;
            InlineMessageModel::instance()->push(InlineMessageModel::Error,
                i18nc("@info", "The image preview could not be opened. Your original and edits are unchanged."));
            return;
        }
        preview->setTitle(title);
        preview->setScreen(screen());
        preview->show();
    });
    watcher->setFuture(QtConcurrent::run([snapshot, transform] { return transform(snapshot); }));
}

void ViewerWindow::combineImages(bool stackWithoutOverlap, bool horizontal)
{
    if (m_combiningImages) return;
    if (auto existing = qobject_cast<QFileDialog *>(m_combineDialog.data())) {
        existing->raise(); existing->activateWindow(); return;
    }
    auto dialog = new QFileDialog;
    m_combineDialog = dialog;
    dialog->setObjectName(QStringLiteral("nkshotCombineDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(horizontal
        ? (stackWithoutOverlap
            ? i18nc("@title:window", "Stack images — retain all content, left to right by filename")
            : i18nc("@title:window", "Combine overlapping screenshots — left to right by filename"))
        : stackWithoutOverlap
        ? i18nc("@title:window", "Stack images — retain all content, top to bottom by filename")
        : i18nc("@title:window", "Combine overlapping screenshots — top to bottom by filename"));
    dialog->setFileMode(QFileDialog::ExistingFiles);
    dialog->setNameFilter(i18nc("@item:inlistbox", "Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    dialog->winId();
    dialog->windowHandle()->setTransientParent(this);
    connect(dialog, &QFileDialog::accepted, this, [this, dialog, stackWithoutOverlap, horizontal] {
        auto paths = dialog->selectedFiles();
        if (paths.size() < 2 || paths.size() > 16) {
            InlineMessageModel::instance()->push(InlineMessageModel::Warning,
                horizontal ? i18nc("@info", "Choose 2–16 images. Number their filenames from left to right.")
                    : i18nc("@info", "Choose 2–16 images. Number their filenames from top to bottom."));
            return;
        }
        QCollator order(QLocale(QLocale::English, QLocale::UnitedStates));
        order.setNumericMode(true);
        std::sort(paths.begin(), paths.end(), [&order](const QString &a, const QString &b) {
            const auto compared = order.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName());
            return compared == 0 ? a < b : compared < 0;
        });
        m_combiningImages = true;
        Q_EMIT combiningImagesChanged();
        auto watcher = new QFutureWatcher<NkShot::Imaging::StitchResult>(this);
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, stackWithoutOverlap, horizontal] {
            const auto result = watcher->result();
            watcher->deleteLater();
            m_combiningImages = false;
            Q_EMIT combiningImagesChanged();
            if (result.error != NkShot::Imaging::StitchError::None) {
                using Error = NkShot::Imaging::StitchError;
                QString reason;
                switch (result.error) {
                case Error::InvalidInput:
                    reason = stackWithoutOverlap ? i18nc("@info", "An image could not be read. Choose intact image files.")
                        : horizontal ? i18nc("@info", "An image could not be read or is too narrow. Choose intact images wider than 24 pixels.")
                        : i18nc("@info", "An image could not be read or is too short. Choose intact images taller than 24 pixels."); break;
                case Error::IncompatibleFrames:
                    reason = horizontal ? i18nc("@info", "Images must have the same height, scale and color profile. Use screenshots from the same capture session.")
                        : i18nc("@info", "Images must have the same width, scale and color profile. Use screenshots from the same capture session."); break;
                case Error::NoOverlap:
                    reason = horizontal ? i18nc("@info", "No unchanged overlap was found. Number images left to right and retain at least 24 overlapping pixels. Prefer original PNG screenshots.")
                        : i18nc("@info", "No unchanged overlap was found. Number images top to bottom and retain at least 24 overlapping pixels. Prefer original PNG screenshots."); break;
                case Error::AmbiguousOverlap:
                    reason = i18nc("@info", "Repeated content makes the overlap ambiguous. Choose screenshots with more distinctive overlapping content."); break;
                case Error::NoProgress:
                    reason = i18nc("@info", "A repeated image adds no new content. Remove duplicate or fully overlapping screenshots."); break;
                case Error::TooLarge:
                    reason = horizontal ? i18nc("@info", "These images exceed the safe size limit. Use fewer or smaller images: 40 megapixels total, at most 32000 pixels wide and 8192 pixels tall.")
                        : i18nc("@info", "These images exceed the safe size limit. Use fewer or smaller images: 40 megapixels total, at most 8192 pixels wide and 32000 pixels tall."); break;
                case Error::None: break;
                }
                InlineMessageModel::instance()->push(InlineMessageModel::Warning,
                    reason + QLatin1Char(' ') + i18nc("@info", "No source files or edits were changed."));
                return;
            }
            auto preview = new PinnedImageWindow(result.image, engine());
            if (preview->status() != QQuickView::Ready) {
                delete preview;
                InlineMessageModel::instance()->push(InlineMessageModel::Error,
                    i18nc("@info", "The combined preview could not be opened. No source files or edits were changed."));
                return;
            }
            preview->setTitle(stackWithoutOverlap ? i18nc("@title:window", "Stacked Image — NkShot") : i18nc("@title:window", "Combined Image — NkShot"));
            preview->setScreen(screen());
            preview->show();
            InlineMessageModel::instance()->push(InlineMessageModel::Saved,
                i18nc("@info", "Combined image opened in a separate window. Copy or Save As there; the source images and current edit are unchanged."));
        });
        watcher->setFuture(QtConcurrent::run([paths, stackWithoutOverlap, horizontal] {
            QList<QImage> frames;
            qint64 pixels = 0;
            for (const auto &path : paths) {
                QImageReader reader(path);
                const auto size = reader.size();
                const qint64 count = qint64(size.width()) * size.height();
                if (!size.isValid() || count < 1)
                    return NkShot::Imaging::StitchResult{{}, {}, NkShot::Imaging::StitchError::InvalidInput};
                if (count > 40000000 - pixels || size.width() > (horizontal ? 32000 : 8192) || size.height() > (horizontal ? 8192 : 32000))
                    return NkShot::Imaging::StitchResult{{}, {}, NkShot::Imaging::StitchError::TooLarge};
                pixels += count;
                auto image = reader.read();
                if (image.isNull()) return NkShot::Imaging::StitchResult{{}, {}, NkShot::Imaging::StitchError::InvalidInput};
                frames.append(image);
            }
            if (horizontal)
                return stackWithoutOverlap ? NkShot::Imaging::stackHorizontal(frames) : NkShot::Imaging::stitchHorizontal(frames);
            return stackWithoutOverlap ? NkShot::Imaging::stackVertical(frames) : NkShot::Imaging::stitchVertical(frames);
        }));
    });
    dialog->open();
}

bool ViewerWindow::canOpenMedia() const
{
    return !m_openingMedia && SpectacleCore::instance()->canOpenMedia()
        && !m_clipJob->busy() && !choosingClipOutput()
        && !m_combiningImages && !m_resizingImage;
}

QQuickWindow *ViewerWindow::editClipboardImage()
{
    // Read only on this explicit action. Never monitor clipboard changes, follow
    // clipboard URLs, or replace the active screenshot/recording document.
    if(!canOpenMedia())return nullptr;
    const auto clipboard=QApplication::clipboard();
    const auto mime=clipboard->mimeData(QClipboard::Clipboard);
    if(!mime||!mime->hasImage()){
        InlineMessageModel::instance()->push(InlineMessageModel::InformationalType,
            i18nc("@info","Copy an image first, then choose Edit clipboard image. Text and file links are not imported."));
        return nullptr;
    }
    const auto image=clipboard->image(QClipboard::Clipboard);
    if(image.isNull()||image.width()>8192||image.height()>32000||qint64(image.width())*image.height()>40'000'000){
        InlineMessageModel::instance()->push(InlineMessageModel::Warning,
            i18nc("@info","The clipboard image could not be opened. Use an image up to 40 megapixels, 8192 pixels wide and 32000 pixels tall."));
        return nullptr;
    }
    auto editor=new ImageCopyEditor(image,engine());
    if(editor->status()!=QQuickView::Ready){
        delete editor;
        InlineMessageModel::instance()->push(InlineMessageModel::Error,
            i18nc("@info","The image editor could not be opened. Your clipboard and current document are unchanged."));
        return nullptr;
    }
    editor->setTitle(i18nc("@title:window","Edit clipboard image — NkShot"));
    editor->setScreen(screen());editor->show();editor->requestActivate();return editor;
}

void ViewerWindow::finishOpeningMedia()
{
    m_openingMedia = false;
    if (auto widget = qobject_cast<QWidget *>(m_openDialog.data())) {
        widget->hide();
        widget->deleteLater();
    }
    m_openDialog = nullptr;
    Q_EMIT openingMediaChanged();
}

void ViewerWindow::openMedia()
{
    if (!canOpenMedia()) {
        InlineMessageModel::instance()->push(InlineMessageModel::InformationalType,
            i18nc("@info", "Finish the current task before opening another file."));
        return;
    }
    m_openingMedia = true;
    m_openEpoch = SpectacleCore::instance()->mediaOpenEpoch();
    auto dialog = new QFileDialog();
    m_openDialog = dialog;
    dialog->setObjectName(QStringLiteral("nkshotOpenMediaDialog"));
    dialog->winId();
    dialog->windowHandle()->setTransientParent(this);
    dialog->setWindowTitle(i18nc("@title:window", "Open image or recording"));
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    dialog->setSupportedSchemes({QStringLiteral("file")});
    dialog->setNameFilters({
        i18nc("@item:inlistbox", "Supported images and short recordings (*.png *.jpg *.jpeg *.webp *.mp4 *.webm)"),
        i18nc("@item:inlistbox", "Images up to 40 megapixels (*.png *.jpg *.jpeg *.webp)"),
        i18nc("@item:inlistbox", "H.264 MP4 / VP9 WebM up to 10 minutes / 256 MiB (*.mp4 *.webm)")});
    dialog->setWindowModality(Qt::ApplicationModal);
    connect(this, &QObject::destroyed, dialog, &QObject::deleteLater);
    connect(dialog, &QFileDialog::finished, this, [this, dialog](int result) {
        const auto url = dialog->selectedUrls().value(0);
        dialog->deleteLater();
        m_openDialog = nullptr;
        if (result != QDialog::Accepted) { finishOpeningMedia(); return; }
        if (!SpectacleCore::instance()->canOpenMedia()
            || m_openEpoch != SpectacleCore::instance()->mediaOpenEpoch()) { finishOpeningMedia(); return; }
        loadOpenedMedia(url);
    });
    Q_EMIT openingMediaChanged();
    dialog->open();
}

QObject *ViewerWindow::recentMediaObject() const { return NkShot::RecentMediaModel::instance(); }
void ViewerWindow::openRecentMedia(const QUrl &url)
{
    if(!canOpenMedia())return;
    m_openingMedia=true;
    m_openEpoch=SpectacleCore::instance()->mediaOpenEpoch();
    Q_EMIT openingMediaChanged();
    loadOpenedMedia(url);
}
void ViewerWindow::loadOpenedMedia(const QUrl &url)
{
    auto progress = new QProgressDialog(i18nc("@info", "Checking local file…"),
        i18nc("@action", "Cancel"), 0, 0);
    progress->setObjectName(QStringLiteral("nkshotOpenMediaProgress"));
    progress->winId();
    progress->windowHandle()->setTransientParent(this);
    progress->setWindowModality(Qt::ApplicationModal);
    progress->setMinimumDuration(0);
    m_openDialog = progress;
    connect(this, &QObject::destroyed, progress, &QObject::deleteLater);
    connect(progress, &QProgressDialog::canceled, this, [this] {
        m_mediaLoader->cancel();
        finishOpeningMedia();
    });
    progress->show();
    m_mediaLoader->load(url);
}

void ViewerWindow::offerOpenedMedia(const QUrl &source, const QImage &image, bool video)
{
    if (auto progress = qobject_cast<QProgressDialog *>(m_openDialog.data())) {
        progress->hide();
        progress->deleteLater();
        m_openDialog = nullptr;
    }
    auto core = SpectacleCore::instance();
    if (!m_openingMedia || !core->canOpenMedia() || m_openEpoch != core->mediaOpenEpoch()
        || m_clipJob->busy() || choosingClipOutput()) { finishOpeningMedia(); return; }
    auto commit = [this, source, image, video] {
        auto core = SpectacleCore::instance();
        if (!m_mediaLoader->validatedSourceUnchanged()) {
            InlineMessageModel::instance()->push(InlineMessageModel::Error,
                i18nc("@info", "The selected file changed. Choose it again; your current content is unchanged."));
            finishOpeningMedia();
            return;
        }
        if (core->canOpenMedia() && m_openEpoch == core->mediaOpenEpoch()
            && !m_clipJob->busy() && !choosingClipOutput()) {
            finishOpeningMedia();
            if(core->commitOpenedMedia(source, image, video)) NkShot::RecentMediaModel::instance()->remember(source);
        } else finishOpeningMedia();
    };
    if (core->annotationDocument()->baseImage().isNull() && core->currentVideo().isEmpty()) {
        commit();
        return;
    }
    auto confirm = new QMessageBox(QMessageBox::Question,
        i18nc("@title:window", "Replace current content?"),
        core->videoMode()
            ? i18nc("@info", "Open a different file? The current recording remains on disk.")
            : i18nc("@info", "Opening this file replaces the current content and its edit history. Unsaved changes will be lost."),
        QMessageBox::Discard | QMessageBox::Cancel);
    confirm->setObjectName(QStringLiteral("nkshotReplaceMediaDialog"));
    confirm->winId();
    confirm->windowHandle()->setTransientParent(this);
    confirm->setDefaultButton(QMessageBox::Cancel);
    confirm->setWindowModality(Qt::ApplicationModal);
    m_openDialog = confirm;
    connect(this, &QObject::destroyed, confirm, &QObject::deleteLater);
    connect(confirm, &QMessageBox::finished, this, [this, confirm, commit](int result) {
        confirm->deleteLater();
        if (result == QMessageBox::Discard) commit();
        else finishOpeningMedia();
    });
    confirm->open();
}

ViewerWindow::~ViewerWindow()
{
    cancelClipExport();
    delete m_combineDialog;
    InlineMessageModel::instance()->clear();
    if (s_viewerWindowInstance == this) {
        s_viewerWindowInstance = nullptr;
    }
}

ViewerWindow::UniquePointer ViewerWindow::makeUnique(Mode mode, QQmlEngine *engine, QWindow *parent)
{
    return UniquePointer(new ViewerWindow(mode, engine, parent), [](ViewerWindow *window) {
        s_viewerWindowInstance = nullptr;
        deleter(window);
    });
}

ViewerWindow *ViewerWindow::instance()
{
    return s_viewerWindowInstance;
}

void ViewerWindow::setMode(ViewerWindow::Mode mode)
{
    if (mode == Dialog) {
        setBackgroundColorRole(QPalette::Window);
        QVariantMap initialProperties = {// Set the parent in initialProperties to avoid having
                                         // the parent and window be null in Component.onCompleted
                                         {u"parent"_s, QVariant::fromValue(contentItem())}};
        setSource(QUrl("%1/Gui/DialogPage.qml"_L1.arg(SPECTACLE_QML_PATH)), initialProperties);
        auto rootItem = rootObject();
        if (!rootItem) {
            return;
        }
        // The setup page scrolls its options instead of changing the window's
        // size whenever a group expands or platform capabilities change.
        setMaximumSize(QSize(16777215, 16777215));
        updateMinimumSize();
        connect(rootItem, SIGNAL(minimumWidthChanged()), this, SLOT(updateMinimumSize()));
        connect(rootItem, SIGNAL(minimumHeightChanged()), this, SLOT(updateMinimumSize()));
        resize(QSize(480, 440).expandedTo(minimumSize()).boundedTo(screen()->availableGeometry().size()));
    } else if (mode == Viewer) {
        setBackgroundColorRole(QPalette::Base);
        QVariantMap initialProperties = {// Set the parent in initialProperties to avoid having
                                         // the parent and window be null in Component.onCompleted
                                         {u"parent"_s, QVariant::fromValue(contentItem())}};
        setSource(QUrl("%1/Gui/ViewerPage.qml"_L1.arg(SPECTACLE_QML_PATH)), initialProperties);
        auto rootItem = rootObject();
        if (!rootItem) {
            return;
        }
        updateMinimumSize();
        connect(rootItem, SIGNAL(minimumWidthChanged()), this, SLOT(updateMinimumSize()));
        connect(rootItem, SIGNAL(minimumHeightChanged()), this, SLOT(updateMinimumSize()));
    }
}

void ViewerWindow::updateColor()
{
    setColor(qGuiApp->palette().color(m_backgroundColorRole));
}

void ViewerWindow::setBackgroundColorRole(QPalette::ColorRole role)
{
    m_backgroundColorRole = role;
    updateColor();
}

void ViewerWindow::updateMinimumSize()
{
    if (auto rootItem = rootObject()) {
        const QSize size = {qRound(rootItem->property("minimumWidth").toReal()), qRound(rootItem->property("minimumHeight").toReal())};
        // Resizing should be automatic, but sometimes a manual resizing is needed
        // to fix window sizing/graphical glitches after a minimum size change.
        if (size.width() > width()) {
            setWidth(size.width());
        }
        if (size.height() > height()) {
            setHeight(size.height());
        }
        setMinimumSize(size);
    }
}

void ViewerWindow::startDrag()
{
    if (m_mode == Dialog) {
        return;
    }

    SpectacleCore::instance()->syncExportImage();
    auto exportManager = ExportManager::instance();
    const auto &image = exportManager->image();

    const QUrl tempFile = SpectacleCore::instance()->videoMode() ? SpectacleCore::instance()->currentVideo() : exportManager->tempSave();
    if (!tempFile.isValid()) {
        return;
    }

    auto mimeData = new QMimeData;
    mimeData->setUrls(QList<QUrl>{tempFile});
    // "application/x-kde-suggestedfilename" is handled by KIO/PasteJob.
    // It is only used when QMimeData::formats() is empty or when the user is
    // prompted to set a filename for the content after drag & drop or paste.
    // When QMimeData::formats() is empty, a dialog for picking the data format
    // is supposed to appear.
    // It's likely that users will never see the data format dialog with Spectacle.
    mimeData->setData(u"application/x-kde-suggestedfilename"_s, QFile::encodeName(tempFile.fileName()));
    KUrlMimeData::exportUrlsToPortal(mimeData);

    auto dragHandler = new QDrag(this);
    dragHandler->setMimeData(mimeData);

    if (SpectacleCore::instance()->videoMode()) {
        auto units = engine()->singletonInstance<Kirigami::Platform::Units *>("org.kde.kirigami.platform", "Units");
        auto iconSize = units->iconSizes()->large();
        dragHandler->setPixmap(QIcon::fromTheme(u"video-x-generic"_s).pixmap(iconSize, iconSize));
    } else {
        QSize size = image.size();
        QPixmap pixmap = QPixmap::fromImage(image);
        // TODO: use the composed pixmap with annotations instead
        if (size.width() > 256 || size.height() > 256) {
            dragHandler->setPixmap(pixmap.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            dragHandler->setPixmap(pixmap);
        }
    }
    dragHandler->exec(Qt::CopyAction);
}

bool ViewerWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Hide || event->type() == QEvent::Close
        || (event->type() == QEvent::WindowStateChange && windowStates().testFlag(Qt::WindowMinimized))) {
        ++m_codeScanEpoch;
        cancelClipExport();
    }
    if (event->type() == QEvent::ApplicationPaletteChange) {
        updateColor();
    }
    // Save window state when the window closes
    if (m_mode == Viewer && (event->type() == QEvent::Hide || event->type() == QEvent::Close)) {
        KConfigGroup configGroup(KSharedConfig::openConfig(), u"ViewerWindow"_s);
        KWindowConfig::saveWindowSize(this, configGroup);
    }
    return SpectacleWindow::event(event);
}

void ViewerWindow::resizeEvent(QResizeEvent *event)
{
    SpectacleWindow::resizeEvent(event);
    if (auto rootItem = rootObject()) {
        // sometimes rootObject size doesn't keep up with the window size
        rootItem->setSize(this->size());
    }
}

#include "moc_ViewerWindow.cpp"

QQuickWindow *ViewerWindow::openScrollPreview()
{
    if(SpectacleCore::instance()->videoMode()) return nullptr;
    if(m_scrollPreview) {
        auto existing=qobject_cast<ScrollCaptureWindow *>(m_scrollPreview.data());
        if(!existing || existing->phase()!=1) {m_scrollPreview->show();m_scrollPreview->requestActivate();}
        return m_scrollPreview;
    }
    auto view=new ScrollCaptureWindow(engine());
    if(view->status()!=QQuickView::Ready) {delete view;return nullptr;}
    m_scrollPreview=view;view->show();view->requestActivate();return view;
}

void ViewerWindow::scanImageCode()
{
    if (m_scanningCode || SpectacleCore::instance()->videoMode()) return;
    const auto snapshot = SpectacleCore::instance()->annotationDocument()->renderToImage();
    m_scanningCode = true;
    Q_EMIT scanningCodeChanged();
    const auto epoch = m_codeScanEpoch;
    auto watcher = new QFutureWatcher<NkShot::Imaging::CodeScanResult>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, snapshot, epoch] {
        const auto result = watcher->result();
        watcher->deleteLater();
        m_scanningCode = false;
        Q_EMIT scanningCodeChanged();
        // A capture or edit may have replaced the source while the worker decoded.
        // Never present a stale result over a selector or a different document.
        if (epoch != m_codeScanEpoch || !isVisible() || windowStates().testFlag(Qt::WindowMinimized)
            || SpectacleCore::instance()->captureTimeRemaining() > 0 || SpectacleCore::instance()->videoMode()
            || snapshot != SpectacleCore::instance()->annotationDocument()->renderToImage()) return;
        using Result = NkShot::Imaging::CodeScanResult;
        if (result.status != Result::Text) {
            const auto message = result.status == Result::Binary
                ? i18nc("@info", "A code contains binary data, not readable text. No content was copied or opened.")
                : result.status == Result::InvalidImage
                ? i18nc("@info", "Code reading requires a valid image of at most 20 megapixels. Crop or resize a copy and try again.")
                : i18nc("@info", "No readable code found. Try a clearer image with the complete code and its surrounding margin.");
            InlineMessageModel::instance()->push(InlineMessageModel::Warning, message);
            return;
        }
        auto preview = new OcrTextWindow(result.text, engine(), true);
        if (preview->status() != QQuickView::Ready) {
            delete preview;
            InlineMessageModel::instance()->push(InlineMessageModel::Error, i18nc("@info", "The code result could not be opened. Your image is unchanged."));
            return;
        }
        preview->setScreen(screen());
        preview->show();
        preview->requestActivate();
    });
    watcher->setFuture(QtConcurrent::run([snapshot] { return NkShot::Imaging::scanImageCode(snapshot); }));
}

QQuickWindow *ViewerWindow::openColorPicker()
{
    if (!isVisible()) return nullptr;
    if (m_colorPicker && !m_colorPicker->isVisible()) delete m_colorPicker;
    if (!m_colorPicker) {
        auto picker = new ColorPickerWindow(m_mode == Dialog || SpectacleCore::instance()->videoMode() ? QImage()
            : SpectacleCore::instance()->annotationDocument()->renderToImage(), engine());
        if (picker->status() != QQuickView::Ready) {
            delete picker;
            InlineMessageModel::instance()->push(InlineMessageModel::Warning,
                i18nc("@info", "The color picker could not be opened. Try again."));
            return nullptr;
        }
        m_colorPicker = picker;
        picker->setTransientParent(this);
        connect(this, &QObject::destroyed, picker, [picker] { delete picker; });
        connect(this, &QWindow::visibleChanged, picker, [picker](bool visible) { if (!visible) picker->close(); });
        picker->setScreen(screen());
    }
    m_colorPicker->show();
    m_colorPicker->setWindowStates(m_colorPicker->windowStates().setFlag(Qt::WindowMinimized, false));
    m_colorPicker->requestActivate();
    return m_colorPicker;
}

QObject *ViewerWindow::clipJobObject() const { return m_clipJob; }

QQuickWindow *ViewerWindow::editVideoAnnotations(QVideoSink *sink, const QUrl &source)
{
    const auto core=SpectacleCore::instance();
    if(!m_clipJob||!m_clipJob->ready()||m_clipJob->busy()||m_clipSaveDialog||!m_clipJob->annotationAvailable()
        ||!core->videoMode()||core->videoPlatform()->isRecording()||source.isEmpty()
        ||source!=core->currentVideo()||source!=m_clipJob->sourceUrl()||!sink)return nullptr;
    if(m_videoAnnotationEditor){m_videoAnnotationEditor->show();m_videoAnnotationEditor->requestActivate();return m_videoAnnotationEditor;}
    const auto frame=sink->videoFrame();
    if(!frame.isValid()||frame.size()!=m_clipJob->sourceSize())return nullptr;
    auto image=frame.toImage();
    if(image.isNull()||image.size()!=m_clipJob->sourceSize())return nullptr;
    image.setDevicePixelRatio(1);
    auto editor=new ImageCopyEditor(image,engine(),true);
    if(editor->status()!=QQuickView::Ready){delete editor;return nullptr;}
    m_videoAnnotationEditor=editor;
    const auto session=m_annotationSession;
    connect(editor,&ImageCopyEditor::annotationsCommitted,this,[this,source,session](const QImage &overlay){
        if(session!=m_annotationSession||!m_clipJob||m_clipJob->sourceUrl()!=source
            ||SpectacleCore::instance()->currentVideo()!=source||m_clipSaveDialog)return;
        m_clipJob->setAnnotationOverlay(overlay);
        requestActivate();
    });
    editor->setObjectName(QStringLiteral("nkshotVideoAnnotationEditor"));
    editor->setTransientParent(this);editor->setModality(Qt::WindowModal);
    editor->setScreen(screen());editor->setPosition(position()+QPoint(30,30));editor->show();
    return editor;
}
void ViewerWindow::clearVideoAnnotations()
{
    if(!m_clipJob||!m_clipJob->ready()||m_clipJob->busy()||m_clipSaveDialog)return;
    ++m_annotationSession;delete m_videoAnnotationEditor;m_videoAnnotationEditor=nullptr;
    m_clipJob->clearAnnotationOverlay();
}

void ViewerWindow::prepareClipExport(const QUrl &source)
{
    if (!m_clipJob || SpectacleCore::instance()->videoPlatform()->isRecording()) return;
    ++m_annotationSession;delete m_videoAnnotationEditor;m_videoAnnotationEditor=nullptr;
    m_clipJob->inspect(source);
}

void ViewerWindow::chooseClipOutput(qint64 startMs, qint64 endMs)
{
    if (!m_clipJob || m_clipJob->busy() || !m_clipJob->ready() || m_clipSaveDialog
        || SpectacleCore::instance()->videoPlatform()->isRecording()) return;
    const QUrl source = m_clipJob->sourceUrl();
    const QString format = m_clipJob->outputFormat();
    const QString resolution = m_clipJob->outputResolution();
    const double exportSpeed = m_clipJob->exportSpeed();
    const bool gif = format == QStringLiteral("gif");
    const bool includeAudio = m_clipJob->includeAudio();
    const QString cropAspect = m_clipJob->cropAspect();
    const QRect cropRect = m_clipJob->cropRect();
    const bool privacyEnabled = m_clipJob->privacyEnabled();
    const QRect privacyRect = m_clipJob->privacyRect();
    const quint64 annotationRevision = m_clipJob->annotationRevision();
    const QFileInfo sourceFile(source.toLocalFile());
    auto dialog = new QFileDialog;
    m_clipSaveDialog = dialog;
    Q_EMIT choosingClipOutputChanged();
    dialog->setObjectName(QStringLiteral("nkshotClipSaveDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setDefaultSuffix(format);
    dialog->setOption(QFileDialog::DontConfirmOverwrite, true);
    dialog->setNameFilter(gif ? i18nc("@item:inlistbox", "Animated GIF (*.gif)") : i18nc("@item:inlistbox", "MP4 video (*.mp4)"));
    dialog->setWindowTitle(gif ? i18nc("@title:window", "Export clip as a new GIF") : i18nc("@title:window", "Export clip as a new MP4"));
    const QString stem = sourceFile.completeBaseName() + QStringLiteral("-clip");
    QString suggested = sourceFile.dir().filePath(stem + QLatin1Char('.') + format);
    for (int i = 2; QFileInfo::exists(suggested) && i < 1000; ++i)
        suggested = sourceFile.dir().filePath(stem + QStringLiteral("-%1.").arg(i) + format);
    dialog->selectFile(suggested);
    dialog->winId();
    dialog->windowHandle()->setTransientParent(this);
    connect(dialog, &QObject::destroyed, this, [this] {
        m_clipSaveDialog = nullptr;
        Q_EMIT choosingClipOutputChanged();
    });
    connect(dialog, &QFileDialog::accepted, this, [this, dialog, source, format, resolution, exportSpeed, includeAudio, cropAspect, cropRect, privacyEnabled, privacyRect, annotationRevision, startMs, endMs] {
        if (!m_clipJob || m_clipJob->sourceUrl() != source || m_clipJob->outputFormat() != format || m_clipJob->outputResolution() != resolution || m_clipJob->exportSpeed() != exportSpeed || m_clipJob->includeAudio() != includeAudio || m_clipJob->cropAspect() != cropAspect || m_clipJob->cropRect() != cropRect || m_clipJob->privacyEnabled() != privacyEnabled || m_clipJob->privacyRect() != privacyRect || m_clipJob->annotationRevision() != annotationRevision || m_clipJob->busy()
            || SpectacleCore::instance()->videoPlatform()->isRecording()) return;
        const auto files = dialog->selectedFiles();
        if (files.size() != 1) return;
        // Clip export is new-file-only. Atomic no-clobber publication also rejects
        // destinations that appear after this dialog closes.
        m_clipJob->exportClip(QUrl::fromLocalFile(files.first()), startMs, endMs);
    });
    dialog->open();
}

void ViewerWindow::cancelClipExport()
{
    ++m_annotationSession;delete m_videoAnnotationEditor;m_videoAnnotationEditor=nullptr;
    if (m_clipSaveDialog) {
        auto dialog = qobject_cast<QFileDialog *>(m_clipSaveDialog.data());
        if (dialog) dialog->reject();
    }
    if (m_clipJob) m_clipJob->cancel();
    Q_EMIT clipUiCancelled();
}
