// SPDX-License-Identifier: GPL-3.0-only
#include "OcrTextWindow.h"
#include "Translation/TranslationManager.h"
#include <QApplication>
#include <QEvent>
#include <QClipboard>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QShortcut>
#include <KLocalizedString>
#include <QFileDialog>
#include <QSaveFile>
#include <QStandardPaths>

OcrTextWindow::OcrTextWindow(const QString &text, QQmlEngine *engine, bool codeResult)
    : QQuickView(engine, nullptr), m_translation(new TranslationManager(this)), m_text(text), m_codeResult(codeResult)
{
    setObjectName(codeResult ? QStringLiteral("nkshotCodeTextWindow") : QStringLiteral("nkshotOcrTextWindow"));
    setTitle(codeResult ? i18nc("@title:window", "Code Content — NkShot") : i18nc("@title:window", "Extracted Text — NkShot"));
    setMinimumSize({360, 260});
    setResizeMode(SizeRootObjectToView);
    auto context = new QQmlContext(engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("ocrWindow"), this);
    const QUrl source(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/OcrText.qml"));
    auto component = new QQmlComponent(engine, source, this);
    setContent(source, component, component->create(context));
    resize(560, 420);
    const auto adjustTranslationHeight = [this] {
        const bool showingTranslation = m_translation->busy() || !m_translation->translatedText().isEmpty();
        const int minimum = showingTranslation ? (width() >= 760 ? 360 : 480) : (!m_translation->status().isEmpty() ? 360 : 260);
        setMinimumHeight(minimum);
        if (height() < minimum) resize(width(), minimum);
    };
    connect(m_translation, &TranslationManager::changed, this, adjustTranslationHeight);
    connect(this, &QWindow::widthChanged, this, adjustTranslationHeight);
    new QShortcut(QKeySequence::SaveAs, this, this, &OcrTextWindow::saveAs);
    new QShortcut(QKeySequence::Save, this, this, &OcrTextWindow::saveAs);
    new QShortcut(QKeySequence::Close, this, this, &OcrTextWindow::close);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, this, &OcrTextWindow::close);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { delete this; });
}
void OcrTextWindow::copyText()
{
    QApplication::clipboard()->setText(m_text);
    Q_EMIT textCopied();
}
bool OcrTextWindow::event(QEvent *event)
{
    const auto type = event->type();
    const bool result = QQuickView::event(event);
    if ((type == QEvent::Close && event->isAccepted()) || type == QEvent::Hide
        || (type == QEvent::WindowStateChange && visibility() == QWindow::Minimized)) {
        if (m_translation->busy()) m_translation->cancel();
    }
    if (type == QEvent::Close && event->isAccepted()) deleteLater();
    return result;
}

OcrTextWindow::~OcrTextWindow() { delete m_saveDialog; }
void OcrTextWindow::saveAs()
{
    if (auto existing = qobject_cast<QFileDialog *>(m_saveDialog.data())) {
        existing->raise(); existing->activateWindow(); return;
    }
    auto dialog = new QFileDialog;
    m_saveDialog = dialog;
    dialog->setObjectName(QStringLiteral("nkshotOcrSaveDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(m_codeResult ? i18nc("@title:window", "Save Code Content") : i18nc("@title:window", "Save Extracted Text"));
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter(i18nc("@item:inlistbox", "UTF-8 text (*.txt)"));
    dialog->setDefaultSuffix(QStringLiteral("txt"));
    dialog->setDirectory(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    dialog->selectFile(m_codeResult ? QStringLiteral("NkShot-code.txt") : QStringLiteral("NkShot-text.txt"));
    dialog->winId();
    dialog->windowHandle()->setTransientParent(this);
    connect(dialog, &QFileDialog::accepted, this, [this, dialog] {
        const auto paths = dialog->selectedFiles();
        if (paths.isEmpty()) return;
        if (!paths.first().endsWith(QStringLiteral(".txt"), Qt::CaseInsensitive)) {
            Q_EMIT saveFinished(i18nc("@info", "Use a .txt filename to save this text."), false);
            return;
        }
        QSaveFile file(paths.first());
        const auto bytes = m_text.toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            Q_EMIT saveFinished(i18nc("@info", "Could not save text: %1", file.errorString()), false);
            return;
        }
        Q_EMIT saveFinished(i18nc("@info", "Text saved"), true);
    });
    dialog->open();
}

QString OcrTextWindow::heading() const
{
    return m_codeResult ? i18nc("@title", "Code content") : i18nc("@title", "Extracted Text");
}
QString OcrTextWindow::description() const
{
    return m_codeResult ? i18nc("@info", "Decoded on this device. Links and commands are shown as text, never opened automatically.")
                        : i18nc("@info", "Recognized on this device. Review the text before using it.");
}

QObject *OcrTextWindow::translation() const { return m_translation; }
void OcrTextWindow::copyTranslation()
{
    if (!m_codeResult && !m_translation->translatedText().isEmpty())
        QApplication::clipboard()->setText(m_translation->translatedText());
}
