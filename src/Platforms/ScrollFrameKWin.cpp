// SPDX-License-Identifier: GPL-3.0-only
#include "ScrollFrameKWin.h"
#include "ImagePlatformKWin.h"
#include "Imaging/ScrollFrameCrop.h"
#include "ImageMetaData.h"
#include <QDBusConnection>
#include <QGuiApplication>
#include <QScreen>
#include <KLocalizedString>

ScrollFrameKWin::ScrollFrameKWin(QObject *parent) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(6000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        const auto ticket = m_ticket;
        cancel();
        Q_EMIT requestFailed(ticket, i18nc("@info", "The scroll frame timed out. Your accepted frames are unchanged."));
    });
}
ScrollFrameKWin::~ScrollFrameKWin() { cancel(); }
void ScrollFrameKWin::cancel()
{
    m_timeout.stop(); ++m_generation;
    // ScreenShotSource2's context-bound continuation cannot invoke a destroyed source.
    delete m_source.data(); m_source = nullptr; m_ticket = 0;
}
bool ScrollFrameKWin::request(quint64 ticket, QScreen *screen, QRect localPixels)
{
    if (busy() || !ticket) return false;
    if (!screen || !qGuiApp->screens().contains(screen) || screen->name().isEmpty()
        || !QDBusConnection::sessionBus().isConnected()) return false;
    const auto geometry = screen->geometry();
    const auto dpr = screen->devicePixelRatio();
    const QSize pixels(qRound(geometry.width()*dpr), qRound(geometry.height()*dpr));
    if (!localPixels.isValid() || !QRect(QPoint(),pixels).contains(localPixels)
        || localPixels.width() > 8192 || localPixels.height() > 32000
        || qint64(localPixels.width())*localPixels.height() > 40000000) return false;
    const QPointer<QScreen> watchedScreen(screen);
    const auto outputId = screen->name();
    m_ticket = ticket;
    const auto generation = ++m_generation;
    auto source = new ScreenShotSourceScreen2(screen, ImagePlatformKWin::ScreenShotFlag::NativeSize);
    source->setParent(this); m_source = source;
    connect(source, &ScreenShotSource2::finished, this,
        [this, source, ticket, generation, watchedScreen, geometry, dpr, pixels, localPixels, outputId](const ResultVariant &result) {
        if (m_generation != generation || m_source != source || m_ticket != ticket) return;
        m_timeout.stop(); m_source = nullptr; m_ticket = 0;
        source->deleteLater();
        if (!watchedScreen || !qGuiApp->screens().contains(watchedScreen)
            || watchedScreen->geometry() != geometry || watchedScreen->devicePixelRatio() != dpr || watchedScreen->name() != outputId) {
            Q_EMIT requestFailed(ticket, i18nc("@info", "The display changed. Start a new scroll capture region."));
            return;
        }
        if (result.index() != ResultVariant::Image) {
            Q_EMIT requestFailed(ticket, result.index() == ResultVariant::ErrorString ? std::get<QString>(result)
                : i18nc("@info", "The frame capture was cancelled."));
            return;
        }
        const auto &image = std::get<QImage>(result);
        const auto returnedOutput = ImageMetaData::screen(image);
        const auto frame = returnedOutput.isEmpty() || returnedOutput == outputId
            ? NkShot::Imaging::cropScrollFrame(image, pixels, localPixels, dpr) : QImage{};
        if (frame.isNull()) {
            Q_EMIT requestFailed(ticket, i18nc("@info", "The captured frame does not match the fixed region or scale. No pixels were added."));
            return;
        }
        Q_EMIT frameReady(ticket, frame);
    });
    m_timeout.start();
    return true;
}
