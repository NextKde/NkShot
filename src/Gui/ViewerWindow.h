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
#include <QList>
#include <QUrl>
#include <functional>

class ViewerWindowPrivate;
class VideoClipJob;
class ImageCopyEditor;
class PresentationPreview;
class RecentThumbnails;
class QVideoSink;
namespace NkShot { class LocalMediaLoader; }

/**
 * The window used for viewing media after it has been accepted or finished recording.
 * This has to be a separate window from the selection/capture window because reusing
 * the same window and changing the flags doesn't work nicely on Wayland. For example,
 * the window uses default window decorations instead of normal decorations.
 */
class ViewerWindow : public SpectacleWindow
{
    Q_OBJECT
    Q_PROPERTY(bool openingMedia READ openingMedia NOTIFY openingMediaChanged)
    Q_PROPERTY(bool canOpenMedia READ canOpenMedia NOTIFY openingMediaChanged)
    Q_PROPERTY(bool canDropMedia READ canDropMedia NOTIFY openingMediaChanged)
    Q_PROPERTY(bool scanningCode READ scanningCode NOTIFY scanningCodeChanged)
    Q_PROPERTY(bool resizingImage READ resizingImage NOTIFY resizingImageChanged)
    Q_PROPERTY(bool combiningImages READ combiningImages NOTIFY combiningImagesChanged)
    Q_PROPERTY(QObject *presentationPreview READ presentationPreviewObject CONSTANT)
    Q_PROPERTY(QObject *clipJob READ clipJobObject CONSTANT)
    Q_PROPERTY(QObject *recentThumbnails READ recentThumbnailsObject CONSTANT)
    Q_PROPERTY(QObject *recentMedia READ recentMediaObject CONSTANT)
    Q_PROPERTY(bool choosingClipOutput READ choosingClipOutput NOTIFY choosingClipOutputChanged)
public:
    enum Mode {
        Dialog,
        Viewer,
    };

    using UniquePointer = std::unique_ptr<ViewerWindow, void (*)(ViewerWindow *)>;

    Mode mode() const { return m_mode; }
    static UniquePointer makeUnique(Mode mode, QQmlEngine *engine, QWindow *parent = nullptr);

    static ViewerWindow *instance();

    Q_INVOKABLE void openMedia();
    Q_INVOKABLE void openRecentMedia(const QUrl &url);
    QObject *recentMediaObject() const;
    QObject *recentThumbnailsObject() const;
    Q_INVOKABLE void beginRecentThumbnails();
    Q_INVOKABLE void clearRecentThumbnails();
    Q_INVOKABLE QQuickWindow *editClipboardImage();
    bool openingMedia() const { return m_openingMedia; }
    bool canOpenMedia() const;
    bool canDropMedia() const;
    Q_INVOKABLE bool canAcceptMediaDrop(const QList<QUrl> &urls) const;
    Q_INVOKABLE bool openDroppedMedia(const QList<QUrl> &urls);
    Q_INVOKABLE QQuickWindow *openScrollPreview();
    Q_INVOKABLE void startDrag();
    Q_INVOKABLE QQuickWindow *pinImage();
    Q_INVOKABLE QQuickWindow *pinVideoFrame(QVideoSink *sink, const QUrl &source);
    Q_INVOKABLE QQuickWindow *openQuickNote();
    Q_INVOKABLE QQuickWindow *openColorPicker();
    Q_INVOKABLE void scanImageCode();
    bool scanningCode() const { return m_scanningCode; }
    Q_INVOKABLE QSize imageSnapshotSize() const;
    Q_INVOKABLE void resizeImageCopy(int width, int quarterTurns = 0, bool mirror = false);
    Q_INVOKABLE void beautifyImageCopy(int padding, int style, const QString &watermark, bool rounded);
    QObject *presentationPreviewObject() const;
    Q_INVOKABLE bool beginPresentationPreview();
    Q_INVOKABLE void cancelPresentationPreview();
    Q_INVOKABLE bool createPresentationCopy();
    bool resizingImage() const { return m_resizingImage; }
    Q_INVOKABLE void combineImages(bool stackWithoutOverlap = false, bool horizontal = false);
    bool combiningImages() const { return m_combiningImages; }
    QObject *clipJobObject() const;
    VideoClipJob *clipJob() const { return m_clipJob; }
    bool choosingClipOutput() const { return !m_clipSaveDialog.isNull(); }
    Q_INVOKABLE void prepareClipExport(const QUrl &source);
    Q_INVOKABLE QQuickWindow *editVideoAnnotations(QVideoSink *sink, const QUrl &source);
    Q_INVOKABLE void clearVideoAnnotations();
    Q_INVOKABLE void chooseClipOutput(qint64 startMs, qint64 endMs);
    Q_INVOKABLE void cancelClipExport();
Q_SIGNALS:
    void openingMediaChanged();
    void choosingClipOutputChanged();
    void clipUiCancelled();
    void scanningCodeChanged();
    void combiningImagesChanged();
    void resizingImageChanged();

protected:
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    explicit ViewerWindow(Mode mode, QQmlEngine *engine, QWindow *parent = nullptr);
    ~ViewerWindow();

    void loadOpenedMedia(const QUrl &url);
    void createImageCopy(std::function<QImage(const QImage &)> transform, const QString &title, const QString &failure, const QImage &sourceSnapshot = {});
    void setMode(ViewerWindow::Mode mode);
    Q_SLOT void updateColor();
    Q_SLOT void updateMinimumSize();

    void setBackgroundColorRole(QPalette::ColorRole role);

    void offerOpenedMedia(const QUrl &source, const QImage &image, bool video);
    void finishOpeningMedia();
    NkShot::LocalMediaLoader *m_mediaLoader = nullptr;
    QPointer<QObject> m_openDialog;
    bool m_openingMedia = false;
    quint64 m_openEpoch = 0;
    QPointer<QQuickWindow> m_colorPicker;
    QPointer<QQuickWindow> m_quickNote;
    QPointer<QQuickWindow> m_scrollPreview;
    QPointer<QObject> m_combineDialog;
    QPointer<QQuickWindow> m_combineReview;
    VideoClipJob *m_clipJob = nullptr;
    QPointer<QObject> m_clipSaveDialog;
    QPointer<ImageCopyEditor> m_videoAnnotationEditor;
    quint64 m_annotationSession = 0;
    bool m_combiningImages = false;
    bool m_scanningCode = false;
    quint64 m_codeScanEpoch = 0;
    bool m_resizingImage = false;
    RecentThumbnails *m_recentThumbnails = nullptr;
    PresentationPreview *m_presentationPreview = nullptr;
    bool m_pixmapExists = false;
    QPalette::ColorRole m_backgroundColorRole;
    Qt::WindowStates m_oldWindowStates;
    const Mode m_mode;
    static ViewerWindow *s_viewerWindowInstance;
};
