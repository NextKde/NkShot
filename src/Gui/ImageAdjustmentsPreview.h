// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "Imaging/ImageAdjustments.h"
#include <QObject>
#include <QPointer>
#include <QUrl>
class QQmlEngine;
class AdjustmentsImageProvider;
class ImageAdjustmentsPreview final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QSize sourceSize READ sourceSize NOTIFY changed)
    Q_PROPERTY(QUrl imageSource READ imageSource NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit ImageAdjustmentsPreview(QQmlEngine *engine, QObject *parent = nullptr);
    ~ImageAdjustmentsPreview() override;
    bool begin(const QImage &source);
    void clear();
    Q_INVOKABLE void update(int brightness, int contrast, int saturation);
    QSize sourceSize() const { return m_source.size(); }
    QUrl imageSource() const;
    bool ready() const { return m_ready; }
    QString error() const { return m_error; }
    QImage source() const { return m_source; }
    NkShot::Imaging::AdjustmentOptions options() const { return m_options; }
Q_SIGNALS:
    void changed();
private:
    QPointer<QQmlEngine> m_engine;
    AdjustmentsImageProvider *m_provider = nullptr;
    QString m_providerId;
    QImage m_source, m_previewSource;
    NkShot::Imaging::AdjustmentOptions m_options;
    quint64 m_revision = 0;
    bool m_ready = false;
    QString m_error;
};
