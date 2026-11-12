// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QObject>
#include <QImage>
#include <QPointer>
#include <QVariantList>
#include <QUrl>
#include <atomic>
#include <memory>
class QQmlEngine;
class CombineImageProvider;
class ImageCombineSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QString direction READ direction WRITE setDirection NOTIFY changed)
    Q_PROPERTY(bool keepAll READ keepAll WRITE setKeepAll NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QUrl previewUrl READ previewUrl NOTIFY changed)
    Q_PROPERTY(QSize outputSize READ outputSize NOTIFY changed)
public:
    explicit ImageCombineSession(QQmlEngine *,QObject *parent=nullptr);
    ~ImageCombineSession() override;
    bool begin(const QList<QUrl> &);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void move(int from,int to);
    Q_INVOKABLE void remove(int index);
    void setDirection(const QString &);
    void setKeepAll(bool);
    QVariantList items() const;
    QString direction() const {return m_direction;}
    bool keepAll() const {return m_keepAll;}
    bool ready() const {return !m_result.isNull()&&!m_running&&!m_pending;}
    bool busy() const {return m_running||m_pending;}
    QString error() const {return m_error;}
    QUrl previewUrl() const;
    QSize outputSize() const {return m_result.size();}
    QImage snapshot() const {return ready()?m_result:QImage();}
Q_SIGNALS:
    void changed();
private:
    struct Entry {QString id,name;QImage image,thumbnail;};
    struct Work {QList<Entry> entries;QImage output,preview;QString error;bool loaded=false;};
    void schedule();
    void launch();
    void publish();
    QPointer<QQmlEngine> m_engine;
    CombineImageProvider *m_provider=nullptr;
    QString m_providerId,m_direction=QStringLiteral("vertical"),m_error;
    bool m_keepAll=true,m_running=false,m_pending=false,m_active=false;
    quint64 m_epoch=0;
    QList<QUrl> m_paths;
    QList<Entry> m_entries;
    QImage m_result,m_preview;
    std::shared_ptr<std::atomic_bool> m_cancel;
};
