// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QQuickView>
#include <QPointer>
#include <QImage>
#include <KQuickImageEditor/AnnotationDocument>
class QMessageBox;
class QFileDialog;
class QWidget;
// Standalone editing session. No dependency on SpectacleCore or its document.
class ImageCopyEditor : public QQuickView {
    Q_OBJECT
    Q_PROPERTY(AnnotationDocument *document READ document CONSTANT)
public:
    ImageCopyEditor(const QImage &image, QQmlEngine *engine);
    ~ImageCopyEditor() override;
    AnnotationDocument *document() { return m_document; }
    Q_INVOKABLE void copyImage();
    Q_INVOKABLE void saveAs();
    bool hasUnsavedChanges() const;
    Q_INVOKABLE QQuickWindow *finishCopy();
    Q_INVOKABLE void showColorDialog(int option);
    Q_INVOKABLE void showFontDialog();
    Q_INVOKABLE qreal dprRound(qreal value) const { return qRound(value * devicePixelRatio()) / devicePixelRatio(); }
Q_SIGNALS:
    void imageCopied();
    void saveFinished(const QString &message, bool success);
protected:
    bool event(QEvent *event) override;
private:
    void prepareDialog(QWidget *dialog);
    AnnotationDocument *m_document;
    QPointer<QWidget> m_optionsDialog;
    QPointer<QMessageBox> m_closeDialog;
    QPointer<QFileDialog> m_saveDialog;
    QImage m_savedImage;
    bool m_acceptClose = false;
};
