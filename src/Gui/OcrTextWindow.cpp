// SPDX-License-Identifier: GPL-3.0-only
#include "OcrTextWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QShortcut>
#include <KLocalizedString>

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
