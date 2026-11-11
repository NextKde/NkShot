// SPDX-License-Identifier: GPL-3.0-only
#include "QuickNoteWindow.h"
#include "PinnedImageWindow.h"
#include "Imaging/NoteCard.h"
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQuickImageProvider>
#include <QMutex>
#include <QMutexLocker>
#include <QGuiApplication>
#include <QClipboard>
#include <QShortcut>
#include <KLocalizedString>
class NoteImageProvider : public QQuickImageProvider {
public:
    NoteImageProvider():QQuickImageProvider(Image){}
    void setImage(const QImage &image) {QMutexLocker lock(&mutex);value=image;}
    QImage requestImage(const QString &,QSize *size,const QSize &) override {QMutexLocker lock(&mutex);if(size)*size=value.size();return value;}
private: QMutex mutex;QImage value;
};
static quint64 noteId=0;
QuickNoteWindow::QuickNoteWindow(QQmlEngine *engine):QQuickView(engine,nullptr),m_engine(engine),m_provider(new NoteImageProvider),m_providerId(QStringLiteral("nkshotnote%1").arg(++noteId))
{
    setObjectName(QStringLiteral("nkshotQuickNoteWindow"));setTitle(i18nc("@title:window","Quick Note — NkShot"));
    setMinimumSize({400,480});setResizeMode(SizeRootObjectToView);
    engine->addImageProvider(m_providerId,m_provider);
    auto context=new QQmlContext(engine->rootContext(),this);context->setContextProperty(QStringLiteral("noteWindow"),this);
    const QUrl source(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/QuickNote.qml"));
    auto component=new QQmlComponent(engine,source,this);setContent(source,component,component->create(context));
    resize(900,620);
    new QShortcut(QKeySequence::Close,this,this,&QuickNoteWindow::close);
    // Closing hides the session draft. Reopening restores this same editor.
    connect(QCoreApplication::instance(),&QCoreApplication::aboutToQuit,this,[this]{delete this;});
}
QuickNoteWindow::~QuickNoteWindow(){if(m_engine)m_engine->removeImageProvider(m_providerId);}
QUrl QuickNoteWindow::imageSource() const {return m_image.isNull()?QUrl():QUrl(QStringLiteral("image://%1/%2").arg(m_providerId).arg(m_revision));}
void QuickNoteWindow::updatePreview(const QString &text,int style)
{
    m_image=NkShot::Imaging::renderNoteCard(text,style);
    m_error=(!text.trimmed().isEmpty() && m_image.isNull())?i18nc("@info","Keep the note under 2,000 characters and shorten very tall text."):QString();
    m_provider->setImage(m_image);++m_revision;Q_EMIT previewChanged();
}
void QuickNoteWindow::copyImage(){if(canExport()){QGuiApplication::clipboard()->setImage(m_image);Q_EMIT statusMessage(i18nc("@info","Note image copied"));}}
QQuickWindow *QuickNoteWindow::pinImage()
{
    if(!canExport())return nullptr;
    auto pin=new PinnedImageWindow(m_image,m_engine);if(pin->status()!=QQuickView::Ready){delete pin;return nullptr;}
    pin->setScreen(screen());pin->show();pin->requestActivate();Q_EMIT statusMessage(i18nc("@info","Note pinned. Use Save As in its toolbar to keep a PNG."));return pin;
}

void QuickNoteWindow::hideDraftsForCapture()
{
    // Keep the private draft in memory, but do not include the editor in a capture.
    // Reopen explicitly from Pins and notes; never pop it over a capture selector.
    for (auto window : QGuiApplication::allWindows()) {
        if (auto note = qobject_cast<QuickNoteWindow *>(window)) {
            note->hide();
        }
    }
}
