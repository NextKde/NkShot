// SPDX-License-Identifier: GPL-3.0-only
#include "TextCoverUiChecks.h"
#include "Gui/ImageCopyEditor.h"
#include "Gui/TextCoverSession.h"
#include "Gui/PinnedImageWindow.h"
#include "Imaging/TextCover.h"
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QPointer>
#include <QQuickItem>
#include <QtTest/QTest>
namespace {
QQuickItem *find(QQuickItem *root, const QString &name)
{
    if (!root) return nullptr;
    if (root->objectName() == name) return root;
    for (auto child : root->childItems())
        if (auto result = find(child, name)) return result;
    return nullptr;
}
}
bool TextCoverUiChecks::run(QApplication &app, ImageCopyEditor *raw, const QString &prefix, QString *failure)
{
    QPointer<ImageCopyEditor> editor = raw;
    auto fail = [&](const QString &message) {
        if (failure) *failure = message;
        if (editor) editor->grabWindow().save(prefix + QStringLiteral(".text-cover-failed.png"));
        return false;
    };
    if (!editor || editor->annotationMode()) return fail(QStringLiteral("Text cover needs an independent image editor"));
    auto document = editor->document();
    const auto before = document->renderToImage().copy(), source = document->baseImage().copy();
    const int history = document->undoStackDepth();
    const bool dirty = editor->hasUnsavedChanges();
    auto unchanged = [&] {
        return editor && document->renderToImage() == before && document->baseImage() == source
            && document->undoStackDepth() == history && editor->hasUnsavedChanges() == dirty;
    };
    auto session = qobject_cast<TextCoverSession *>(editor->textCoverSessionObject());
    if (!session) return fail(QStringLiteral("Text cover session unavailable"));
    auto item = [&](const QString &name) { return editor ? find(editor->contentItem(), name) : nullptr; };
    auto expose = [&](QQuickItem *target) {
        auto scroll = item(QStringLiteral("nkshotTextCoverScroll"));
        auto flick = scroll ? qvariant_cast<QQuickItem *>(scroll->property("contentItem")) : nullptr;
        bool inside = false;
        for (auto ancestor = target; ancestor && flick; ancestor = ancestor->parentItem())
            if (ancestor == flick) { inside = true; break; }
        if (inside) {
            const auto position = target->mapToItem(flick, QPointF());
            if (position.y() < 0 || position.y() + target->height() > flick->height()) {
                const auto y = flick->property("contentY").toDouble();
                const auto maximum = qMax(0.0, flick->property("contentHeight").toDouble() - flick->height());
                flick->setProperty("contentY", qBound(0.0, y + position.y() - flick->height() / 2, maximum));
            }
        }
        QTest::qWait(50);
    };
    auto click = [&](const QString &name) {
        auto target = item(name);
        if (!target || !target->isVisible() || !target->isEnabled()) return false;
        expose(target);
        const auto point = target->mapToScene(QPointF(target->width()/2, target->height()/2)).toPoint();
        if (!QRect(QPoint(), editor->size()).contains(point)) return false;
        QTest::mouseClick(editor, Qt::LeftButton, Qt::NoModifier, point);
        QTest::qWait(100);
        return true;
    };
    auto open = [&] {
        // Cancellation is cooperative. Do not disguise an unfinished OCR worker
        // as a second successful session, or inject synthetic OCR metadata.
        if (!QTest::qWaitFor([&] { return !session->busy(); }, 20000)) return false;
        editor->requestActivate(); QTest::qWait(80);
        if (!click(QStringLiteral("nkshotCopyEditorMore")) || !click(QStringLiteral("nkshotTextCoverAction"))) return false;
        return QTest::qWaitFor([&] {
            auto create = item(QStringLiteral("nkshotTextCoverCreate"));
            return editor->editToolsBlocked() && create && create->isVisible() && session->ready();
        }, 20000);
    };
    auto typeQuery = [&](const char *text) {
        if (!click(QStringLiteral("nkshotTextCoverQuery"))) return false;
        QTest::keyClick(editor, Qt::Key_A, Qt::ControlModifier);
        for (const char *key = text; *key; ++key) QTest::keyClick(editor, *key);
        QTest::qWait(80);
        auto query = item(QStringLiteral("nkshotTextCoverQuery"));
        return query && query->property("text").toString() == QString::fromLatin1(text);
    };
    auto search = [&](const char *text) {
        if (!typeQuery(text) || !click(QStringLiteral("nkshotTextCoverFind"))) return false;
        return QTest::qWaitFor([&] { return session->matchCount() > 0; }, 1500);
    };
    if (!open()) return fail(QStringLiteral("Actual text-cover menu/OCR failed: ") + session->error());
    if (session->matchCount() || session->selectedCount() || !unchanged())
        return fail(QStringLiteral("Opening text cover changed source or retained an old query"));
    if (!search("SECRET") || session->matchCount() < 2 || session->selectedCount() != session->matchCount())
        return fail(QStringLiteral("Real OCR did not find both SECRET fixture occurrences: ") + session->error());
    const auto count = session->selectedCount();
    auto firstMatch = item(QStringLiteral("nkshotTextCoverMatch_0"));
    if (!firstMatch || !firstMatch->isVisible() || !firstMatch->isEnabled())
        return fail(QStringLiteral("Keyboard review checkbox is unavailable"));
    expose(firstMatch);
    firstMatch->forceActiveFocus();
    if (!firstMatch->hasActiveFocus())
        return fail(QStringLiteral("Keyboard review checkbox did not receive focus"));
    QTest::keyClick(editor, Qt::Key_Space);
    if (!QTest::qWaitFor([&] {
        auto current = item(QStringLiteral("nkshotTextCoverMatch_0"));
        return session->selectedCount() == count - 1 && current && current->hasActiveFocus();
    }, 1000)) return fail(QStringLiteral("First keyboard toggle lost the current match focus or selection"));
    // Deliberately do not force focus again. Model updates must not interrupt a
    // user who reviews the same match with consecutive keyboard activations.
    QTest::keyClick(editor, Qt::Key_Space);
    if (!QTest::qWaitFor([&] {
        auto current = item(QStringLiteral("nkshotTextCoverMatch_0"));
        return session->selectedCount() == count && current && current->hasActiveFocus();
    }, 1000) || !unchanged())
        return fail(QStringLiteral("Second keyboard toggle did not restore selection with continuous focus"));
    if(qEnvironmentVariableIsSet("NKSHOT_TEST_TEXT_COVER_MANY")) {
        if(count<12)return fail(QStringLiteral("Many-match OCR fixture did not produce enough review rows"));
        auto fullyVisible=[&](QQuickItem *target) {
            for(const auto &name:{QStringLiteral("nkshotTextCoverMatchesScroll"),QStringLiteral("nkshotTextCoverScroll")}) {
                auto area=item(name);auto flick=area?qvariant_cast<QQuickItem *>(area->property("contentItem")):nullptr;
                if(!flick)return false;
                const auto rect=QRectF(target->mapToItem(flick,QPointF()),target->size());
                if(!QRectF(0,0,flick->width(),flick->height()).adjusted(-1,-1,1,1).contains(rect))return false;
            }
            return true;
        };
        for(int direction:{1,-1}) {
            const int start=direction==1?1:count-2,end=direction==1?count:-1;
            for(int index=start;index!=end;index+=direction) {
                QTest::keyClick(editor,Qt::Key_Tab,direction==1?Qt::NoModifier:Qt::ShiftModifier);
                if(!QTest::qWaitFor([&]{auto target=item(QStringLiteral("nkshotTextCoverMatch_%1").arg(index));
                    return target&&target->hasActiveFocus()&&fullyVisible(target);},1000))
                    return fail(QStringLiteral("Keyboard review row was lost or clipped: ")+QString::number(index));
            }
        }
        if(session->selectedCount()!=count||!unchanged())return fail(QStringLiteral("Keyboard navigation changed matches or source"));
    }
    if (!click(QStringLiteral("nkshotTextCoverMatch_0")) || session->selectedCount() != count - 1)
        return fail(QStringLiteral("Actual match checkbox did not deselect one occurrence"));
    auto create = item(QStringLiteral("nkshotTextCoverCreate"));
    if (!create) return fail(QStringLiteral("Create control missing"));
    create->forceActiveFocus();
    app.clipboard()->setText(QStringLiteral("cover-shortcut-sentinel"));
    QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
    QTest::keyClick(editor, Qt::Key_S, Qt::ControlModifier);
    QTest::qWait(100);
    if (app.clipboard()->text() != QStringLiteral("cover-shortcut-sentinel") || !unchanged())
        return fail(QStringLiteral("Popup Copy/Save escaped to the source editor"));
    for (auto widget : app.topLevelWidgets())
        if (qobject_cast<QFileDialog *>(widget) && widget->isVisible())
            return fail(QStringLiteral("Popup Save opened a source file dialog"));
    if (!typeQuery("ABSENTTOKEN") || !click(QStringLiteral("nkshotTextCoverFind"))
        || session->matchCount() || session->selectedCount() || create->isEnabled() || !unchanged())
        return fail(QStringLiteral("No-match search offered a misleading covered output"));
    if (!typeQuery("PUBLIC") || session->selectedCount() || session->matchCount() || create->isEnabled())
        return fail(QStringLiteral("Edited query retained stale cover selections"));
    if (!click(QStringLiteral("nkshotTextCoverFind")) || session->matchCount() < 1)
        return fail(QStringLiteral("Changed query did not search actual OCR words"));
    if (!click(QStringLiteral("nkshotTextCoverCancel"))
        || !QTest::qWaitFor([&] { return !editor->editToolsBlocked(); }, 1000) || !unchanged())
        return fail(QStringLiteral("Cancel did not preserve the edit session"));
    if (!open() || session->matchCount() || session->selectedCount())
        return fail(QStringLiteral("Reopen retained previous search results"));
    QTest::keyClick(editor, Qt::Key_Escape); QTest::qWait(150);
    if (editor->editToolsBlocked() || !unchanged() || !open())
        return fail(QStringLiteral("Escape/reopen did not preserve the edit session"));
    if (!search("SECRET") || session->matchCount() < 2)
        return fail(QStringLiteral("Final real OCR search failed"));
    const auto regions = session->selectedRegions();
    const auto expected = NkShot::Imaging::coveredTextCopy(before, regions);
    if (regions.isEmpty() || expected.isNull() || expected == before)
        return fail(QStringLiteral("Covered reference pixels are empty or unchanged"));
    for (const auto &rect : regions)
        for (int y = rect.top(); y <= rect.bottom(); ++y)
            for (int x = rect.left(); x <= rect.right(); ++x)
                if (expected.pixelColor(x,y) != QColor(Qt::black)) return fail(QStringLiteral("Cover reference is not opaque black"));
    editor->grabWindow().save(prefix + QStringLiteral(".text-cover.png"));
    if (auto scroll = item(QStringLiteral("nkshotTextCoverScroll")))
        if (auto flick = qvariant_cast<QQuickItem *>(scroll->property("contentItem"))) {
            flick->setProperty("contentY", 0); QTest::qWait(80);
            editor->grabWindow().save(prefix + QStringLiteral(".text-cover-preview.png"));
        }
    bool busyObserved = false, duplicateRejected = false;
    const auto connection = QObject::connect(editor, &ImageCopyEditor::adjustmentStateChanged, editor, [&] {
        if (!busyObserved && editor->coveringCopy()) {
            busyObserved = true;
            duplicateRejected = !editor->createCoveredCopy();
        }
    });
    const bool clicked = click(QStringLiteral("nkshotTextCoverCreate"));
    QObject::disconnect(connection);
    if (!clicked || !busyObserved || !duplicateRejected)
        return fail(QStringLiteral("Cover output did not reject duplicate busy submission"));
    QPointer<PinnedImageWindow> pin;
    if (!QTest::qWaitFor([&] {
        for (auto window : app.topLevelWindows())
            if (window->objectName() == QStringLiteral("nkshotCoveredTextPin")) {
                pin = qobject_cast<PinnedImageWindow *>(window);
                if (pin && pin->isVisible()) return true;
            }
        return false;
    }, 15000)) return fail(QStringLiteral("Covered image pin did not appear"));
    if (pin->snapshot() != expected || pin->snapshot().devicePixelRatio() != expected.devicePixelRatio() || !unchanged())
        return fail(QStringLiteral("Covered output differs from full-resolution reference or changed original edits"));
    const auto actual = pin->snapshot();
    for (int y = 0; y < expected.height(); ++y)
        for (int x = 0; x < expected.width(); ++x)
            if (actual.pixelColor(x,y).alpha() != 255)
                return fail(QStringLiteral("Covered output retained transparent pixels"));
    if (!editor->coverReviewPending() || !editor->editToolsBlocked() || !editor->hasCoveredCopy())
        return fail(QStringLiteral("Successful cover did not pause the original editor for review"));
    // Exercise the dangerous focus case: the old, unredacted editor becomes
    // active again. Neither its shortcuts nor its C++ Copy route may leak it.
    editor->requestActivate();
    if (!QTest::qWaitFor([&] { return editor->isActive(); }, 1000))
        return fail(QStringLiteral("Could not activate original editor to verify the review gate"));
    app.clipboard()->setText(QStringLiteral("cover-review-gate-sentinel"));
    QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
    QTest::keyClick(editor, Qt::Key_S, Qt::ControlModifier);
    editor->copyImage();
    QTest::qWait(100);
    if (app.clipboard()->text() != QStringLiteral("cover-review-gate-sentinel") || !unchanged())
        return fail(QStringLiteral("Paused original editor copied unredacted pixels or changed source"));
    for (auto widget : app.topLevelWidgets())
        if (qobject_cast<QFileDialog *>(widget) && widget->isVisible())
            return fail(QStringLiteral("Paused original editor opened a Save dialog"));
    // A hidden result must be recoverable through the actual paused-editor
    // control, without resuming the original or creating a second output.
    pin->hide(); QTest::qWait(80);
    editor->requestActivate(); QTest::qWait(80);
    editor->grabWindow().save(prefix + QStringLiteral(".original-paused.png"));
    if (!editor->coverReviewPending() || !editor->hasCoveredCopy()
        || !click(QStringLiteral("nkshotShowCoveredCopy"))
        || !QTest::qWaitFor([&] { return pin && pin->isVisible() && pin->isActive(); }, 1000)
        || !editor->coverReviewPending() || !editor->editToolsBlocked() || !unchanged())
        return fail(QStringLiteral("Show covered copy did not restore the result while retaining the review gate"));
    pin->grabWindow().save(prefix + QStringLiteral(".covered-copy.png"));
    auto copy = find(pin->contentItem(), QStringLiteral("nkshotPinCopy"));
    if (!copy || !copy->isVisible() || !copy->isEnabled()) return fail(QStringLiteral("Covered pin Copy unavailable"));
    pin->requestActivate(); QTest::qWait(80);
    QTest::mouseClick(pin, Qt::LeftButton, Qt::NoModifier,
        copy->mapToScene(QPointF(copy->width()/2, copy->height()/2)).toPoint());
    if (!QTest::qWaitFor([&] { return app.clipboard()->image() == expected; }, 1000) || !unchanged())
        return fail(QStringLiteral("Actual pin Copy did not contain only the covered output"));
    pin->close();
    if (!QTest::qWaitFor([&] { return pin.isNull(); }, 1500)
        || !editor->coverReviewPending() || !editor->editToolsBlocked() || editor->hasCoveredCopy() || !unchanged())
        return fail(QStringLiteral("Closing the covered pin unexpectedly resumed the unredacted original"));
    editor->requestActivate();
    if (!QTest::qWaitFor([&] { return editor->isActive(); }, 1000))
        return fail(QStringLiteral("Original editor could not regain focus after covered pin close"));
    app.clipboard()->setText(QStringLiteral("closed-cover-gate-sentinel"));
    QTest::keyClick(editor, Qt::Key_C, Qt::ControlModifier);
    editor->copyImage(); QTest::qWait(80);
    if (app.clipboard()->text() != QStringLiteral("closed-cover-gate-sentinel"))
        return fail(QStringLiteral("Closed-result review gate allowed source Copy"));
    if (!click(QStringLiteral("nkshotResumeOriginal"))
        || !QTest::qWaitFor([&] { return !editor->coverReviewPending() && !editor->editToolsBlocked(); }, 1000)
        || !unchanged())
        return fail(QStringLiteral("Explicit Resume original did not restore the unchanged edit session"));
    qInfo() << "111 actual OCR search, individual review, query invalidation, cancel/Escape, shortcut guards, exact covered copy and explicit-original-resume gate passed";
    return true;
}
