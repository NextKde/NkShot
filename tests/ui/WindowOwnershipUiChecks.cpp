// SPDX-License-Identifier: GPL-3.0-only
#include "WindowOwnershipUiChecks.h"
#include "Gui/QuickNoteWindow.h"
#include <QQmlEngine>
#include <QJSValue>
#include <QQuickView>
#include <QQuickItem>
#include <QPointer>
#include <QCoreApplication>
#include <QEvent>
#include <QtTest>

bool WindowOwnershipUiChecks::run(QQuickView *owner, QString *failure)
{
    const auto fail = [&](const QString &message) {
        if (failure) *failure = message;
        return false;
    };
    if (!owner || !owner->engine() || !owner->isVisible())
        return fail(QStringLiteral("Window ownership check needs a visible owner and engine"));
    QPointer<QQmlEngine> engine = owner->engine();
    QPointer<QQuickView> ownerGuard = owner;
    auto ownerValue = engine->newQObject(owner);
    QString invocationError;
    const auto invoke = [&](const QString &method) -> QPointer<QQuickWindow> {
        auto callable = ownerValue.property(method);
        if (!callable.isCallable()) {
            invocationError = QStringLiteral("Missing invokable: ") + method;
            return {};
        }
        auto returned = callable.callWithInstance(ownerValue);
        if (returned.isError()) {
            invocationError = method + QStringLiteral(": ") + returned.toString();
            return {};
        }
        QPointer<QQuickWindow> window = qobject_cast<QQuickWindow *>(returned.toQObject());
        // Deliberately release the JS return wrapper before collecting. Keeping
        // only QPointer does not make the returned window a GC root.
        returned = QJSValue(QJSValue::UndefinedValue);
        return window;
    };
    const auto collect = [&] {
        for (int round = 0; round < 2; ++round) {
            if (!engine) return;
            engine->collectGarbage();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        }
    };
    const auto cppOwned = [](const QPointer<QQuickWindow> &window) {
        return window && QQmlEngine::objectOwnership(window.data()) == QQmlEngine::CppOwnership;
    };
    auto note = invoke(QStringLiteral("openQuickNote"));
    if (!cppOwned(note) || !note->isVisible())
        return fail(QStringLiteral("Quick Note return is not visible/CppOwned: ") + invocationError);
    auto noteView = qobject_cast<QuickNoteWindow *>(note.data());
    if (!noteView || !noteView->rootObject()) return fail(QStringLiteral("Quick Note returned an unexpected or incomplete window"));
    // Use the real editable draft, not only the generated image cache.
    QPointer<QObject> draft;
    for (auto object : noteView->rootObject()->findChildren<QObject *>()) {
        if (object->metaObject()->indexOfProperty("textDocument") >= 0
            && object->metaObject()->indexOfProperty("cursorPosition") >= 0) {
            draft = object;
            break;
        }
    }
    if (!draft) return fail(QStringLiteral("Quick Note draft field missing"));
    const auto originalText = draft->property("text").toString();
    const QString fixture = QStringLiteral("Ownership regression draft\nKeep this local note through GC.");
    draft->setProperty("text", fixture);
    if (!QTest::qWaitFor([&] { return note && noteView->canExport(); }, 1500))
        return fail(QStringLiteral("Quick Note draft did not render"));
    // Let the production debounce settle before taking the preservation snapshot.
    QTest::qWait(180);
    if (!note || !draft) return fail(QStringLiteral("Quick Note disappeared before GC"));
    const auto snapshot = noteView->snapshot().copy();
    collect();
    if (!cppOwned(note) || !note->isVisible() || !draft || draft->property("text").toString() != fixture
        || noteView->snapshot() != snapshot)
        return fail(QStringLiteral("GC destroyed or changed the visible Quick Note draft"));
    note->close();
    collect();
    if (!cppOwned(note) || note->isVisible() || !draft || draft->property("text").toString() != fixture)
        return fail(QStringLiteral("Closing Quick Note must hide and retain its draft across GC"));
    auto reopened = invoke(QStringLiteral("openQuickNote"));
    collect();
    if (!note || reopened != note || !note->isVisible() || !draft
        || draft->property("text").toString() != fixture || noteView->snapshot() != snapshot)
        return fail(QStringLiteral("Quick Note reopen did not retain the same session and draft"));
    draft->setProperty("text", originalText);
    note->close();
    for (const QString method : {QStringLiteral("openColorPicker"), QStringLiteral("openScrollPreview")}) {
        if (!ownerGuard) return fail(QStringLiteral("Owner disappeared during ownership checks"));
        ownerGuard->show();
        auto window = invoke(method);
        if (!cppOwned(window) || !window->isVisible())
            return fail(method + QStringLiteral(" return is not visible/CppOwned: ") + invocationError);
        collect();
        if (!cppOwned(window) || !window->isVisible())
            return fail(method + QStringLiteral(" window was destroyed/hidden by GC"));
        window->close();
        if (!QTest::qWaitFor([&] { return window.isNull(); }, 1500))
            return fail(method + QStringLiteral(" did not preserve explicit-close destruction"));
        collect();
    }
    if (!ownerGuard || !engine) return fail(QStringLiteral("Owner/engine lost during child-window cleanup"));
    ownerGuard->requestActivate();
    qInfo() << "105 real JS invokable ownership: Quick Note GC/draft/reopen, Color Picker and Scroll Preview GC/explicit-close passed";
    return true;
}
