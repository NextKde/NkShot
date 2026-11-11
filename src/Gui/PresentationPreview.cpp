// SPDX-License-Identifier: GPL-3.0-only
#include "PresentationPreview.h"
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QColorSpace>
#include <QMutex>
#include <QMutexLocker>
#include <KLocalizedString>
class PresentationImageProvider final : public QQuickImageProvider {
public:
    PresentationImageProvider() : QQuickImageProvider(Image) {}
    void setImage(const QImage &image) { QMutexLocker lock(&mutex); current = image; }
    QImage requestImage(const QString &, QSize *size, const QSize &) override {
        QMutexLocker lock(&mutex);
        if (size) *size = current.size();
        return current;
    }
private:
    QMutex mutex;
    QImage current;
};
PresentationPreview::PresentationPreview(QQmlEngine *engine, QObject *parent)
    : QObject(parent), m_engine(engine)
{
    static quint64 next = 0;
    m_providerId = QStringLiteral("nkshotpresentation%1").arg(++next);
    m_provider = new PresentationImageProvider;
    engine->addImageProvider(m_providerId, m_provider);
}
PresentationPreview::~PresentationPreview()
{
    if (m_engine) m_engine->removeImageProvider(m_providerId);
}
bool PresentationPreview::begin(const QImage &source)
{
    clear();
    if (source.isNull() || source.width() > 8192 || source.height() > 32000
        || qint64(source.width()) * source.height() > 40000000) return false;
    // Own pixels even if the caller wraps mutable external memory. Convert only
    // once, not every time a watermark character or padding step changes.
    m_source = source.colorSpace().isValid() && source.colorSpace() != QColorSpace(QColorSpace::SRgb)
        ? source.convertedToColorSpace(QColorSpace::SRgb) : source.copy();
    if (m_source.isNull()) return false;
    m_source.setColorSpace(QColorSpace::SRgb);
    m_options = {};
    Q_EMIT changed();
    return true;
}
void PresentationPreview::clear()
{
    m_source = {}; m_ready = false; m_error.clear(); ++m_revision;
    if (m_engine && m_provider) m_provider->setImage({});
    Q_EMIT changed();
}
QUrl PresentationPreview::imageSource() const
{
    return m_ready ? QUrl(QStringLiteral("image://%1/%2").arg(m_providerId).arg(m_revision)) : QUrl();
}
void PresentationPreview::update(int padding, int style, const QString &watermark, bool rounded)
{
    if (m_source.isNull() || !m_engine) return;
    m_options = {padding, NkShot::Imaging::PresentationStyle(style), watermark, rounded};
    const auto preview = NkShot::Imaging::previewPresentation(m_source, m_options);
    m_ready = !preview.isNull();
    m_error = m_ready ? QString() : i18nc("@info", "This copy cannot be previewed. Reduce padding or use a smaller image.");
    m_provider->setImage(preview); ++m_revision;
    Q_EMIT changed();
}
