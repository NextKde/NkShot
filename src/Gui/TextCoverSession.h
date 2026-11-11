// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Imaging/LocalTextExtraction.h"
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantList>
#include <QImage>
class QQmlEngine;
class TextCoverImageProvider;
class TextCoverSession final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY changed)
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY changed)
    Q_PROPERTY(QVariantList matches READ matches NOTIFY changed)
    Q_PROPERTY(QVariantList candidates READ candidates NOTIFY candidatesChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY changed)
    Q_PROPERTY(int matchCount READ matchCount NOTIFY changed)
public:
    explicit TextCoverSession(QQmlEngine *,QObject *parent=nullptr);
    ~TextCoverSession() override;
    bool begin(const QImage &,const QStringList &languages);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void clearMatches();
    Q_INVOKABLE void setSelected(int index,bool selected);
    Q_INVOKABLE bool isSelected(int index) const;
    bool busy() const { return m_running; }
    bool ready() const { return m_ready; }
    QString error() const { return m_error; }
    QSize sourceSize() const { return m_source.size(); }
    QUrl imageSource() const;
    QVariantList matches() const { return m_matches; }
    QVariantList candidates() const;
    int matchCount() const { return m_matches.size(); }
    int selectedCount() const;
    QImage source() const { return m_source; }
    QList<QRect> selectedRegions() const;
Q_SIGNALS:
    void changed();
    void candidatesChanged();
private:
    void notifyCandidatesChanged();
    QPointer<QQmlEngine> m_engine;
    TextCoverImageProvider *m_provider=nullptr;
    QString m_providerId,m_error;
    QImage m_source;
    QList<NkShot::Imaging::TextWord> m_words;
    QVariantList m_matches;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    quint64 m_epoch=0;
    bool m_running=false,m_ready=false,m_active=false;
};
