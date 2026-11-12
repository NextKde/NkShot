// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QList>
#include <QUrl>
#include <QStringList>
#include <QPointer>
class ImageCombineSession;
class QCloseEvent;
class QFileDialog;
class ImageCombineWindow final : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(QObject *session READ sessionObject CONSTANT)
    Q_PROPERTY(bool choosingImages READ choosingImages NOTIFY busyChanged)
    Q_PROPERTY(QString selectionError READ selectionError NOTIFY busyChanged)
public:
    ImageCombineWindow(const QStringList &paths, bool keepAll, bool horizontal, QQmlEngine *engine);
    ~ImageCombineWindow() override;
    QString selectionError() const { return m_selectionError; }
    bool busy() const;
    bool choosingImages() const { return !m_chooseDialog.isNull(); }
    Q_INVOKABLE void chooseImages();
    QObject *sessionObject() const;
    Q_INVOKABLE QQuickWindow *createCopy();
Q_SIGNALS:
    void busyChanged();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    ImageCombineSession *m_session = nullptr;
    bool m_submitted = false;
    QPointer<QFileDialog> m_chooseDialog;
    QString m_selectionError;
};
