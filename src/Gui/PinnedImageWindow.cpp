// SPDX-License-Identifier: GPL-3.0-only
#include "PinnedImageWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QScreen>
#include <QShortcut>
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
    new QShortcut(QKeySequence::Copy, this, this, &PinnedImageWindow::copyImage);
    new QShortcut(QKeySequence::Close, this, this, &PinnedImageWindow::close);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, this, &PinnedImageWindow::close);
    // Views must be destroyed before their shared application engine.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { delete this; });
}

PinnedImageWindow::~PinnedImageWindow()
{
    if (m_sharedEngine) m_sharedEngine->removeImageProvider(m_providerId);
}
QUrl PinnedImageWindow::imageSource() const { return QUrl(QStringLiteral("image://%1/snapshot").arg(m_providerId)); }
void PinnedImageWindow::copyImage() { QApplication::clipboard()->setImage(m_image); Q_EMIT imageCopied(); }
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
    if (type == QEvent::Close && event->isAccepted()) deleteLater();
    return result;
}
