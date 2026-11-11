// SPDX-License-Identifier: GPL-3.0-only
#include "PinnedImageWindow.h"
#include "OcrTextWindow.h"
#include "Imaging/LocalTextExtraction.h"
#include "settings.h"
#include <QtConcurrentRun>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QImageWriter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QScreen>
#include <QShortcut>
#include <QQuickItem>
#include <KLocalizedString>

namespace {
class SnapshotProvider final : public QQuickImageProvider {
public:
    explicit SnapshotProvider(const QImage &image) : QQuickImageProvider(Image), m_image(image) {}
    QImage requestImage(const QString &, QSize *size, const QSize &) override {
        if (size) *size = m_image.size();
        return m_image;
    }
private:
    const QImage m_image;
};
quint64 nextPin = 0;
}

PinnedImageWindow::PinnedImageWindow(const QImage &image, QQmlEngine *engine)
    : QQuickView(engine, nullptr), m_image(image),
      m_providerId(QStringLiteral("nkshotpin%1").arg(++nextPin)), m_sharedEngine(engine)
{
    setObjectName(QStringLiteral("nkshotPinnedImage"));
    setTitle(i18nc("@title:window", "Pinned Image - NkShot"));
    setFlags(Qt::Window | Qt::WindowStaysOnTopHint);
    setMinimumSize({260, 180});
    setResizeMode(SizeRootObjectToView);
    engine->addImageProvider(m_providerId, new SnapshotProvider(m_image));
    auto context = new QQmlContext(engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("pinnedWindow"), this);
    const QUrl source(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/PinnedImage.qml"));
    auto component = new QQmlComponent(engine, source, this);
    setContent(source, component, component->create(context));
    const auto available = screen()->availableGeometry().size();
    const auto fitted = m_image.size().scaled(QSize(qMin(720, available.width() - 80), qMin(480, available.height() - 120)), Qt::KeepAspectRatio);
    resize(qMax(300, fitted.width() + 24), qMax(220, fitted.height() + 72));
    new QShortcut(QKeySequence::SaveAs, this, this, &PinnedImageWindow::saveAs);
    new QShortcut(QKeySequence::Copy, this, this, &PinnedImageWindow::copyImage);
    new QShortcut(QKeySequence::Close, this, this, &PinnedImageWindow::close);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, this, [this] {
        if (rootObject() && rootObject()->property("selectingArea").toBool())
            QMetaObject::invokeMethod(rootObject(), "cancelArea");
        else close();
    });
    // Views must be destroyed before their shared application engine.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { delete this; });
}

PinnedImageWindow::~PinnedImageWindow()
{
    if(m_ocrCancelled) m_ocrCancelled->store(true);
    delete m_saveDialog;
    if (m_sharedEngine) m_sharedEngine->removeImageProvider(m_providerId);
}
QUrl PinnedImageWindow::imageSource() const { return QUrl(QStringLiteral("image://%1/snapshot").arg(m_providerId)); }
void PinnedImageWindow::copyImage() { QApplication::clipboard()->setImage(m_image); Q_EMIT imageCopied(); }
void PinnedImageWindow::saveAs()
{
    if (auto existing = qobject_cast<QFileDialog *>(m_saveDialog.data())) {
        existing->raise(); existing->activateWindow(); return;
    }
    auto dialog = new QFileDialog;
    m_saveDialog = dialog;
    dialog->setObjectName(QStringLiteral("nkshotPinSaveDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window", "Save Pinned Image"));
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter(i18nc("@item:inlistbox", "PNG image (*.png)"));
    dialog->setDefaultSuffix(QStringLiteral("png"));
    dialog->setDirectory(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    dialog->selectFile(QStringLiteral("NkShot.png"));
    dialog->winId();
    dialog->windowHandle()->setTransientParent(this);
    connect(dialog, &QFileDialog::accepted, this, [this, dialog] {
        const auto paths = dialog->selectedFiles();
        if (paths.isEmpty()) return;
        if (!paths.first().endsWith(QStringLiteral(".png"), Qt::CaseInsensitive)) {
            Q_EMIT saveFinished(i18nc("@info", "Use a .png filename to save this image."), false);
            return;
        }
        // Never replace the editable document's global export image or settings.
        QSaveFile file(paths.first());
        if (!file.open(QIODevice::WriteOnly)) {
            Q_EMIT saveFinished(i18nc("@info", "Could not save image: %1", file.errorString()), false);
            return;
        }
        QImageWriter writer(&file, "png");
        if (!writer.write(m_image)) {
            file.cancelWriting();
            Q_EMIT saveFinished(i18nc("@info", "Could not save image: %1", writer.errorString()), false);
        } else if (!file.commit()) {
            Q_EMIT saveFinished(i18nc("@info", "Could not save image: %1", file.errorString()), false);
        } else {
            Q_EMIT saveFinished(i18nc("@info", "Image saved"), true);
        }
    });
    dialog->open();
}

void PinnedImageWindow::setStayOnTop(bool value)
{
    if (value == stayOnTop()) return;
    const bool wasVisible = isVisible();
    setFlag(Qt::WindowStaysOnTopHint, value);
    if (wasVisible) show();
    Q_EMIT stayOnTopChanged();
}
bool PinnedImageWindow::event(QEvent *event)
{
    const auto type = event->type();
    const bool result = QQuickView::event(event);
    if (type == QEvent::Hide && rootObject()) QMetaObject::invokeMethod(rootObject(), "cancelArea");
    if (type == QEvent::Close && event->isAccepted()) {m_closing=true;if(m_ocrCancelled)m_ocrCancelled->store(true);deleteLater();}
    return result;
}

void PinnedImageWindow::extractText(const QString &language)
{
    extractTextRegion(m_image.rect(), language);
}

void PinnedImageWindow::extractTextArea(const QRect &area)
{
    // Reject empty/inverted bounds; intersection is in physical source pixels.
    if (area.width() < 2 || area.height() < 2) return;
    const qint64 left = qMax<qint64>(0, area.x());
    const qint64 top = qMax<qint64>(0, area.y());
    const qint64 right = qMin<qint64>(m_image.width(), qint64(area.x()) + area.width());
    const qint64 bottom = qMin<qint64>(m_image.height(), qint64(area.y()) + area.height());
    if (right - left < 2 || bottom - top < 2) return;
    const QRect clipped(int(left), int(top), int(right - left), int(bottom - top));
    if (clipped.width() < 2 || clipped.height() < 2) return;
    extractTextRegion(clipped, QString());
}

void PinnedImageWindow::extractTextRegion(const QRect &area, const QString &language)
{
    if(m_extractingText || m_closing) return;
    // Bound the region before allocating its independent crop, not only in OCR.
    if (area.width() > 8192 || area.height() > 32000 || qint64(area.width()) * area.height() > 20000000) {
        Q_EMIT textExtractionStatus(i18nc("@info", "This area is too large for text extraction. Select a smaller area."));
        return;
    }
    auto languages=language.isEmpty()?Settings::ocrLanguages():language.split(u'+',Qt::SkipEmptyParts);
    if(languages.isEmpty()) languages={QStringLiteral("eng")};
    if(!m_pendingText.isEmpty() && m_pendingLanguages==languages && m_pendingArea==area) {
        if(isVisible() && visibility()!=QWindow::Minimized) {
            showExtractedText(m_pendingText);
            m_pendingText.clear();
        }
        return;
    }
    m_pendingText.clear();m_pendingLanguages.clear();
    m_extractingText=true;m_ocrCancelled=std::make_shared<std::atomic_bool>(false);
    Q_EMIT extractingTextChanged();
    Q_EMIT textExtractionStatus(i18nc("@info","Reading text locally…"));
    const auto image=area==m_image.rect()?m_image:m_image.copy(area);const auto cancelled=m_ocrCancelled;
    QtConcurrent::run([image,languages,cancelled]{return NkShot::Imaging::extractLocalText(image,languages,cancelled);})
        .then(this,[this,cancelled,languages,area](const NkShot::Imaging::TextExtractionResult &result){
        if(m_closing || cancelled->load()) return;
        m_extractingText=false;Q_EMIT extractingTextChanged();
        using Error=NkShot::Imaging::TextExtractionError;
        if(result.error!=Error::None) {
            Q_EMIT textExtractionStatus(result.error==Error::Unavailable || result.error==Error::InvalidLanguage
                ? i18nc("@info","The configured OCR language data is unavailable. Check OCR languages in Options.")
                : result.error==Error::Busy ? i18nc("@info","Other text extractions are running. Try again shortly.")
                : result.error==Error::InvalidInput ? i18nc("@info","This image is too large for text extraction. Use a smaller sharing copy.")
                : i18nc("@info","Text extraction failed or timed out. The image and clipboard are unchanged."));
            return;
        }
        if(result.text.isEmpty()) {Q_EMIT textExtractionStatus(i18nc("@info","No text was found. The image and clipboard are unchanged."));return;}
        // Scrolling capture temporarily hides pins. Never steal focus from its selector.
        if(!isVisible() || visibility()==QWindow::Minimized) {
            if (area != m_image.rect()) {
                Q_EMIT textExtractionStatus(i18nc("@info", "Area reading finished while hidden. Select an area again to read it."));
                return;
            }
            m_pendingText=result.text;m_pendingLanguages=languages;m_pendingArea=area;
            Q_EMIT textExtractionStatus(i18nc("@info","Text ready. Choose Extract text to view it."));
            return;
        }
        showExtractedText(result.text);
    });
}

void PinnedImageWindow::showExtractedText(const QString &text)
{
    auto textWindow=new OcrTextWindow(text,m_sharedEngine);
    if(textWindow->status()!=QQuickView::Ready) {
        delete textWindow;
        Q_EMIT textExtractionStatus(i18nc("@info","The text preview could not open. Try again."));
        return;
    }
    textWindow->setScreen(screen());textWindow->show();textWindow->requestActivate();
    Q_EMIT textExtractionStatus(i18nc("@info","Text ready. Select, copy or save it in the text window."));
}
