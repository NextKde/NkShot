// SPDX-License-Identifier: GPL-3.0-only
#include "FrameInspectionWindow.h"
#include "Media/FrameInspectionSession.h"
#include "PinnedImageWindow.h"
#include <QCloseEvent>
#include <QCoreApplication>
#include <QImage>
#include <QKeySequence>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QShortcut>
#include <KLocalizedString>

FrameInspectionWindow::FrameInspectionWindow(const QUrl &source, qint64 positionMs, QQmlEngine *engine)
    : QQuickView(engine, nullptr), m_session(new FrameInspectionSession(engine, this))
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    setObjectName(QStringLiteral("nkshotFrameInspectionWindow"));
    setTitle(i18nc("@title:window", "Browse video frames — NkShot"));
    setMinimumSize({400, 420});
    setResizeMode(SizeRootObjectToView);
    auto context = new QQmlContext(engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("inspectionWindow"), this);
    const QUrl qml(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/FrameInspectionWindow.qml"));
    auto component = new QQmlComponent(engine, qml, this);
    setContent(qml, component, component->create(context));
    const auto available = screen()->availableGeometry().size();
    resize(qMax(400, qMin(900, available.width() - 80)), qMax(420, qMin(620, available.height() - 100)));
    m_session->begin(source, positionMs);
    new QShortcut(QKeySequence::Close, this, this, &FrameInspectionWindow::close);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, this, &FrameInspectionWindow::close);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { delete this; });
}
QObject *FrameInspectionWindow::sessionObject() const { return m_session; }
QQuickWindow *FrameInspectionWindow::extractCopy()
{
    if (!m_session->ready() || m_session->busy()) return nullptr;
    const QImage image = m_session->snapshot();
    if (image.isNull()) {
        // Surface a source-retirement error instead of silently doing nothing.
        m_session->requestFrame(m_session->currentIndex());
        return nullptr;
    }
    auto pin = new PinnedImageWindow(image, engine());
    if (pin->status() != QQuickView::Ready) { delete pin; return nullptr; }
    pin->setObjectName(QStringLiteral("nkshotInspectedFramePin"));
    pin->setTitle(i18nc("@title:window", "Frame %1 — NkShot", m_session->currentIndex() + 1));
    pin->setScreen(screen());
    pin->setPosition(position() + QPoint(30, 30));
    pin->show();
    return pin;
}
void FrameInspectionWindow::closeEvent(QCloseEvent *event)
{
    m_session->clear();
    QQuickView::closeEvent(event);
    deleteLater();
}
