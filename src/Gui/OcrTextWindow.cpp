// SPDX-License-Identifier: GPL-3.0-only
#include "OcrTextWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QShortcut>
#include <KLocalizedString>
#include <QFileDialog>
#include <QSaveFile>
#include <QStandardPaths>

OcrTextWindow::OcrTextWindow(const QString &text, QQmlEngine *engine)
    : QQuickView(engine, nullptr), m_text(text)
{
    setObjectName(QStringLiteral("nkshotOcrTextWindow"));
    setTitle(i18nc("@title:window", "Extracted Text — NkShot"));
    setMinimumSize({360, 260});
    setResizeMode(SizeRootObjectToView);
    auto context = new QQmlContext(engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("ocrWindow"), this);
    const QUrl source(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/OcrText.qml"));
    auto component = new QQmlComponent(engine, source, this);
    setContent(source, component, component->create(context));
    resize(560, 420);
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
    dialog->setWindowTitle(i18nc("@title:window", "Save Extracted Text"));
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter(i18nc("@item:inlistbox", "UTF-8 text (*.txt)"));
    dialog->setDefaultSuffix(QStringLiteral("txt"));
    dialog->setDirectory(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    dialog->selectFile(QStringLiteral("NkShot-text.txt"));
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
