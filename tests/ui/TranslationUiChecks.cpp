// SPDX-License-Identifier: GPL-3.0-only
// Synthetic immutable OCR inputs; translations are produced by the actual models.
#include "TranslationUiChecks.h"
#include "Gui/OcrTextWindow.h"
#include "Gui/ViewerWindow.h"
#include "SpectacleCore.h"
#include <KQuickImageEditor/AnnotationDocument>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QQuickItem>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QRegularExpression>
#include <QtTest/QTest>

bool TranslationUiChecks::run(QApplication &application,const QString &screenshot,QString *failure)
{
    const auto fail=[&](const QString &message){if(failure)*failure=message;return false;};
    auto viewer=ViewerWindow::instance();
    auto core=SpectacleCore::instance();
    if(!viewer||!core)return fail(QStringLiteral("Viewer unavailable"));
    const QList<QByteArray> keys{"NKSHOT_TRANSLATION_PYTHON","NKSHOT_TRANSLATION_RUNNER","NKSHOT_TRANSLATION_MODEL_DIR"};
    QList<QByteArray> old;QList<bool> existed;
    for(const auto &key:keys){old.append(qgetenv(key.constData()));existed.append(qEnvironmentVariableIsSet(key.constData()));}
    const auto restoreEnv=qScopeGuard([&]{for(int i=0;i<keys.size();++i){if(existed[i])qputenv(keys[i].constData(),old[i]);else qunsetenv(keys[i].constData());}});
    const bool defaultDiscovery=qEnvironmentVariableIsSet("NKSHOT_TEST_TRANSLATION_DEFAULT");
    if(defaultDiscovery){for(const auto &key:keys)qunsetenv(key.constData());}
    else if(!existed[0]&&!existed[1]&&!existed[2]){
        qputenv(keys[0].constData(),"/opt/nkshot/lib/nkshot/translation-python/bin/python");
        qputenv(keys[1].constData(),"/sources/nkshot/runtime/translation/translate.py");
        qputenv(keys[2].constData(),"/logs/080-translation-staging/runtime-models");
    }
    const QImage image=core->annotationDocument()->baseImage();
    const auto depth=core->annotationDocument()->undoStackDepth();
    const auto video=core->currentVideo();
    const bool videoMode=core->videoMode();
    const QString oldClipboard=application.clipboard()->text();
    const auto restoreClipboard=qScopeGuard([&]{application.clipboard()->setText(oldClipboard);});
    const QString sentinel=QStringLiteral("translation test: explicit copy only");
    application.clipboard()->setText(sentinel);
    QTemporaryDir folder;if(!folder.isValid())return fail(QStringLiteral("Save fixture unavailable"));
    QPointer<OcrTextWindow> window;
    const auto cleanup=qScopeGuard([&]{if(window)delete window;});
    const auto item=[&](const char *name)->QQuickItem*{
        if(!window||!window->rootObject())return nullptr;
        return window->rootObject()->findChild<QQuickItem *>(QString::fromLatin1(name));
    };
    const auto click=[&](const char *name){
        auto control=item(name);
        if(!control||!control->isVisible()||!control->isEnabled())return false;
        const auto position=control->mapToScene(QPointF(control->width()/2,control->height()/2));
        if(position.x()<0||position.x()>window->width()||position.y()<0||position.y()>window->height())return false;
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,position.toPoint());return true;
    };
    const auto create=[&](const QString &text,bool code=false){
        if(window)delete window;
        window=new OcrTextWindow(text,viewer->engine(),code);
        window->resize(qEnvironmentVariableIsSet("NKSHOT_TEST_TRANSLATION_NARROW")?360:840,560);
        window->show();window->requestActivate();
        return QTest::qWaitFor([&]{return window&&window->isVisible()&&window->rootObject()
            &&window->rootObject()->size()==QSizeF(window->size());},1000);
    };
    const auto unchanged=[&]{
        return core->annotationDocument()->baseImage()==image&&core->annotationDocument()->undoStackDepth()==depth
            &&core->currentVideo()==video&&core->videoMode()==videoMode;
    };
    const auto selectDirection=[&](int index){
        if(!click("nkshotTranslationDirection"))return false;
        auto control=item("nkshotTranslationDirection");
        auto popup=qvariant_cast<QObject *>(control->property("popup"));
        if(!QTest::qWaitFor([&]{return popup&&popup->property("visible").toBool();},1000))return false;
        QTest::keyClick(window,Qt::Key_Home);
        for(int i=0;i<index;++i)QTest::keyClick(window,Qt::Key_Down);
        QTest::keyClick(window,Qt::Key_Return);
        return QTest::qWaitFor([&]{return !popup->property("visible").toBool()&&control->property("currentIndex").toInt()==index;},1000);
    };
    const QStringList sources{QStringLiteral("The screenshot has been copied to the clipboard."),
        QString::fromUtf8("请将截图保存到图片文件夹。")};
    for(int direction=0;direction<2;++direction){
        application.clipboard()->setText(sentinel);
        if(!create(sources[direction])||!selectDirection(direction))return fail(QStringLiteral("Actual translation direction control unavailable"));
        auto job=window->translation();
        if(!click("nkshotTranslate"))return fail(QStringLiteral("Translate button unreachable"));
        if(!QTest::qWaitFor([&]{return !job->property("busy").toBool();},65000))
            return fail(QStringLiteral("Real model exceeded UI test deadline"));
        const QString result=job->property("translatedText").toString();
        if(result.isEmpty()||result==sources[direction])
            return fail(QStringLiteral("Actual translation missing: ")+job->property("status").toString());
        if(direction==0&&!result.contains(QRegularExpression(QStringLiteral("[\\x{4E00}-\\x{9FFF}]"))))
            return fail(QStringLiteral("Forward model did not produce Chinese text"));
        if(direction==1&&!result.contains(QRegularExpression(QStringLiteral("[A-Za-z]"))))
            return fail(QStringLiteral("Reverse model did not produce English text"));
        if(application.clipboard()->text()!=sentinel||!unchanged()||window->recognizedText()!=sources[direction])
            return fail(QStringLiteral("Translation modified original text, editor or clipboard"));
        auto originalArea=item("nkshotOcrText");auto translatedArea=item("nkshotTranslatedText");
        if(!originalArea||!translatedArea||!translatedArea->isVisible()
            ||originalArea->property("text").toString()!=sources[direction]||translatedArea->property("text").toString()!=result
            ||originalArea->property("textFormat").toInt()!=0||translatedArea->property("textFormat").toInt()!=0)
            return fail(QStringLiteral("Plain-text source/result panes do not match actual model"));
        QSignalSpy frame(window,&QQuickWindow::frameSwapped);window->update();
        if(!QTest::qWaitFor([&]{return !frame.isEmpty();},1500)
            ||!window->grabWindow().save(screenshot+(direction?QStringLiteral(".zh-en.png"):QStringLiteral(".en-zh.png"))))
            return fail(QStringLiteral("Translation screenshot could not be rendered"));
        if(!click("nkshotCopyTranslation")||application.clipboard()->text()!=result)
            return fail(QStringLiteral("Explicit Copy translation failed"));
        if(!click("nkshotOcrCopy")||application.clipboard()->text()!=sources[direction])
            return fail(QStringLiteral("Explicit Copy original failed"));
        application.clipboard()->setText(sentinel);
        if(!click("nkshotOcrSave"))return fail(QStringLiteral("Original Save action unavailable"));
        QPointer<QFileDialog> dialog;
        if(!QTest::qWaitFor([&]{for(auto widget:QApplication::topLevelWidgets()) {
            if(widget->objectName()==QStringLiteral("nkshotOcrSaveDialog"))dialog=qobject_cast<QFileDialog *>(widget);
            }
            return dialog&&dialog->isVisible();},1000))return fail(QStringLiteral("Original Save dialog not shown"));
        const QString output=folder.filePath(QStringLiteral("original %1.txt").arg(direction));
        auto filename=dialog->findChild<QLineEdit *>(QStringLiteral("fileNameEdit"));
        auto buttons=dialog->findChild<QDialogButtonBox *>();
        auto save=buttons?buttons->button(QDialogButtonBox::Save):nullptr;
        if(!filename||!save)return fail(QStringLiteral("Actual Save dialog controls missing"));
        filename->setFocus();QTest::keyClick(filename,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(filename,output);
        QTest::mouseClick(save,Qt::LeftButton);
        if(!QTest::qWaitFor([&]{return QFile::exists(output);},1000))return fail(QStringLiteral("Original text file not saved"));
        QFile file(output);if(!file.open(QIODevice::ReadOnly)||file.readAll()!=sources[direction].toUtf8())
            return fail(QStringLiteral("Save As exported translation instead of original UTF-8"));
        if(!QTest::qWaitFor([&]{return !dialog||!dialog->isVisible();},1000))return fail(QStringLiteral("Original Save dialog did not close"));
        if(application.clipboard()->text()!=sentinel||!unchanged())return fail(QStringLiteral("Saving changed editor or clipboard"));
        if(!selectDirection(1-direction)||!job->property("translatedText").toString().isEmpty())
            return fail(QStringLiteral("Changing direction retained stale translation"));
        if(!selectDirection(direction)||!click("nkshotTranslate")||!job->property("busy").toBool())
            return fail(QStringLiteral("Cold-start cancellation setup unavailable"));
        if(!click("nkshotTranslate")||job->property("busy").toBool()||!job->property("translatedText").toString().isEmpty())
            return fail(QStringLiteral("Actual Cancel button did not invalidate pending model"));
        if(!click("nkshotTranslate")||!job->property("busy").toBool())return fail(QStringLiteral("Close cancellation setup failed"));
        if(!click("nkshotOcrClose")||!QTest::qWaitFor([&]{return window.isNull();},1500))
            return fail(QStringLiteral("Close during inference did not delete window"));
        if(application.clipboard()->text()!=sentinel||!unchanged())return fail(QStringLiteral("Cancellation changed editor or clipboard"));
    }
    // Missing runtime is a recoverable UI state; inspect the fixed guide URL
    // without clicking it or opening any browser/network connection.
    qputenv("NKSHOT_TRANSLATION_PYTHON", "/missing/nkshot-python");
    qputenv("NKSHOT_TRANSLATION_RUNNER", "/missing/translate.py");
    qputenv("NKSHOT_TRANSLATION_MODEL_DIR", "/missing/models");
    if(!create(sources.first()) || !click("nkshotTranslate"))return fail(QStringLiteral("Missing-runtime setup failed"));
    auto setup=item("nkshotTranslationSetup");
    if(!setup || !setup->isVisible() || setup->property("guideUrl").toUrl()!=QUrl(QStringLiteral("https://github.com/NextKde/NkShot/blob/main/docs/local-translation.md")))
        return fail(QStringLiteral("Missing-runtime installation guide unavailable"));
    QTest::qWait(150);
    if(!window->grabWindow().save(screenshot+QStringLiteral(".missing.png")))return fail(QStringLiteral("Missing-runtime screenshot failed"));
    if(!create(QStringLiteral("<tag> https://example.invalid/"),true))return fail(QStringLiteral("Code-result regression window failed"));
    auto controls=item("nkshotTranslationDirection");
    if(controls&&controls->isVisible())return fail(QStringLiteral("Code-result unexpectedly offers translation"));
    if(!item("nkshotOcrText")||item("nkshotOcrText")->property("text").toString()!=QStringLiteral("<tag> https://example.invalid/"))
        return fail(QStringLiteral("Code-result literal text changed"));
    qInfo()<<"Actual model translation UI passed both directions; discovery"<<(defaultDiscovery?"XDG default":"explicit overrides");
    return true;
}
