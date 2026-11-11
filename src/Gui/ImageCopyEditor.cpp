// SPDX-License-Identifier: GPL-3.0-only
#include "RecentMediaModel.h"
#include "ImageCopyEditor.h"
#include "PinnedImageWindow.h"
#include "Imaging/ImageExport.h"
#include <KQuickImageEditor/AnnotationTool>
#include <KLocalizedString>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMessageBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QQuickItem>
#include <QScreen>
#include <QFileDialog>
#include <QImageWriter>
#include <QSaveFile>
#include <QStandardPaths>

ImageCopyEditor::ImageCopyEditor(const QImage &image, QQmlEngine *engine, bool annotationMode)
    : QQuickView(engine, nullptr), m_document(new AnnotationDocument(this)), m_originalImage(image.copy()), m_annotationMode(annotationMode)
{
    // C++ owns close/draft lifetime; QML return values must not become GC-owned.
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    connect(this, &QWindow::activeChanged, this, [this] { if (!isActive()) endComparison(); });
    connect(this, &QWindow::visibleChanged, this, [this](bool visible) { if (!visible) endComparison(); });
    setObjectName(QStringLiteral("nkshotImageCopyEditor"));
    setTitle(m_annotationMode ? i18nc("@title:window", "Video annotations — NkShot")
                             : i18nc("@title:window", "Edit image copy — NkShot"));
    setMinimumSize({400, 400});
    setResizeMode(SizeRootObjectToView);
    // One owned input snapshot shared through QImage copy-on-write, never edited.
    auto reference = m_originalImage;
    if (m_annotationMode) reference.setDevicePixelRatio(1);
    m_document->setBaseImage(reference);
    if (m_annotationMode) {
        // The reference frame is never part of the committed overlay.
        m_referenceImage = m_document->baseImage();
        m_document->renderToImage(); // Populate the document's base/annotation caches.
        m_referenceCanvas = m_document->canvasBaseImage();
        connect(m_document->tool(), &AnnotationTool::typeChanged, this, [this] {
            switch (m_document->tool()->type()) {
            case AnnotationTool::NoTool:
            case AnnotationTool::SelectTool:
            case AnnotationTool::FreehandTool:
            case AnnotationTool::HighlighterTool:
            case AnnotationTool::LineTool:
            case AnnotationTool::ArrowTool:
            case AnnotationTool::RectangleTool:
            case AnnotationTool::EllipseTool:
            case AnnotationTool::TextTool:
            case AnnotationTool::NumberTool:
                break;
            default:
                m_document->tool()->setType(AnnotationTool::SelectTool);
            }
        });
    }
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
void ImageCopyEditor::beginComparison()
{
    if (m_comparing || !compareAvailable() || !isVisible() || !isActive()
        || m_optionsDialog || m_closeDialog || m_saveDialog) return;
    m_comparing = true;
    Q_EMIT comparingChanged();
}
void ImageCopyEditor::endComparison()
{
    if (!m_comparing) return;
    m_comparing = false;
    Q_EMIT comparingChanged();
}
void ImageCopyEditor::prepareDialog(QWidget *dialog)
{
    endComparison();
    m_optionsDialog=dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->winId();dialog->windowHandle()->setTransientParent(this);
    // Prevent changing the selected item while a style chooser is open.
    dialog->setWindowModality(Qt::WindowModal);
    connect(this,&QObject::destroyed,dialog,&QObject::deleteLater);
}
void ImageCopyEditor::copyImage()
{
    endComparison();
    if (m_annotationMode) return;
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
    endComparison();
    if (m_annotationMode) return;
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
bool ImageCopyEditor::jpegAvailable() const
{
    return NkShot::Imaging::imageExportAvailable(NkShot::Imaging::ImageExportFormat::Jpeg);
}
bool ImageCopyEditor::webpAvailable() const
{
    return NkShot::Imaging::imageExportAvailable(NkShot::Imaging::ImageExportFormat::WebpLossless);
}
bool ImageCopyEditor::exportAs(int format, int quality)
{
    endComparison();
    if (m_annotationMode) return false;
    if (m_saveDialog) { m_saveDialog->raise(); m_saveDialog->activateWindow(); return false; }
    if (m_optionsDialog) { m_optionsDialog->raise(); return false; }
    if (m_closeDialog) { m_closeDialog->raise(); return false; }
    if ((format != 0 && format != 1) || (format == 0 && (quality < 1 || quality > 100))) {
        Q_EMIT saveFinished(i18nc("@info", "Choose a supported export format and quality."), false); return false;
    }
    const auto selected = format == 0 ? NkShot::Imaging::ImageExportFormat::Jpeg : NkShot::Imaging::ImageExportFormat::WebpLossless;
    if (!NkShot::Imaging::imageExportAvailable(selected)) {
        Q_EMIT saveFinished(i18nc("@info", "This image format is unavailable. Save a PNG instead."), false); return false;
    }
    if (rootObject()) QMetaObject::invokeMethod(rootObject(), "commitOptions");
    m_document->selectedItemWrapper()->commitChanges();
    const QImage snapshot = m_document->renderToImage();
    if (snapshot.isNull()) return false;
    auto dialog = new QFileDialog;
    m_saveDialog = dialog;
    dialog->setObjectName(QStringLiteral("nkshotCopyExportDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(i18nc("@title:window", "Export Sharing Copy"));
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setNameFilter(format == 0 ? i18nc("@item:inlistbox", "JPEG image (*.jpg *.jpeg)") : i18nc("@item:inlistbox", "WebP image (*.webp)"));
    const auto suffix = format == 0 ? QStringLiteral("jpg") : QStringLiteral("webp");
    dialog->setDefaultSuffix(suffix);
    dialog->setDirectory(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    dialog->selectFile(QStringLiteral("NkShot-edited.") + suffix);
    dialog->winId(); dialog->windowHandle()->setTransientParent(this);
    dialog->setWindowModality(Qt::WindowModal);
    connect(dialog, &QFileDialog::accepted, this, [this, dialog, snapshot, selected, quality] {
        const auto paths = dialog->selectedFiles();
        if (paths.isEmpty()) return;
        QString error;
        if (!NkShot::Imaging::saveImageCopy(snapshot, paths.first(), selected, quality, &error)) {
            Q_EMIT saveFinished(i18nc("@info", "Could not export image: %1", error), false); return;
        }
        // A sharing export is not the lossless PNG save point. Do not clear
        // unsaved-change protection, including for a lossy JPEG destination.
        NkShot::RecentMediaModel::instance()->remember(QUrl::fromLocalFile(paths.first()));
        Q_EMIT saveFinished(i18nc("@info", "Sharing copy exported. This editing session is unchanged; Save still creates a PNG."), true);
    });
    dialog->open();
    return true;
}
QQuickWindow *ImageCopyEditor::finishCopy()
{
    endComparison();
    if (m_annotationMode) { commitAnnotations(); return nullptr; }
    const auto image=m_document->renderToImage();
    if(image.isNull())return nullptr;
    auto result=new PinnedImageWindow(image,engine());
    if(result->status()!=QQuickView::Ready){delete result;return nullptr;}
    result->setObjectName(QStringLiteral("nkshotEditedImagePin"));
    result->setTitle(i18nc("@title:window", "Edited image copy — NkShot"));
    result->setScreen(screen());result->setPosition(position()+QPoint(30,30));result->show();
    m_acceptClose=true;close();return result;
}
bool ImageCopyEditor::commitAnnotations()
{
    if (!m_annotationMode) return false;
    if (m_optionsDialog) { m_optionsDialog->raise(); return false; }
    // Window-close must commit pending text/options just like the Done action.
    if (rootObject()) QMetaObject::invokeMethod(rootObject(), "commitOptions");
    m_document->selectedItemWrapper()->commitChanges();
    m_document->renderToImage(); // annotationsImage() alone returns the lazy cache.
    const QRectF expected(QPointF(0, 0), QSizeF(m_referenceImage.size()));
    if (m_referenceImage.isNull() || m_document->canvasRect() != expected
        || !m_document->transform().isIdentity() || m_document->imageDpr() != 1
        || m_document->baseImage() != m_referenceImage
        || m_document->canvasBaseImage() != m_referenceCanvas) {
        Q_EMIT saveFinished(i18nc("@info", "Video annotations must keep the original frame size and pixels. Undo image changes before returning."), false);
        return false;
    }
    auto overlay = m_document->annotationsImage().copy();
    if (!overlay.isNull() && (overlay.size() != m_referenceImage.size() || !overlay.hasAlphaChannel())) {
        Q_EMIT saveFinished(i18nc("@info", "The transparent annotation layer could not be prepared."), false);
        return false;
    }
    if (!overlay.isNull()) {
        overlay = overlay.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        overlay.setDevicePixelRatio(1);
        bool visibleMark = false;
        for (int y = 0; y < overlay.height() && !visibleMark; ++y) {
            const auto row = reinterpret_cast<const QRgb *>(overlay.constScanLine(y));
            for (int x = 0; x < overlay.width(); ++x) {
                if (qAlpha(row[x])) { visibleMark = true; break; }
            }
        }
        if (!visibleMark) overlay = QImage();
    }
    // Hide, do not delete: reopening this export keeps editable marks and undo.
    hide();
    Q_EMIT annotationsCommitted(overlay);
    return true;
}
void ImageCopyEditor::showColorDialog(int option)
{
    endComparison();
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
    endComparison();
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
    if (m_comparing) {
        if (event->type() == QEvent::InputMethod || event->type() == QEvent::ShortcutOverride) {
            event->accept();
            return true; // No hidden text commits or focus-item shortcut interception.
        }
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            const auto key = static_cast<QKeyEvent *>(event);
            if (event->type() == QEvent::KeyPress) {
                if (key->key() == Qt::Key_Escape) endComparison();
                else if (key->matches(QKeySequence::Copy)) copyImage();
                else if (key->matches(QKeySequence::Save) || key->matches(QKeySequence::SaveAs)) saveAs();
            } else if (key->key() == Qt::Key_Space && !key->isAutoRepeat()) {
                endComparison();
            }
            event->accept();
            return true;
        }
    }
    if (event->type() == QEvent::Close || event->type() == QEvent::Hide
        || event->type() == QEvent::FocusOut || event->type() == QEvent::WindowDeactivate)
        endComparison();
    if (event->type() == QEvent::Close && m_annotationMode) {
        static_cast<QCloseEvent *>(event)->ignore();
        commitAnnotations();
        return true;
    }
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
