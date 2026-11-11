// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QPointer>
#include <QVariantList>
#include "Imaging/ScrollCaptureController.h"
class ScrollFrameKWin;
class QScreen;
class ScrollPreviewProvider;
class ScrollCaptureWindow : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(QUrl previewSource READ previewSource NOTIFY changed)
    Q_PROPERTY(QSize previewSize READ previewSize NOTIFY changed)
    Q_PROPERTY(QVariantList seamRows READ seamRows NOTIFY changed)
    Q_PROPERTY(bool retryFrame READ retryFrame NOTIFY changed)
    Q_PROPERTY(QStringList outputs READ outputs CONSTANT)
    Q_PROPERTY(int outputIndex READ outputIndex WRITE setOutputIndex NOTIFY changed)
    Q_PROPERTY(qreal selectionScale READ selectionScale NOTIFY changed)
    Q_PROPERTY(int phase READ phase NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canCapture READ canCapture NOTIFY changed)
    Q_PROPERTY(bool canFinish READ canFinish NOTIFY changed)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
public:
    explicit ScrollCaptureWindow(QQmlEngine *engine);
    ~ScrollCaptureWindow() override;
    QUrl previewSource() const;
    QSize previewSize() const { return m_preview.size(); }
    QVariantList seamRows() const;
    bool retryFrame() const { return m_retryFrame; }
    QStringList outputs() const { return m_outputs; }
    int outputIndex() const { return m_outputIndex; }
    void setOutputIndex(int index);
    qreal selectionScale() const;
    int phase() const { return m_phase; }
    bool available() const { return m_available; }
    bool busy() const;
    int frameCount() const;
    bool canCapture() const {return m_controller->state()==NkShot::Imaging::ScrollCaptureController::State::Ready && frameCount()<16;}
    bool canFinish() const {return !m_result.isNull() || frameCount()>=2;}
    QString statusText() const { return m_status; }
    Q_INVOKABLE void selectRegion();
    Q_INVOKABLE void acceptRegion(const QRectF &localLogical);
    Q_INVOKABLE void captureNext();
    Q_INVOKABLE void finishCapture();
    Q_INVOKABLE void cancelPreview();
Q_SIGNALS:
    void changed();
protected:
    bool event(QEvent *) override;
    bool eventFilter(QObject *, QEvent *) override;
private:
#ifdef NKSHOT_ENABLE_UI_CAPTURE
    friend class ScrollSessionUiChecks;
#endif
    void bindController();
    void updatePreview();
    QString m_providerId;
    ScrollPreviewProvider *m_previewProvider = nullptr;
    QImage m_preview;
    QVector<int> m_previewSeams;
    quint64 m_previewRevision = 0;
    bool m_retryFrame = false;
    NkShot::Imaging::ScrollOutputLayout currentLayout() const;
    void restoreWindows();
    void loadView(QQuickView *view);
    void setStatus(const QString &text);
    QStringList m_outputs;
    QList<QPointer<QScreen>> m_screens;
    QList<QPair<QPointer<QWindow>,QWindow::Visibility>> m_hidden;
    QPointer<QScreen> m_screen;
    QPointer<QQuickView> m_overlay;
    QPointer<QQmlEngine> m_engine;
    ScrollFrameKWin *m_backend;
    NkShot::Imaging::ScrollCaptureController *m_controller;
    int m_outputIndex=0;
    int m_phase=0;
    bool m_available=false;
    QString m_status;
    QImage m_result;
    void showResult();
};
