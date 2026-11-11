/* This file is part of Spectacle, the KDE screenshot utility
 * SPDX-FileCopyrightText: 2015 Boudhayan Gupta <bgupta@kde.org>
 * SPDX-FileCopyrightText: 2022 Noah Davis <noahadvs@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "Gui/SpectacleWindow.h"
#include <QPalette>
#include <QPointer>
#include <QImage>
#include <functional>

class ViewerWindowPrivate;

/**
 * The window used for viewing media after it has been accepted or finished recording.
 * This has to be a separate window from the selection/capture window because reusing
 * the same window and changing the flags doesn't work nicely on Wayland. For example,
 * the window uses default window decorations instead of normal decorations.
 */
class ViewerWindow : public SpectacleWindow
{
    Q_OBJECT
    Q_PROPERTY(bool scanningCode READ scanningCode NOTIFY scanningCodeChanged)
    Q_PROPERTY(bool scrollPreviewEnabled READ scrollPreviewEnabled CONSTANT)
    Q_PROPERTY(bool resizingImage READ resizingImage NOTIFY resizingImageChanged)
    Q_PROPERTY(bool combiningImages READ combiningImages NOTIFY combiningImagesChanged)
public:
    enum Mode {
        Dialog,
        Viewer,
    };

    using UniquePointer = std::unique_ptr<ViewerWindow, void (*)(ViewerWindow *)>;

    static UniquePointer makeUnique(Mode mode, QQmlEngine *engine, QWindow *parent = nullptr);

    static ViewerWindow *instance();

    bool scrollPreviewEnabled() const;
    Q_INVOKABLE QQuickWindow *openScrollPreview();
    Q_INVOKABLE void startDrag();
    Q_INVOKABLE QQuickWindow *pinImage();
    Q_INVOKABLE QQuickWindow *openQuickNote();
    Q_INVOKABLE void scanImageCode();
    bool scanningCode() const { return m_scanningCode; }
    Q_INVOKABLE QSize imageSnapshotSize() const;
    Q_INVOKABLE void resizeImageCopy(int width);
    Q_INVOKABLE void beautifyImageCopy(int padding, int style, const QString &watermark, bool rounded);
    bool resizingImage() const { return m_resizingImage; }
    Q_INVOKABLE void combineImages(bool stackWithoutOverlap = false);
    bool combiningImages() const { return m_combiningImages; }
Q_SIGNALS:
    void scanningCodeChanged();
    void combiningImagesChanged();
    void resizingImageChanged();

protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    explicit ViewerWindow(Mode mode, QQmlEngine *engine, QWindow *parent = nullptr);
    ~ViewerWindow();

    void createImageCopy(std::function<QImage(const QImage &)> transform, const QString &title, const QString &failure);
    void setMode(ViewerWindow::Mode mode);
    Q_SLOT void updateColor();
    Q_SLOT void updateMinimumSize();

    void setBackgroundColorRole(QPalette::ColorRole role);

    QPointer<QQuickWindow> m_quickNote;
    QPointer<QQuickWindow> m_scrollPreview;
    QPointer<QObject> m_combineDialog;
    bool m_combiningImages = false;
    bool m_scanningCode = false;
    quint64 m_codeScanEpoch = 0;
    bool m_resizingImage = false;
    bool m_pixmapExists = false;
    QPalette::ColorRole m_backgroundColorRole;
    Qt::WindowStates m_oldWindowStates;
    const Mode m_mode;
    static ViewerWindow *s_viewerWindowInstance;
};
