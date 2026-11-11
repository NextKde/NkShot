// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QPointer>
#include <QImage>
#include <KQuickImageEditor/AnnotationDocument>
class ImageAdjustmentsPreview;
class QMessageBox;
class QFileDialog;
class QWidget;
// Standalone editing session. No dependency on SpectacleCore or its document.
class ImageCopyEditor : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(QObject *adjustmentPreview READ adjustmentPreviewObject CONSTANT)
    Q_PROPERTY(bool adjustingCopy READ adjustingCopy NOTIFY adjustmentStateChanged)
    Q_PROPERTY(bool adjustmentBlocked READ adjustmentBlocked NOTIFY adjustmentStateChanged)
    Q_PROPERTY(AnnotationDocument *document READ document CONSTANT)
    Q_PROPERTY(bool jpegAvailable READ jpegAvailable CONSTANT)
    Q_PROPERTY(bool webpAvailable READ webpAvailable CONSTANT)
    Q_PROPERTY(bool annotationMode READ annotationMode CONSTANT)
    Q_PROPERTY(QImage originalImage READ originalImage CONSTANT)
    Q_PROPERTY(QSize originalSize READ originalSize CONSTANT)
    Q_PROPERTY(bool compareAvailable READ compareAvailable CONSTANT)
    Q_PROPERTY(bool comparing READ comparing NOTIFY comparingChanged)
public:
    ImageCopyEditor(const QImage &image, QQmlEngine *engine, bool annotationMode = false);
    ~ImageCopyEditor() override;
    QImage originalImage() const { return m_annotationMode ? QImage() : m_originalImage; }
    QSize originalSize() const { return originalImage().size(); }
    bool compareAvailable() const { return !m_annotationMode && !m_originalImage.isNull(); }
    bool comparing() const { return m_comparing; }
    Q_INVOKABLE void beginComparison();
    Q_INVOKABLE void endComparison();
    bool annotationMode() const { return m_annotationMode; }
    AnnotationDocument *document() { return m_document; }
    QObject *adjustmentPreviewObject() const;
    bool adjustingCopy() const { return m_adjustmentBusy; }
    bool adjustmentBlocked() const { return m_adjustmentActive || m_adjustmentBusy; }
    Q_INVOKABLE bool beginAdjustments();
    Q_INVOKABLE void cancelAdjustments();
    Q_INVOKABLE bool createAdjustedCopy();
    Q_INVOKABLE void copyImage();
    Q_INVOKABLE void saveAs();
    bool jpegAvailable() const;
    bool webpAvailable() const;
    Q_INVOKABLE bool exportAs(int format, int quality);
    bool hasUnsavedChanges() const;
    Q_INVOKABLE QQuickWindow *finishCopy();
    Q_INVOKABLE void showColorDialog(int option);
    Q_INVOKABLE void showFontDialog();
    Q_INVOKABLE qreal dprRound(qreal value) const { return qRound(value * devicePixelRatio()) / devicePixelRatio(); }
Q_SIGNALS:
    void adjustmentStateChanged();
    void comparingChanged();
    void imageCopied();
    void annotationsCommitted(const QImage &image);
    void saveFinished(const QString &message, bool success);
protected:
    bool event(QEvent *event) override;
private:
    void prepareDialog(QWidget *dialog);
    bool commitAnnotations();
    AnnotationDocument *m_document;
    ImageAdjustmentsPreview *m_adjustmentPreview = nullptr;
    bool m_adjustmentActive = false;
    bool m_adjustmentBusy = false;
    QPointer<QWidget> m_optionsDialog;
    QPointer<QMessageBox> m_closeDialog;
    QPointer<QFileDialog> m_saveDialog;
    const QImage m_originalImage;
    bool m_comparing = false;
    QImage m_savedImage;
    const bool m_annotationMode;
    QImage m_referenceImage;
    QImage m_referenceCanvas;
    bool m_acceptClose = false;
};
