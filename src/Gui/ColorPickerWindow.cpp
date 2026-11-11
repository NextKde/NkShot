// SPDX-License-Identifier: GPL-3.0-only
#include "ColorPickerWindow.h"
#include "Imaging/ColorSample.h"

#include <KLocalizedString>
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
    resize(820, 620);
    new QShortcut(QKeySequence::Copy, this, this, &ColorPickerWindow::copyHex);
    new QShortcut(QKeySequence::Close, this, this, &ColorPickerWindow::close);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, this, &ColorPickerWindow::close);
}

ColorPickerWindow::~ColorPickerWindow()
{
    if (m_engine) m_engine->removeImageProvider(m_providerId);
}

bool ColorPickerWindow::event(QEvent *event)
{
    const auto type = event->type();
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
    return hasSample() ? QUrl(QStringLiteral("image://%1/sample/%2").arg(m_providerId).arg(m_revision)) : QUrl();
}

QColor ColorPickerWindow::sampleColor() const
{
    return hasSample() ? m_image.pixelColor(m_pixel) : QColor();
}

QString ColorPickerWindow::hexValue() const { return NkShot::Imaging::colorSampleHex(sampleColor()); }
QString ColorPickerWindow::rgbValue() const { return NkShot::Imaging::colorSampleRgb(sampleColor()); }

void ColorPickerWindow::selectPixel(QPoint pixel)
{
    if (!m_image.rect().contains(pixel) || pixel == m_pixel) return;
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
    if (!valid()) return;
    if (!hasSample()) { selectPixel({m_image.width() / 2, m_image.height() / 2}); return; }
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
