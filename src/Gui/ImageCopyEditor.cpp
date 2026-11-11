// SPDX-License-Identifier: GPL-3.0-only
#include "RecentMediaModel.h"
#include "ImageCopyEditor.h"
#include "PinnedImageWindow.h"
#include <KQuickImageEditor/AnnotationTool>
#include <KLocalizedString>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QMessageBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QQuickItem>
#include <QScreen>
#include <QFileDialog>
#include <QImageWriter>
#include <QSaveFile>
#include <QStandardPaths>

ImageCopyEditor::ImageCopyEditor(const QImage &image, QQmlEngine *engine) : QQuickView(engine, nullptr), m_document(new AnnotationDocument(this))
{
    setObjectName(QStringLiteral("nkshotImageCopyEditor"));
    setTitle(i18nc("@title:window", "Edit image copy — NkShot"));
    setMinimumSize({400, 400});
    setResizeMode(SizeRootObjectToView);
    m_document->setBaseImage(image.copy());
    m_document->tool()->setType(AnnotationTool::SelectTool);
    auto context = new QQmlContext(engine->rootContext(), this);
    context->setContextProperty(QStringLiteral("copyEditor"), this);
    context->setContextProperty(QStringLiteral("contextWindow"), this);
    const QUrl source(QStringLiteral("qrc:/qt/qml/org/kde/spectacle/private/Gui/ImageCopyEditor.qml"));
    auto component = new QQmlComponent(engine, source, this);
    setContent(source, component, component->create(context));
    const auto area = screen()->availableGeometry().size();
    resize(qMin(900, area.width()), qMin(640, area.height()));
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] { delete this; });
}
ImageCopyEditor::~ImageCopyEditor() { delete m_optionsDialog; delete m_closeDialog; delete m_saveDialog; }
void ImageCopyEditor::prepareDialog(QWidget *dialog)
{
    m_optionsDialog=dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->winId();dialog->windowHandle()->setTransientParent(this);
    // Prevent changing the selected item while a style chooser is open.
    dialog->setWindowModality(Qt::WindowModal);
    connect(this,&QObject::destroyed,dialog,&QObject::deleteLater);
}
void ImageCopyEditor::copyImage()
{
    const auto image=m_document->renderToImage();
    if(image.isNull())return;
    QApplication::clipboard()->setImage(image);Q_EMIT imageCopied();
}
bool ImageCopyEditor::hasUnsavedChanges() const
{
    return m_savedImage.isNull() ? m_document->undoStackDepth() > 0
        : m_document->renderToImage() != m_savedImage;
}
void ImageCopyEditor::saveAs()
{
    if(m_saveDialog){m_saveDialog->raise();m_saveDialog->activateWindow();return;}
    // Capture exactly the requested revision; never mutate global export settings.
    const QImage snapshot=m_document->renderToImage();
    if(snapshot.isNull())return;
    auto dialog=new QFileDialog;
    m_saveDialog=dialog;
    dialog->setObjectName(QStringLiteral("nkshotCopySaveDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window","Save Edited Image Copy"));
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter(i18nc("@item:inlistbox","PNG image (*.png)"));
    dialog->setDefaultSuffix(QStringLiteral("png"));
    dialog->setDirectory(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    dialog->selectFile(QStringLiteral("NkShot-edited.png"));
    dialog->winId();dialog->windowHandle()->setTransientParent(this);
    dialog->setWindowModality(Qt::WindowModal);
    connect(dialog,&QFileDialog::accepted,this,[this,dialog,snapshot]{
        const auto paths=dialog->selectedFiles();
        if(paths.isEmpty())return;
        if(!paths.first().endsWith(QStringLiteral(".png"),Qt::CaseInsensitive)){
            Q_EMIT saveFinished(i18nc("@info","Use a .png filename to save this image."),false);return;
        }
        QSaveFile file(paths.first());
        if(!file.open(QIODevice::WriteOnly)){
            Q_EMIT saveFinished(i18nc("@info","Could not save image: %1",file.errorString()),false);return;
        }
        QImageWriter writer(&file,"png");
        if(!writer.write(snapshot)){
            file.cancelWriting();Q_EMIT saveFinished(i18nc("@info","Could not save image: %1",writer.errorString()),false);return;
        }
        if(!file.commit()){
            Q_EMIT saveFinished(i18nc("@info","Could not save image: %1",file.errorString()),false);return;
        }
        m_savedImage=snapshot;
        NkShot::RecentMediaModel::instance()->remember(QUrl::fromLocalFile(paths.first()));
        Q_EMIT saveFinished(i18nc("@info","PNG saved. You can continue editing; further changes need another save."),true);
    });
    dialog->open();
}
QQuickWindow *ImageCopyEditor::finishCopy()
{
    const auto image=m_document->renderToImage();
    if(image.isNull())return nullptr;
    auto result=new PinnedImageWindow(image,engine());
    if(result->status()!=QQuickView::Ready){delete result;return nullptr;}
    result->setObjectName(QStringLiteral("nkshotEditedImagePin"));
    result->setTitle(i18nc("@title:window", "Edited image copy — NkShot"));
    result->setScreen(screen());result->setPosition(position()+QPoint(30,30));result->show();
    m_acceptClose=true;close();return result;
}
void ImageCopyEditor::showColorDialog(int option)
{
    if(m_optionsDialog){m_optionsDialog->raise();return;}
    auto tool=m_document->tool();auto selected=m_document->selectedItemWrapper();
    const bool useSelected=tool->type()==AnnotationTool::SelectTool
        ||(tool->type()==AnnotationTool::TextTool&&selected->options().testFlag(AnnotationTool::TextOption));
    QColor initial;
    if(option==AnnotationTool::StrokeOption)initial=useSelected?selected->strokeColor():tool->strokeColor();
    else if(option==AnnotationTool::FillOption)initial=useSelected?selected->fillColor():tool->fillColor();
    else if(option==AnnotationTool::FontOption)initial=useSelected?selected->fontColor():tool->fontColor();
    else return;
    auto dialog=new QColorDialog(initial);dialog->setOption(QColorDialog::ShowAlphaChannel);prepareDialog(dialog);
    dialog->setObjectName(QStringLiteral("nkshotCopyColorDialog"));
    connect(dialog,&QColorDialog::colorSelected,this,[this,option,useSelected](const QColor &color){
        auto tool=m_document->tool();auto selected=m_document->selectedItemWrapper();
        const bool updateTool=!useSelected||tool->type()==AnnotationTool::TextTool;
        if(option==AnnotationTool::StrokeOption){if(updateTool)tool->setStrokeColor(color);if(useSelected)selected->setStrokeColor(color);}
        if(option==AnnotationTool::FillOption){if(updateTool)tool->setFillColor(color);if(useSelected)selected->setFillColor(color);}
        if(option==AnnotationTool::FontOption){if(updateTool)tool->setFontColor(color);if(useSelected)selected->setFontColor(color);}
        if(useSelected)selected->commitChanges();
    });
    dialog->open();
}
void ImageCopyEditor::showFontDialog()
{
    if(m_optionsDialog){m_optionsDialog->raise();return;}
    auto tool=m_document->tool();auto selected=m_document->selectedItemWrapper();
    const bool useSelected=tool->type()==AnnotationTool::SelectTool
        ||(tool->type()==AnnotationTool::TextTool&&selected->options().testFlag(AnnotationTool::TextOption));
    auto dialog=new QFontDialog(useSelected?selected->font():tool->font());prepareDialog(dialog);
    dialog->setObjectName(QStringLiteral("nkshotCopyFontDialog"));
    connect(dialog,&QFontDialog::fontSelected,this,[this,useSelected](QFont font){
        if(font.weight()==QFont::Normal&&(font.styleName()==QStringLiteral("Regular")||font.styleName()==QStringLiteral("Normal")
            ||font.styleName()==QStringLiteral("Book")||font.styleName()==QStringLiteral("Roman")))font.setStyleName({});
        if(!useSelected||m_document->tool()->type()==AnnotationTool::TextTool)m_document->tool()->setFont(font);
        if(useSelected){m_document->selectedItemWrapper()->setFont(font);m_document->selectedItemWrapper()->commitChanges();}
    });
    dialog->open();
}
bool ImageCopyEditor::event(QEvent *event)
{
    if(event->type()==QEvent::Close&&m_saveDialog){
        static_cast<QCloseEvent *>(event)->ignore();m_saveDialog->raise();return true;
    }
    if(event->type()==QEvent::Close&&!m_acceptClose&&hasUnsavedChanges()){
        static_cast<QCloseEvent *>(event)->ignore();
        if(m_closeDialog){m_closeDialog->raise();return true;}
        auto dialog=new QMessageBox(QMessageBox::Question,i18nc("@title:window","Discard this edited copy?"),
            i18nc("@info","The original image is unchanged. Save a PNG or use Done to keep this edited copy before closing."),QMessageBox::Discard|QMessageBox::Cancel);
        m_closeDialog=dialog;dialog->setObjectName(QStringLiteral("nkshotDiscardCopyDialog"));
        dialog->setDefaultButton(QMessageBox::Cancel);dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->winId();dialog->windowHandle()->setTransientParent(this);dialog->setWindowModality(Qt::WindowModal);
        connect(dialog,&QMessageBox::finished,this,[this](int result){if(result==QMessageBox::Discard){m_acceptClose=true;close();}});
        dialog->open();return true;
    }
    const bool result=QQuickView::event(event);
    if(event->type()==QEvent::Close&&event->isAccepted())deleteLater();
    return result;
}
