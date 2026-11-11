// SPDX-License-Identifier: GPL-3.0-only
#include "RecordingCountdownWindow.h"
#include <KLocalizedString>
#include <QEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QShortcut>

RecordingCountdownWindow::RecordingCountdownWindow(QQmlEngine *engine)
    : QQuickView(engine, nullptr)
{
    setObjectName(QStringLiteral("nkshotRecordingCountdownWindow"));
    setTitle(i18nc("@title:window", "Prepare Recording — NkShot"));
    setFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
    setResizeMode(SizeRootObjectToView);
    setMinimumSize({320, 250});
    auto context = new QQmlContext(engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("countdownWindow"), this);
    const QUrl source(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/RecordingCountdown.qml"));
    auto component = new QQmlComponent(engine, source, this);
    setContent(source, component, component->create(context));
    resize(360, 280);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, this, &RecordingCountdownWindow::cancel);
    new QShortcut(QKeySequence::Close, this, this, &RecordingCountdownWindow::cancel);
}

void RecordingCountdownWindow::showCountdown(int seconds, const QString &description)
{
    m_active = true;
    m_description = description;
    m_remainingSeconds = qMax(0, seconds);
    m_phase = seconds > 0 ? QStringLiteral("countdown") : QStringLiteral("preparing");
    Q_EMIT stateChanged();
    show();
    raise();
    requestActivate();
}

void RecordingCountdownWindow::updateRemaining(int seconds)
{
    if (!m_active) return;
    m_remainingSeconds = qMax(0, seconds);
    m_phase = seconds > 0 ? QStringLiteral("countdown") : QStringLiteral("starting");
    Q_EMIT stateChanged();
}

void RecordingCountdownWindow::showStarting()
{
    if (!m_active) return;
    m_remainingSeconds = 0;
    m_phase = QStringLiteral("starting");
    Q_EMIT stateChanged();
}

void RecordingCountdownWindow::dismiss()
{
    m_active = false;
    hide();
}

void RecordingCountdownWindow::cancel()
{
    if (!m_active) return;
    m_active = false;
    hide();
    Q_EMIT cancelRequested();
}

bool RecordingCountdownWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Close || event->type() == QEvent::Hide
        || (event->type() == QEvent::WindowStateChange && windowStates().testFlag(Qt::WindowMinimized))) {
        cancel();
    }
    return QQuickView::event(event);
}
