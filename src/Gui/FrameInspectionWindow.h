// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QUrl>
class FrameInspectionSession;
class QCloseEvent;
class FrameInspectionWindow final : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(QObject *session READ sessionObject CONSTANT)
public:
    FrameInspectionWindow(const QUrl &source, qint64 positionMs, QQmlEngine *engine);
    QObject *sessionObject() const;
    Q_INVOKABLE QQuickWindow *extractCopy();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    FrameInspectionSession *m_session = nullptr;
};
