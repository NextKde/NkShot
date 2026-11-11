// SPDX-License-Identifier: GPL-3.0-only
#include "PinnedImageWindow.h"
#include "ImageCopyEditor.h"
#include "OcrTextWindow.h"
#include "Imaging/LocalTextExtraction.h"
#include "settings.h"
#include <QtConcurrentRun>
#include <QApplication>
#include <QMap>
#include <limits>
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
#include <cmath>

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
    new QShortcut(QKeySequence::Copy, this, this, [this] { if (m_textSelectionActive) copySelectedText(); else copyImage(); });
    new QShortcut(QKeySequence::Close, this, this, &PinnedImageWindow::close);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, this, [this] {
        if (m_textSelectionActive) cancelTextSelection();
        else if (rootObject() && rootObject()->property("selectingArea").toBool())
            QMetaObject::invokeMethod(rootObject(), "cancelArea");
        else close();
    });
    // Views must be destroyed before their shared application engine.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { delete this; });
}

PinnedImageWindow::~PinnedImageWindow()
{
    if(m_ocrCancelled) m_ocrCancelled->store(true);
    if(m_inlineCancelled) m_inlineCancelled->store(true);
    delete m_saveDialog;
    if (m_sharedEngine) m_sharedEngine->removeImageProvider(m_providerId);
}
QUrl PinnedImageWindow::imageSource() const { return QUrl(QStringLiteral("image://%1/snapshot").arg(m_providerId)); }
void PinnedImageWindow::copyImage() { QApplication::clipboard()->setImage(m_image); Q_EMIT imageCopied(); }
QQuickWindow *PinnedImageWindow::editCopy()
{
    if(m_image.isNull()||qint64(m_image.width())*m_image.height()>40'000'000)return nullptr;
    if(!m_copyEditor) {
        auto editor=new ImageCopyEditor(m_image,engine());
        if(editor->status()!=QQuickView::Ready){delete editor;return nullptr;}
        m_copyEditor=editor;
    }
    m_copyEditor->show();m_copyEditor->requestActivate();return m_copyEditor;
}
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
    if (type == QEvent::Hide || type == QEvent::Close
        || (type == QEvent::WindowStateChange && visibility() == QWindow::Minimized)) {
        cancelTextSelection();
        if (rootObject()) QMetaObject::invokeMethod(rootObject(), "cancelArea");
    }
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
    if(extractingText() || m_closing) return;
    cancelTextSelection();
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


QVariantList PinnedImageWindow::textWords() const
{
    QVariantList list;
    list.reserve(m_words.size());
    for (const auto &word : m_words)
        list.append(QVariantMap{{QStringLiteral("text"), word.text}, {QStringLiteral("bounds"), word.bounds}});
    return list;
}

QString PinnedImageWindow::selectedText() const
{
    return NkShot::Imaging::selectedWordText(m_words, selectionFirst(), selectionLast());
}

void PinnedImageWindow::cancelTextSelection()
{
    ++m_inlineGeneration;
    if (m_inlineCancelled) m_inlineCancelled->store(true);
    const bool active = m_textSelectionActive;
    m_textSelectionActive = false;
    m_words.clear();
    m_anchorWord = m_activeWord = -1;
    if (active) {
        Q_EMIT textSelectionActiveChanged();
        Q_EMIT textWordsChanged();
        Q_EMIT textSelectionChanged();
        Q_EMIT textExtractionStatus(QString());
    }
}

void PinnedImageWindow::selectTextOnImage()
{
    if (extractingText() || m_closing || !isVisible() || visibility() == QWindow::Minimized) return;
    cancelTextSelection();
    if (rootObject()) QMetaObject::invokeMethod(rootObject(), "cancelArea");
    m_pendingText.clear(); m_pendingLanguages.clear();
    auto languages = Settings::ocrLanguages();
    if (languages.isEmpty()) languages = {QStringLiteral("eng")};
    m_textSelectionActive = true;
    m_inlineBusy = true;
    m_inlineCancelled = std::make_shared<std::atomic_bool>(false);
    const auto cancelled = m_inlineCancelled;
    const auto generation = m_inlineGeneration;
    Q_EMIT textSelectionActiveChanged();
    Q_EMIT extractingTextChanged();
    Q_EMIT textExtractionStatus(QString());
    const auto image = m_image;
    QtConcurrent::run([image, languages, cancelled] { return NkShot::Imaging::extractLocalText(image, languages, cancelled); })
        .then(this, [this, cancelled, generation](const NkShot::Imaging::TextExtractionResult &result) {
            m_inlineBusy = false;
            Q_EMIT extractingTextChanged();
            if (m_closing || cancelled->load() || generation != m_inlineGeneration || !m_textSelectionActive) return;
            if (!isVisible() || visibility() == QWindow::Minimized) { cancelTextSelection(); return; }
            using Error = NkShot::Imaging::TextExtractionError;
            if (result.error != Error::None || result.words.isEmpty() || !result.wordsComplete) {
                cancelTextSelection();
                const auto message = result.error == Error::Unavailable || result.error == Error::InvalidLanguage
                    ? i18nc("@info", "The configured OCR language data is unavailable. Check OCR languages in Options.")
                    : result.error == Error::InvalidInput
                    ? i18nc("@info", "This image is too large for text selection. Read a smaller area instead.")
                    : result.error != Error::None
                    ? i18nc("@info", "Text selection failed or timed out. Try again shortly.")
                    : result.text.isEmpty()
                    ? i18nc("@info", "No text was found. The image and clipboard are unchanged.")
                    : i18nc("@info", "Text could not be aligned for selection. Use Read whole image or select an area.");
                Q_EMIT textExtractionStatus(message);
                return;
            }
            m_words = result.words;
            Q_EMIT textWordsChanged();
            Q_EMIT textSelectionChanged();
        });
}

void PinnedImageWindow::setWordSelection(int index, bool extend)
{
    if (!m_textSelectionActive || index < 0 || index >= m_words.size()) return;
    const int anchor = (!extend || m_anchorWord < 0) ? index : m_anchorWord;
    if (m_anchorWord == anchor && m_activeWord == index) return;
    m_anchorWord = anchor;
    m_activeWord = index;
    Q_EMIT textSelectionChanged();
}

void PinnedImageWindow::selectWordAt(qreal x, qreal y, bool extend, bool nearest)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !m_textSelectionActive) return;
    for (int i = 0; i < m_words.size(); ++i) {
        if (QRectF(m_words[i].bounds).contains(QPointF(x, y))) { setWordSelection(i, extend); return; }
    }
    // Drag endpoints snap to the vertically nearest OCR line, then the nearest
    // horizontal word on that line. A distant adjacent line must not win merely
    // because its word happens to be closer horizontally. Initial clicks stay exact.
    if (nearest && extend && m_anchorWord >= 0 && !m_words.isEmpty()) {
        QMap<int, QRect> lines;
        for (const auto &word : m_words) lines[word.lineIndex] |= word.bounds;
        int line = m_words[m_activeWord].lineIndex;
        qreal best = std::numeric_limits<qreal>::max();
        for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
            const qreal distance = qMax(qreal(0), qMax(it->top() - y, y - it->bottom()));
            if (distance < best) { best = distance; line = it.key(); }
        }
        // Separate OCR blocks on the same visual row can differ by a few ink
        // pixels (for example capitals with/without a descender). Allow a small
        // height-relative vertical tolerance, then resolve horizontally. This
        // must not prefer a distant row merely because it is nearer in x.
        const qreal tolerance = qBound(qreal(2), lines[line].height() * 0.2, qreal(8));
        qreal bestHorizontal = std::numeric_limits<qreal>::max();
        for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
            const qreal distance = qMax(qreal(0), qMax(it->top() - y, y - it->bottom()));
            if (distance > best + tolerance) continue;
            const qreal horizontal = qMax(qreal(0), qMax(it->left() - x, x - it->right()));
            if (horizontal < bestHorizontal) { bestHorizontal = horizontal; line = it.key(); }
        }
        int index = -1;
        best = std::numeric_limits<qreal>::max();
        for (int i = 0; i < m_words.size(); ++i) {
            if (m_words[i].lineIndex != line) continue;
            const auto box = m_words[i].bounds;
            const qreal distance = qMax(qreal(0), qMax(box.left() - x, x - box.right()));
            if (distance < best) { best = distance; index = i; }
        }
        if (index >= 0) setWordSelection(index, true);
        return;
    }
    if (!extend) { m_anchorWord = m_activeWord = -1; Q_EMIT textSelectionChanged(); }
}

void PinnedImageWindow::moveTextSelection(int direction, bool extend)
{
    if (m_words.isEmpty()) return;
    int index = m_activeWord;
    if (direction == -2) index = 0;
    else if (direction == 2) index = m_words.size() - 1;
    else index = index < 0 ? 0 : qBound(0, index + (direction < 0 ? -1 : 1), int(m_words.size()) - 1);
    setWordSelection(index, extend);
}

void PinnedImageWindow::selectAllText()
{
    if (!m_textSelectionActive || m_words.isEmpty()) return;
    m_anchorWord = 0; m_activeWord = m_words.size() - 1;
    Q_EMIT textSelectionChanged();
}

void PinnedImageWindow::copySelectedText()
{
    const auto text = selectedText();
    if (!m_textSelectionActive || text.isEmpty()) return;
    QApplication::clipboard()->setText(text);
    Q_EMIT textExtractionStatus(i18nc("@info", "Selected text copied"));
}
