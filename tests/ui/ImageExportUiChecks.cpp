// SPDX-License-Identifier: GPL-3.0-only
#include "ImageExportUiChecks.h"
#include "Gui/ImageCopyEditor.h"
#include <QApplication>
#include <QQuickItem>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QFile>
#include <QImageReader>
#include <QColorSpace>
#include <QPainter>
#include <QTimer>
#include <memory>
#include <QSignalSpy>
#include <QtTest/QTest>
namespace {
QQuickItem *find(QQuickItem *root, const QString &name) {
    if (!root) return nullptr;
    if (root->objectName() == name) return root;
    for (auto child : root->childItems()) if (auto result = find(child, name)) return result;
    return nullptr;
}
QFileDialog *dialog() {
    for (auto widget : QApplication::topLevelWidgets())
        if (widget->objectName() == QStringLiteral("nkshotCopyExportDialog") && widget->isVisible()) return qobject_cast<QFileDialog *>(widget);
    return nullptr;
}
QByteArray bytes(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
}
bool ImageExportUiChecks::run(QApplication &application, ImageCopyEditor *editor, const QString &prefix, QString *failure) {
    Q_UNUSED(application);
    auto fail = [&](const QString &message) { if (failure) *failure = message; if (editor) editor->grabWindow().save(prefix + QStringLiteral(".export-failed.png")); return false; };
    if (!editor || editor->annotationMode()) return fail(QStringLiteral("Export requires an image editor"));
    auto document = editor->document();
    const auto before = document->renderToImage().copy();
    const auto source = document->baseImage().copy();
    const auto history = document->undoStackDepth();
    const auto dirty = editor->hasUnsavedChanges();
    if (!dirty) return fail(QStringLiteral("Export fixture must contain unsaved edits"));
    auto unchanged = [&] { return document->renderToImage() == before && document->baseImage() == source
        && document->undoStackDepth() == history && editor->hasUnsavedChanges() == dirty; };
    auto click = [&](const QString &name) {
        auto item = find(editor->contentItem(), name);
        if (!item || !item->isVisible() || !item->isEnabled()) return false;
        const QRectF rect(item->mapToScene(QPointF()), item->size());
        if (!QRectF(QPointF(), editor->size()).contains(rect)) return false;
        editor->requestActivate(); QTest::qWait(60);
        std::unique_ptr<QSignalSpy> spy;
        if (item->metaObject()->indexOfSignal("clicked()") >= 0) spy=std::make_unique<QSignalSpy>(item,SIGNAL(clicked()));
        QTest::mouseClick(editor, Qt::LeftButton, Qt::NoModifier, rect.center().toPoint());
        if (spy) return QTest::qWaitFor([&] { return !spy->isEmpty(); }, 500);
        QTest::qWait(80); return true;
    };
    auto open = [&] {
        if (!click(QStringLiteral("nkshotCopyEditorSaveOptions"))) return false;
        auto action = find(editor->contentItem(), QStringLiteral("nkshotCopyEditorExportAction"));
        if (!action || !action->isVisible()) return false;
        QTest::mouseClick(editor, Qt::LeftButton, Qt::NoModifier, action->mapToScene(QPointF(action->width()/2, action->height()/2)).toPoint());
        return QTest::qWaitFor([&] { auto button=find(editor->contentItem(),QStringLiteral("nkshotImageExportContinue"));return button&&button->isVisible(); },1000);
    };
    const bool jpeg = editor->property("jpegAvailable").toBool(), webp = editor->property("webpAvailable").toBool();
    if (!jpeg && !webp) return fail(QStringLiteral("No export codec available for UI acceptance"));
    if (!open() || !click(QStringLiteral("nkshotImageExportCancel")) || !unchanged() || !open()) return fail(QStringLiteral("Export menu/cancel/reopen failed"));
    QTest::keyClick(editor, Qt::Key_Escape); QTest::qWait(100);
    QTemporaryDir folder;
    if (!folder.isValid()) return fail(QStringLiteral("Export temporary directory unavailable"));
    auto choose = [&](int format, int qualityIndex) {
        if (!open()) return false;
        auto combo=find(editor->contentItem(),QStringLiteral("nkshotImageExportFormat"));
        if (!combo) return false;
        if (jpeg && webp) {
            if (!click(QStringLiteral("nkshotImageExportFormat"))) return false;
            QTest::keyClick(editor,Qt::Key_Home);
            if(format==1)QTest::keyClick(editor,Qt::Key_Down);
            QTest::keyClick(editor,Qt::Key_Return);QTest::qWait(80);
        }
        if (combo->property("currentIndex").toInt() != (jpeg && webp ? format : 0)) return false;
        auto quality=find(editor->contentItem(),QStringLiteral("nkshotImageExportQuality"));
        if (format==0) {
            if (!quality || !quality->isVisible() || !click(QStringLiteral("nkshotImageExportQuality")))return false;
            QTest::keyClick(editor,Qt::Key_Home);
            for(int i=0;i<qualityIndex;++i)QTest::keyClick(editor,Qt::Key_Down);
            QTest::keyClick(editor,Qt::Key_Return);QTest::qWait(80);
            if (quality->property("currentIndex").toInt()!=qualityIndex)return false;
        } else if (quality && quality->isVisible()) return false;
        editor->grabWindow().save(prefix+(format==0?QStringLiteral(".jpeg-options.png"):QStringLiteral(".webp-options.png")));
        return click(QStringLiteral("nkshotImageExportContinue")) && QTest::qWaitFor([]{return dialog()!=nullptr;},1000);
    };
    auto submit = [&](const QString &path, bool overwrite) {
        QPointer<QFileDialog> picker=dialog();auto field=picker?picker->findChild<QLineEdit *>(QStringLiteral("fileNameEdit")):nullptr;
        auto box=picker?picker->findChild<QDialogButtonBox *>():nullptr;
        auto save=box?box->button(QDialogButtonBox::Save):nullptr;
        if(!field||!save)return false;
        QTest::mouseClick(field,Qt::LeftButton);QTest::keyClick(field,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(field,path);
        if(overwrite)QTimer::singleShot(150,[] {
            for(auto widget:QApplication::topLevelWidgets())if(auto confirm=qobject_cast<QMessageBox *>(widget))
                if(confirm->isVisible())if(auto yes=confirm->button(QMessageBox::Yes))QTest::mouseClick(yes,Qt::LeftButton);
        });
        QTest::mouseClick(save,Qt::LeftButton);
        return QTest::qWaitFor([&]{return picker.isNull();},1500);
    };
    QSignalSpy results(editor,&ImageCopyEditor::saveFinished);
    if(!choose(jpeg?0:1,0))return fail(QStringLiteral("Cancel-path file dialog unavailable"));
    QPointer<QFileDialog> cancelled=dialog();
    QTest::keyClick(cancelled,Qt::Key_Escape);
    if(!QTest::qWaitFor([&]{return cancelled.isNull();},1000)||!results.isEmpty()||!unchanged())
        return fail(QStringLiteral("File-dialog cancel changed export/session state"));
    if(!choose(jpeg?0:1,0))return fail(QStringLiteral("Save cancellation dialog unavailable"));
    auto cancelBox=dialog()->findChild<QDialogButtonBox *>();
    auto cancelButton=cancelBox?cancelBox->button(QDialogButtonBox::Cancel):nullptr;
    if(!cancelButton)return fail(QStringLiteral("Save dialog Cancel unavailable"));
    QTest::mouseClick(cancelButton,Qt::LeftButton);
    if(!QTest::qWaitFor([]{return dialog()==nullptr;},1000)||!results.isEmpty()||!unchanged())return fail(QStringLiteral("Save cancellation changed edit or emitted result"));
    for(int format=0;format<2;++format) {
        if((format==0&&!jpeg)||(format==1&&!webp))continue;
        if(!choose(format,1))return fail(QStringLiteral("Actual export format/quality/dialog workflow failed"));
        auto existing=dialog();
        if(editor->exportAs(format,95)||dialog()!=existing)return fail(QStringLiteral("Repeated export opened another dialog"));
        const auto path=folder.filePath(format==0?QStringLiteral("share.jpg"):QStringLiteral("share.webp"));
        results.clear();
        if(!submit(path,false)||!QTest::qWaitFor([&]{return !results.isEmpty();},1000)||!results.last().at(1).toBool())return fail(QStringLiteral("Export success signal absent"));
        const auto encoded=bytes(path);QImage decoded(path);
        if(decoded.size()!=before.size()||!unchanged())return fail(QStringLiteral("Export changed dimensions/history/source/dirty state"));
        QImage expected=before.colorSpace().isValid()?before.convertedToColorSpace(QColorSpace::SRgb):before;
        if(format==1) {
            if(!encoded.startsWith("RIFF")||encoded.mid(8,4)!="WEBP")return fail(QStringLiteral("Lossless WebP signature missing"));
            const auto actualPixels=decoded.convertToFormat(QImage::Format_ARGB32_Premultiplied),expectedPixels=expected.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            for(int y=0;y<actualPixels.height();++y)for(int x=0;x<actualPixels.width();++x)
                if(actualPixels.pixel(x,y)!=expectedPixels.pixel(x,y))return fail(QStringLiteral("Lossless WebP pixels differ"));
        } else {
            if(!encoded.startsWith(QByteArray::fromHex("ffd8")))return fail(QStringLiteral("JPEG signature missing"));
            QImage flattened(before.size(),QImage::Format_RGB32);flattened.fill(Qt::white);expected.setDevicePixelRatio(1);
            {QPainter painter(&flattened);painter.drawImage(0,0,expected);}
            quint64 error=0;
            for(int y=0;y<decoded.height();++y)for(int x=0;x<decoded.width();++x){const auto a=decoded.pixelColor(x,y),b=flattened.pixelColor(x,y);if(a.alpha()!=255)return fail(QStringLiteral("JPEG retained alpha"));error+=qAbs(a.red()-b.red())+qAbs(a.green()-b.green())+qAbs(a.blue()-b.blue());}
            if(double(error)/(3.0*decoded.width()*decoded.height())>20)return fail(QStringLiteral("JPEG pixels exceed lossy tolerance"));
        }
    }
    const int supported=jpeg?0:1;
    if(!choose(supported,0))return fail(QStringLiteral("Error-path dialog unavailable"));
    const auto sentinel=folder.filePath(QStringLiteral("preserve.png"));{QFile file(sentinel);if(!file.open(QIODevice::WriteOnly)||file.write("keep-sentinel")<0)return fail(QStringLiteral("Sentinel creation failed"));}
    results.clear();
    if(!submit(sentinel,true)||!QTest::qWaitFor([&]{return !results.isEmpty();},1000)||results.last().at(1).toBool()||bytes(sentinel)!="keep-sentinel"||!unchanged())return fail(QStringLiteral("Wrong suffix did not preserve sentinel and emit failure"));
    qInfo()<<"107 actual image export menu, cancel/reopen, codecs, pixels, duplicate-dialog guard, error and edit isolation passed";
    return true;
}
