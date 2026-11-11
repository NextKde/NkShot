// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QPointer>
#include <QImage>
class NoteImageProvider;
class QuickNoteWindow : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY previewChanged)
    Q_PROPERTY(bool canExport READ canExport NOTIFY previewChanged)
    Q_PROPERTY(QString error READ error NOTIFY previewChanged)
public:
    QuickNoteWindow(QQmlEngine *engine);
    ~QuickNoteWindow() override;
    static void hideDraftsForCapture();
    QUrl imageSource() const;
    bool canExport() const {return !m_image.isNull();}
    QString error() const {return m_error;}
    const QImage &snapshot() const {return m_image;}
    Q_INVOKABLE void updatePreview(const QString &text,int style);
    Q_INVOKABLE void copyImage();
    Q_INVOKABLE QQuickWindow *pinImage();
Q_SIGNALS:
    void previewChanged();
    void statusMessage(const QString &text);
private:
    QPointer<QQmlEngine> m_engine;
    NoteImageProvider *m_provider;
    const QString m_providerId;
    QImage m_image;
    QString m_error;
    quint64 m_revision=0;
};
