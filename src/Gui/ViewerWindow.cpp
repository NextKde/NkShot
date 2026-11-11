/* This file is part of Spectacle, the KDE screenshot utility
 * SPDX-FileCopyrightText: 2015 Boudhayan Gupta <bgupta@kde.org>
 * SPDX-FileCopyrightText: 2019 David Redondo <kde@david-redondo.de>
 * SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ViewerWindow.h"
#include "ScrollCaptureWindow.h"
#include "PinnedImageWindow.h"
#include "OcrTextWindow.h"
#include "Imaging/ImageTransforms.h"
#include "OcrManager.h"
#include "Imaging/VerticalStitcher.h"
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
#include <QDrag>
#include <QFile>
#include <QMimeData>
#include <QShortcut>

using namespace Qt::StringLiterals;

ViewerWindow *ViewerWindow::s_viewerWindowInstance = nullptr;

ViewerWindow::ViewerWindow(Mode mode, QQmlEngine *engine, QWindow *parent)
    : SpectacleWindow(engine, parent)
    , m_mode(mode)
{
    s_viewerWindowInstance = this;
    s_isAnnotating = false;

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

QSize ViewerWindow::imageSnapshotSize() const
{
    return SpectacleCore::instance()->annotationDocument()->renderToImage().size();
}
void ViewerWindow::resizeImageCopy(int width)
{
    createImageCopy([width](const QImage &snapshot) { return NkShot::Imaging::resizedToWidth(snapshot, width); },
        i18nc("@title:window", "Resized Copy — NkShot"),
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

void ViewerWindow::combineImages(bool stackWithoutOverlap)
{
    if (m_combiningImages) return;
    if (auto existing = qobject_cast<QFileDialog *>(m_combineDialog.data())) {
        existing->raise(); existing->activateWindow(); return;
    }
    auto dialog = new QFileDialog;
    m_combineDialog = dialog;
    dialog->setObjectName(QStringLiteral("nkshotCombineDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(stackWithoutOverlap
        ? i18nc("@title:window", "Stack images — retain all content, top to bottom by filename")
        : i18nc("@title:window", "Combine overlapping screenshots — top to bottom by filename"));
    dialog->setFileMode(QFileDialog::ExistingFiles);
    dialog->setNameFilter(i18nc("@item:inlistbox", "Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    dialog->winId();
    dialog->windowHandle()->setTransientParent(this);
    connect(dialog, &QFileDialog::accepted, this, [this, dialog, stackWithoutOverlap] {
        auto paths = dialog->selectedFiles();
        if (paths.size() < 2 || paths.size() > 16) {
            InlineMessageModel::instance()->push(InlineMessageModel::Warning,
                i18nc("@info", "Choose 2–16 images. Number their filenames from top to bottom."));
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
        connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, stackWithoutOverlap] {
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
                        : i18nc("@info", "An image could not be read or is too short. Choose intact images taller than 24 pixels."); break;
                case Error::IncompatibleFrames:
                    reason = i18nc("@info", "Images must have the same width, scale and color profile. Use screenshots from the same capture session."); break;
                case Error::NoOverlap:
                    reason = i18nc("@info", "No unchanged overlap was found. Number images top to bottom and retain at least 24 overlapping pixels. Prefer original PNG screenshots."); break;
                case Error::AmbiguousOverlap:
                    reason = i18nc("@info", "Repeated content makes the overlap ambiguous. Choose screenshots with more distinctive overlapping content."); break;
                case Error::NoProgress:
                    reason = i18nc("@info", "A repeated image adds no new content. Remove duplicate or fully overlapping screenshots."); break;
                case Error::TooLarge:
                    reason = i18nc("@info", "These images exceed the safe size limit. Use fewer or smaller images: 40 megapixels total, at most 8192 pixels wide and 32000 pixels tall."); break;
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
        watcher->setFuture(QtConcurrent::run([paths, stackWithoutOverlap] {
            QList<QImage> frames;
            qint64 pixels = 0;
            for (const auto &path : paths) {
                QImageReader reader(path);
                const auto size = reader.size();
                const qint64 count = qint64(size.width()) * size.height();
                if (!size.isValid() || count < 1)
                    return NkShot::Imaging::StitchResult{{}, {}, NkShot::Imaging::StitchError::InvalidInput};
                if (count > 40000000 - pixels || size.width() > 8192 || size.height() > 32000)
                    return NkShot::Imaging::StitchResult{{}, {}, NkShot::Imaging::StitchError::TooLarge};
                pixels += count;
                auto image = reader.read();
                if (image.isNull()) return NkShot::Imaging::StitchResult{{}, {}, NkShot::Imaging::StitchError::InvalidInput};
                frames.append(image);
            }
            return stackWithoutOverlap ? NkShot::Imaging::stackVertical(frames) : NkShot::Imaging::stitchVertical(frames);
        }));
    });
    dialog->open();
}

ViewerWindow::~ViewerWindow()
{
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
        const QSize implicitSize = {qRound(rootItem->implicitWidth()), qRound(rootItem->implicitHeight())};
        setMinimumSize(implicitSize);
        setMaximumSize(implicitSize);
        connect(rootItem, &QQuickItem::implicitWidthChanged, this, [this]() {
            int implicitWidth = qRound(rootObject()->implicitWidth());
            setMinimumWidth(implicitWidth);
            setMaximumWidth(implicitWidth);
        });
        connect(rootItem, &QQuickItem::implicitHeightChanged, this, [this]() {
            int implicitHeight = qRound(rootObject()->implicitHeight());
            setMinimumHeight(implicitHeight);
            setMaximumHeight(implicitHeight);
        });
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

bool ViewerWindow::scrollPreviewEnabled() const {return qEnvironmentVariable("NKSHOT_EXPERIMENTAL_SCROLL")==QStringLiteral("1");}
QQuickWindow *ViewerWindow::openScrollPreview()
{
    if(!scrollPreviewEnabled() || SpectacleCore::instance()->videoMode()) return nullptr;
    if(m_scrollPreview) {
        auto existing=qobject_cast<ScrollCaptureWindow *>(m_scrollPreview.data());
        if(!existing || existing->phase()!=1) {m_scrollPreview->show();m_scrollPreview->requestActivate();}
        return m_scrollPreview;
    }
    auto view=new ScrollCaptureWindow(engine());
    if(view->status()!=QQuickView::Ready) {delete view;return nullptr;}
    m_scrollPreview=view;view->show();view->requestActivate();return view;
}
