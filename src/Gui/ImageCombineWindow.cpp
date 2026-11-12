// SPDX-License-Identifier: GPL-3.0-only
#include "ImageCombineWindow.h"
#include "ImageCombineSession.h"
#include "PinnedImageWindow.h"
#include <QCloseEvent>
#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QScreen>
#include <QShortcut>
#include <QKeySequence>
#include <QFileDialog>

#include <KLocalizedString>

ImageCombineWindow::ImageCombineWindow(const QStringList &paths, bool keepAll, bool horizontal, QQmlEngine *engine)
    : QQuickView(engine, nullptr), m_session(new ImageCombineSession(engine, this))
{
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    setObjectName(QStringLiteral("nkshotImageCombineWindow"));
    setTitle(i18nc("@title:window", "Combine images — NkShot"));
    setMinimumSize({400, 420});
    setResizeMode(SizeRootObjectToView);
    auto context = new QQmlContext(engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("combineWindow"), this);
    const QUrl source(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/ImageCombineWindow.qml"));
    auto component = new QQmlComponent(engine, source, this);
    setContent(source, component, component->create(context));
    const auto available = screen()->availableGeometry().size();
    resize(qMax(400, qMin(900, available.width() - 80)), qMax(420, qMin(650, available.height() - 100)));
    m_session->setDirection(horizontal ? QStringLiteral("horizontal") : QStringLiteral("vertical"));
    m_session->setKeepAll(keepAll);
    QList<QUrl> urls;
    for (const auto &path : paths) urls.append(QUrl::fromLocalFile(path));
    connect(m_session, &ImageCombineSession::changed, this, &ImageCombineWindow::busyChanged);
    m_session->begin(urls);
    new QShortcut(QKeySequence::Close, this, this, &ImageCombineWindow::close);
    new QShortcut(QKeySequence(Qt::Key_Escape), this, this, &ImageCombineWindow::close);
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { delete this; });
}
ImageCombineWindow::~ImageCombineWindow() { delete m_chooseDialog.data(); }
bool ImageCombineWindow::busy() const { return m_session->busy() || choosingImages(); }
QObject *ImageCombineWindow::sessionObject() const { return m_session; }
QQuickWindow *ImageCombineWindow::createCopy()
{
    if (m_submitted || !m_session->ready() || busy()) return nullptr;
    const QImage image = m_session->snapshot();
    if (image.isNull()) return nullptr;
    auto pin = new PinnedImageWindow(image, engine());
    if (pin->status() != QQuickView::Ready) { delete pin; return nullptr; }
    pin->setObjectName(QStringLiteral("nkshotCombinedImagePin"));
    pin->setTitle(i18nc("@title:window", "Combined image — NkShot"));
    pin->setScreen(screen());
    pin->setPosition(position() + QPoint(30, 30));
    pin->show();
    m_submitted = true;
    close();
    return pin;
}
void ImageCombineWindow::closeEvent(QCloseEvent *event)
{
    if (m_chooseDialog) {
        auto dialog = m_chooseDialog.data();
        m_chooseDialog = nullptr;
        dialog->disconnect(this);
        delete dialog;
    }
    m_session->clear();
    QQuickView::closeEvent(event);
    deleteLater();
}

void ImageCombineWindow::chooseImages()
{
    if (m_submitted || busy()) return;
    m_selectionError.clear();
    auto dialog = new QFileDialog(nullptr, i18nc("@title:window", "Replace images to combine"));
    m_chooseDialog = dialog;
    dialog->setObjectName(QStringLiteral("nkshotCombineReviewFileDialog"));
    dialog->setFileMode(QFileDialog::ExistingFiles);
    dialog->setAcceptMode(QFileDialog::AcceptOpen);
    dialog->setNameFilter(i18nc("@item:inlistbox", "Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    dialog->setWindowModality(Qt::WindowModal);
    dialog->winId();
    dialog->windowHandle()->setTransientParent(this);
    connect(dialog, &QFileDialog::accepted, this, [this, dialog] {
        const auto paths = dialog->selectedFiles();
        m_chooseDialog = nullptr;
        dialog->deleteLater();
        // Invalid selection leaves the previous immutable group intact.
        if (paths.size() < 2 || paths.size() > 16) {
            m_selectionError = i18nc("@info", "Choose between 2 and 16 images to replace the list. Your current list is unchanged.");
            Q_EMIT busyChanged();
            return;
        }
        QList<QUrl> urls;
        for (const auto &path : paths) urls.append(QUrl::fromLocalFile(path));
        QPointer<ImageCombineWindow> guard(this);
        m_session->begin(urls);
        if (guard) Q_EMIT busyChanged();
    });
    connect(dialog, &QFileDialog::rejected, this, [this, dialog] {
        m_chooseDialog = nullptr;
        dialog->deleteLater();
        Q_EMIT busyChanged();
    });
    dialog->open();
    Q_EMIT busyChanged();
}
