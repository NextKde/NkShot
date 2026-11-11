// SPDX-License-Identifier: GPL-3.0-only
#include "FreeCropUiChecks.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QLineF>
#include <QQuickItem>
#include <QQuickWindow>
#include <QUrl>
#include <QWheelEvent>
#include <QtTest/QTest>

namespace {
QQuickItem *find(QQuickItem *root, const QString &name) {
    if (!root) return nullptr;
    if (root->objectName() == name) return root;
    for (auto child : root->childItems()) if (auto result = find(child, name)) return result;
    return nullptr;
}
QByteArray sourceHash(QObject *job) {
    QFile file(job->property("sourceUrl").toUrl().toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    return hash.addData(&file) ? hash.result() : QByteArray();
}
}

bool FreeCropUiChecks::checkLocked(QQuickWindow *window, QObject *job, QString *failure) {
    const auto fail = [&](const QString &text) { if (failure) *failure = text; return false; };
    auto overlay = find(window->contentItem(), QStringLiteral("nkshotVideoCropOverlay"));
    if (!overlay || overlay->property("interactive").toBool()) return fail(QStringLiteral("Free crop remains interactive while export is locked"));
    const auto before = job->property("cropRect").toRect();
    overlay->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Right);
    QTest::keyClick(window, Qt::Key_Down, Qt::ControlModifier|Qt::ShiftModifier);
    QTest::keyClick(window, Qt::Key_Home);
    for (int i = 0; i < 4; ++i) {
        auto handle = find(window->contentItem(), QStringLiteral("nkshotVideoCropHandle_%1").arg(i));
        if (!handle) return fail(QStringLiteral("Locked free crop corner missing"));
        const auto point = handle->mapToScene(QPointF(handle->width()/2, handle->height()/2)).toPoint();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, point);
        QTest::mouseMove(window, point + QPoint(6,6), 10);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, point + QPoint(6,6));
    }
    if (job->property("cropRect").toRect() != before) return fail(QStringLiteral("Locked pointer or keyboard changed free crop"));
    return true;
}

bool FreeCropUiChecks::run(QApplication &application, QQuickWindow *window, QObject *job, const QString &evidence, QString *failure) {
    Q_UNUSED(application);
    const auto fail = [&](const QString &text) { if (failure) *failure = text; window->grabWindow().save(evidence + QStringLiteral(".free-crop-failed.png")); return false; };
    const auto item = [&](const char *name) { return find(window->contentItem(), QString::fromLatin1(name)); };
    const auto hash = sourceHash(job);
    if (hash.isEmpty()) return fail(QStringLiteral("Free crop source hash unavailable"));
    const auto click = [&](const char *name) {
        auto target = item(name);
        if (!target || !target->isVisible() || !target->isEnabled()) return false;
        auto scroll = item("nkshotClipDetailScroll");
        bool inDetails = false;
        for (auto p = target->parentItem(); p; p = p->parentItem()) if (p == scroll) inDetails = true;
        if (inDetails) {
            const QRectF bounds(scroll->mapToScene(QPointF()), scroll->size());
            for (int attempt = 0; attempt < 14; ++attempt) {
                const QRectF rect(target->mapToScene(QPointF()), target->size());
                if (bounds.contains(rect)) break;
                const auto at = scroll->mapToScene(QPointF(scroll->width()/2, scroll->height()/2));
                QWheelEvent wheel(at, window->mapToGlobal(at.toPoint()), QPoint(), QPoint(0, rect.bottom() > bounds.bottom() ? -120 : 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                QCoreApplication::sendEvent(window, &wheel); QTest::qWait(100);
            }
            if (!bounds.contains(QRectF(target->mapToScene(QPointF()), target->size()))) {
                auto bar=item("nkshotClipDetailScrollbar");
                if(!bar)return false;
                const qreal position=bar->property("position").toReal(), extent=bar->property("size").toReal();
                const auto from=bar->mapToScene(QPointF(bar->width()/2,(position+extent/2)*bar->height())).toPoint();
                const bool below=target->mapToScene(QPointF()).y()>bounds.bottom();
                const auto to=bar->mapToScene(QPointF(bar->width()/2,below?bar->height()-2:2)).toPoint();
                QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,from);
                QTest::mouseMove(window,to,100);
                QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,to);QTest::qWait(150);
            }
            if (!bounds.contains(QRectF(target->mapToScene(QPointF()), target->size()))) return false;
        }
        for (auto p = target->parentItem(); p; p = p->parentItem())
            if (p->property("moving").isValid() && !QTest::qWaitFor([&] {return !p->property("moving").toBool();}, 3000)) return false;
        const auto point = target->mapToScene(QPointF(target->width()/2,target->height()/2)).toPoint();
        if (!QRect(QPoint(),window->size()).contains(point)) return false;
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,point); QTest::qWait(100);
        return true;
    };
    auto choice = item("nkshotClipCropAspect");
    if (!choice || !click("nkshotClipCropAspect")) return fail(QStringLiteral("Free crop choice not reachable"));
    // Free is appended to the existing preset menu, preserving preset indices.
    QTest::keyClick(window, Qt::Key_End);
    QTest::keyClick(window,Qt::Key_Return);
    if (!QTest::qWaitFor([&] {return job->property("cropAspect").toString()==QStringLiteral("free");},1000)) return fail(QStringLiteral("Real Free selection did not reach job"));
    if (!click("nkshotClipDetails")) return fail(QStringLiteral("Free crop details collapse failed"));
    auto overlay = item("nkshotVideoCropOverlay");
    auto frame = item("nkshotVideoCropFrame");
    const auto size = job->property("sourceSize").toSize();
    const auto crop = [&] { return job->property("cropRect").toRect(); };
    const auto geometry = [&] {
        const auto r=crop(); const auto display=overlay->property("displayRect").toRectF();
        const auto top=overlay->mapToScene(display.topLeft()+QPointF(qreal(r.x())/size.width()*display.width(),qreal(r.y())/size.height()*display.height()));
        return r.width()>=2&&r.height()>=2&&QRect(QPoint(),size).contains(r)
            &&!(r.x()%2||r.y()%2||r.width()%2||r.height()%2)
            &&QLineF(frame->mapToScene(QPointF()),top).length()<1.5
            &&qAbs(frame->width()-qreal(r.width())/size.width()*display.width())<1.5
            &&qAbs(frame->height()-qreal(r.height())/size.height()*display.height())<1.5;
    };
    if (!overlay || !frame || !overlay->property("interactive").toBool() || !geometry()) return fail(QStringLiteral("Free crop does not use full source coordinates"));
    const auto drag = [&](QQuickItem *target,QPoint delta) {
        const auto p=target->mapToScene(QPointF(target->width()/2,target->height()/2)).toPoint();
        QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,p);
        for(int step=1;step<=6;++step) QTest::mouseMove(window,p+delta*step/6,20);
        QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,p+delta); QTest::qWait(80);
    };
    // TL, TR, BL, BR. Each independent inward gesture keeps its opposite edges fixed.
    const QPoint inward[]={{8,8},{-8,8},{8,-8},{-8,-8}};
    for(int i=0;i<4;++i) {
        auto handle=find(window->contentItem(),QStringLiteral("nkshotVideoCropHandle_%1").arg(i));
        if(!handle||!handle->isVisible()) return fail(QStringLiteral("Free crop corner %1 unavailable").arg(i));
        const auto before=crop(); drag(handle,inward[i]); const auto after=crop();
        const int fixedX=(i%2)?before.x():before.x()+before.width();
        const int fixedY=(i>=2)?before.y():before.y()+before.height();
        if(after.width()>=before.width()||after.height()>=before.height()
            ||((i%2)?after.x():after.x()+after.width())!=fixedX
            ||((i>=2)?after.y():after.y()+after.height())!=fixedY||!geometry())
            return fail(QStringLiteral("Free corner %1 did not resize around fixed opposite corner").arg(i));
    }
    auto before=crop();drag(frame,QPoint(8,8));
    if(crop().size()!=before.size()||crop().x()<=before.x()||crop().y()<=before.y()||!geometry()) return fail(QStringLiteral("Free interior drag resized or failed to move frame"));
    before=crop();QTest::keyClick(window,Qt::Key_Left);
    if(crop()!=before.translated(-2,0)) return fail(QStringLiteral("Free Arrow movement failed"));
    before=crop();QTest::keyClick(window,Qt::Key_Right,Qt::ControlModifier);
    if(crop().topLeft()!=before.topLeft()||crop().height()!=before.height()||crop().width()!=before.width()+2||!geometry()) return fail(QStringLiteral("Free Ctrl Arrow did not resize bottom-right"));
    before=crop();QTest::keyClick(window,Qt::Key_Left,Qt::ShiftModifier);
    if(crop()!=before.translated(-10,0)) return fail(QStringLiteral("Free Shift Arrow fast movement failed"));
    before=crop();QTest::keyClick(window,Qt::Key_Down,Qt::ControlModifier|Qt::ShiftModifier);
    if(crop().topLeft()!=before.topLeft()||crop().width()!=before.width()||crop().height()!=before.height()+10) return fail(QStringLiteral("Free Ctrl Shift Arrow resize failed"));
    before=crop();QTest::keyClick(window,Qt::Key_Home);
    if(crop().size()!=before.size()||crop().x()!=((size.width()-before.width())/2&~1)||crop().y()!=((size.height()-before.height())/2&~1)) return fail(QStringLiteral("Free Home resized instead of centering"));
    if(!window->grabWindow().save(evidence+QStringLiteral(".free-crop.png"))) return fail(QStringLiteral("Free crop evidence failed"));
    // Privacy adjustment takes exclusive pointer and keyboard ownership, then returns it.
    const auto finalCrop=crop();
    if(!click("nkshotClipDetails")||!click("nkshotClipPrivacyEnabled")) return fail(QStringLiteral("Privacy mode unavailable for crop exclusion check"));
    auto privacy=item("nkshotVideoPrivacyOverlay");
    if(!privacy||!privacy->property("interactive").toBool()||overlay->property("interactive").toBool()) return fail(QStringLiteral("Privacy and crop are interactive simultaneously"));
    overlay->forceActiveFocus();QTest::keyClick(window,Qt::Key_Right);QTest::keyClick(window,Qt::Key_Right,Qt::ShiftModifier);
    if(crop()!=finalCrop) return fail(QStringLiteral("Privacy mode keyboard changed crop"));
    if(!click("nkshotClipDetails")||!click("nkshotClipPrivacyEnabled")||job->property("privacyEnabled").toBool()||!overlay->property("interactive").toBool()) return fail(QStringLiteral("Ending privacy did not restore crop ownership"));
    // Leave Details open as expected by caller, and leave actual Free rectangle for export.
    if(sourceHash(job)!=hash||crop()!=finalCrop||!geometry()) return fail(QStringLiteral("Free crop changed source or lost reviewed geometry"));
    qInfo()<<"112 actual Free selection, four corners, move/keyboard/center, full-source geometry and privacy exclusion passed";
    return true;
}
