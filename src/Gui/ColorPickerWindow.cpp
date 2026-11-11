// SPDX-License-Identifier: GPL-3.0-only
#include "ColorPickerWindow.h"
#include "Imaging/ColorSample.h"
#include "Platforms/ScreenColorPicker.h"

#include <KLocalizedString>
#include <KWaylandExtras>
#include <QClipboard>
#include <QEvent>
#include <QGuiApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QShortcut>
#include <QTimer>

#include <algorithm>

class ColorImageProvider : public QQuickImageProvider
{
public:
    explicit ColorImageProvider(const QImage &image) : QQuickImageProvider(Image), m_image(image) {}
    void setNeighborhood(const QImage &image)
    {
        QMutexLocker lock(&m_mutex);
        m_neighborhood = image;
    }
    QImage requestImage(const QString &id, QSize *size, const QSize &) override
    {
        QMutexLocker lock(&m_mutex);
        const auto image = id.startsWith(QStringLiteral("sample/")) ? m_neighborhood : m_image;
        if (size) *size = image.size();
        return image;
    }
private:
    QMutex m_mutex;
    const QImage m_image;
    QImage m_neighborhood;
};

static quint64 colorPickerId = 0;

ColorPickerWindow::ColorPickerWindow(const QImage &image, QQmlEngine *engine)
    : QQuickView(engine, nullptr)
    , m_image(NkShot::Imaging::colorSampleSnapshot(image))
    , m_engine(engine)
    , m_provider(new ColorImageProvider(m_image))
    , m_providerId(QStringLiteral("nkshotcolor%1").arg(++colorPickerId))
{
    // C++ owns close/draft lifetime; QML return values must not become GC-owned.
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    m_screenPicker = new ScreenColorPicker(this);
    m_screenMode = m_image.isNull();
    connect(m_screenPicker, &ScreenColorPicker::changed, this, [this] {
        if (m_screenRequestActive && !m_screenPicker->busy()) {
            m_screenResultValid = m_screenPicker->status() == QStringLiteral("success") && m_screenPicker->hasColor();
            m_screenRequestActive = false;
        }
        if (!m_screenPicker->busy() && !m_waitingForParent) releaseParentExport();
        Q_EMIT screenStateChanged();
        if (m_screenMode) Q_EMIT sampleChanged();
    });
    setObjectName(QStringLiteral("nkshotColorPickerWindow"));
    setTitle(i18nc("@title:window", "Pick a Color — NkShot"));
    setMinimumSize({400, 460});
    setResizeMode(SizeRootObjectToView);
    engine->addImageProvider(m_providerId, m_provider);
    auto context = new QQmlContext(engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("colorWindow"), this);
    const QUrl source(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/ColorPicker.qml"));
    auto component = new QQmlComponent(engine, source, this);
    setContent(source, component, component->create(context));
    resize(m_screenMode ? QSize(420, 520) : QSize(820, 620));
    new QShortcut(QKeySequence::Copy, this, this, &ColorPickerWindow::copyHex);
    new QShortcut(QKeySequence::Close, this, this, &ColorPickerWindow::close);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, this, [this] {
        if (m_screenMode && screenPending()) cancelScreenPick();
        else close();
    });
}

ColorPickerWindow::~ColorPickerWindow()
{
    cancelScreenPick();
    releaseParentExport();
    if (m_engine) m_engine->removeImageProvider(m_providerId);
}

bool ColorPickerWindow::event(QEvent *event)
{
    const auto type = event->type();
    if (type == QEvent::Hide || type == QEvent::Close
        || (type == QEvent::WindowStateChange && windowStates().testFlag(Qt::WindowMinimized))) {
        cancelScreenPick();
    }
    const bool result = QQuickView::event(event);
    if (type == QEvent::Close && event->isAccepted()) deleteLater();
    return result;
}

QUrl ColorPickerWindow::imageSource() const
{
    return QUrl(QStringLiteral("image://%1/image").arg(m_providerId));
}

QUrl ColorPickerWindow::neighborhoodSource() const
{
    return imageHasSample() ? QUrl(QStringLiteral("image://%1/sample/%2").arg(m_providerId).arg(m_revision)) : QUrl();
}

QColor ColorPickerWindow::sampleColor() const
{
    return m_screenMode ? (hasSample() ? m_screenPicker->color() : QColor()) : (imageHasSample() ? m_image.pixelColor(m_pixel) : QColor());
}

QString ColorPickerWindow::hexValue() const { return m_screenMode ? (hasSample() ? m_screenPicker->hexValue() : QString()) : NkShot::Imaging::colorSampleHex(sampleColor()); }
QString ColorPickerWindow::rgbValue() const { return m_screenMode ? (hasSample() ? m_screenPicker->rgbValue() : QString()) : NkShot::Imaging::colorSampleRgb(sampleColor()); }

void ColorPickerWindow::selectPixel(QPoint pixel)
{
    if (m_screenMode || !m_image.rect().contains(pixel) || pixel == m_pixel) return;
    m_pixel = pixel;
    m_provider->setNeighborhood(NkShot::Imaging::colorSampleNeighborhood(m_image, pixel));
    ++m_revision;
    Q_EMIT sampleChanged();
}

void ColorPickerWindow::sampleAt(qreal x, qreal y, qreal width, qreal height)
{
    selectPixel(NkShot::Imaging::colorSamplePixelAt({x, y}, {width, height}, m_image.size()));
}

void ColorPickerWindow::moveSample(int dx, int dy)
{
    if (m_screenMode || !hasImage()) return;
    if (!imageHasSample()) { selectPixel({m_image.width() / 2, m_image.height() / 2}); return; }
    // Bound the delta before addition, even for calls outside the keyboard UI.
    selectPixel({int(std::clamp(qint64(m_pixel.x()) + dx, qint64(0), qint64(m_image.width() - 1))),
                 int(std::clamp(qint64(m_pixel.y()) + dy, qint64(0), qint64(m_image.height() - 1)))});
}

void ColorPickerWindow::copyHex()
{
    if (!hasSample()) return;
    QGuiApplication::clipboard()->setText(hexValue());
    Q_EMIT valueCopied(hexValue());
}

void ColorPickerWindow::copyRgb()
{
    if (!hasSample()) return;
    QGuiApplication::clipboard()->setText(rgbValue());
    Q_EMIT valueCopied(rgbValue());
}

QObject *ColorPickerWindow::screenPicker() const { return m_screenPicker; }

bool ColorPickerWindow::hasSample() const
{
    return m_screenMode ? (m_screenResultValid && !screenPending() && m_screenPicker->hasColor()) : imageHasSample();
}

bool ColorPickerWindow::screenPending() const
{
    return m_waitingForParent || m_screenPicker->busy();
}

QString ColorPickerWindow::screenStatus() const
{
    if (m_waitingForParent) return i18nc("@info", "Preparing the desktop color picker…");
    if (!m_screenNotice.isEmpty()) return m_screenNotice;
    const auto status = m_screenPicker->status();
    if (status == QStringLiteral("waiting")) return i18nc("@info", "Choose a color in the desktop picker. Press Esc there to cancel.");
    if (m_screenResultValid && status == QStringLiteral("success")) return i18nc("@info", "Selected by your desktop. Nothing is copied until you choose Copy.");
    if (!m_screenPicker->error().isEmpty()) return m_screenPicker->error();
    if (status == QStringLiteral("cancelled")) return i18nc("@info", "Selection cancelled. Nothing was copied.");
    return i18nc("@info", "Use the desktop color picker. No screenshot is required.");
}

void ColorPickerWindow::setScreenMode(bool screen)
{
    if (!screen && !hasImage()) return;
    if (m_screenMode == screen) return;
    cancelScreenPick();
    m_screenNotice.clear();
    m_screenMode = screen;
    Q_EMIT sourceModeChanged();
    Q_EMIT screenStateChanged();
    Q_EMIT sampleChanged();
}

void ColorPickerWindow::releaseParentExport()
{
    if (!m_parentExported) return;
    m_parentExported = false;
    KWaylandExtras::unexportToplevel(this);
}

void ColorPickerWindow::cancelScreenPick()
{
    const bool wasPending = screenPending();
    m_screenRequestActive = false;
    if (wasPending) m_screenResultValid = false;
    ++m_screenEpoch;
    m_waitingForParent = false;
    if (m_screenPicker->busy()) m_screenPicker->cancel();
    releaseParentExport();
    if (wasPending) m_screenNotice = i18nc("@info", "Stopped waiting. If the desktop picker is still visible, press Esc there to cancel.");
    Q_EMIT screenStateChanged();
    if (m_screenMode) Q_EMIT sampleChanged();
}

void ColorPickerWindow::requestScreenColor(const QString &parentIdentifier)
{
    m_waitingForParent = false;
    if (!m_screenMode || !isVisible() || windowStates().testFlag(Qt::WindowMinimized)) {
        releaseParentExport();
        Q_EMIT screenStateChanged();
        return;
    }
    m_screenRequestActive = true;
    m_screenPicker->request(parentIdentifier);
    Q_EMIT screenStateChanged();
    Q_EMIT sampleChanged();
}

void ColorPickerWindow::pickScreenColor()
{
    if (!m_screenMode || screenPending() || !isVisible() || windowStates().testFlag(Qt::WindowMinimized)) return;
    const quint64 epoch = ++m_screenEpoch;
    // Invalidate immediately, including the asynchronous parent-export phase:
    // cancelling before request() must never reveal the previous pick again.
    m_screenResultValid = false;
    m_screenRequestActive = false;
    m_screenNotice.clear();
    const auto platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("xcb")) {
        requestScreenColor(QStringLiteral("x11:%1").arg(qulonglong(winId()), 0, 16));
        return;
    }
    // A cancelled/expired export may still finish. Do not start a second native
    // export that its late cleanup could revoke; an unparented portal is valid.
    if (!platform.startsWith(QStringLiteral("wayland")) || m_parentWatcher) {
        requestScreenColor(QString());
        return;
    }
    m_waitingForParent = true;
    m_parentExported = true;
    auto watcher = new QFutureWatcher<QString>(this);
    m_parentWatcher = watcher;
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, epoch] {
        const auto future = watcher->future();
        const auto handle = !future.isCanceled() && future.resultCount() ? future.result() : QString();
        m_parentWatcher = nullptr;
        watcher->deleteLater();
        if (epoch != m_screenEpoch || !m_waitingForParent || !m_screenMode || !isVisible()) {
            // Explicitly revoke a handle that became available after cancellation.
            KWaylandExtras::unexportToplevel(this);
            m_parentExported = false;
            return;
        }
        const auto parent = handle.isEmpty() ? QString()
            : handle.startsWith(QStringLiteral("wayland:")) ? handle : QStringLiteral("wayland:") + handle;
        requestScreenColor(parent);
    });
    watcher->setFuture(KWaylandExtras::exportToplevel(this));
    QTimer::singleShot(1500, this, [this, epoch] {
        if (epoch != m_screenEpoch || !m_waitingForParent) return;
        releaseParentExport();
        requestScreenColor(QString());
    });
    Q_EMIT screenStateChanged();
    Q_EMIT sampleChanged();
}
