// SPDX-License-Identifier: GPL-3.0-only
#include "LocalTextExtraction.h"
#include <QPainter>
#include <QRegularExpression>
#include <QSemaphore>
#include <QScopeGuard>
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>
#include <exception>
namespace NkShot::Imaging {
TextExtractionResult extractLocalText(const QImage &image,QStringList languages,const std::shared_ptr<std::atomic_bool> &cancelled,int deadlineMs)
{
    const auto stopped=[&]{return cancelled && cancelled->load();};
    if(stopped()) return {{},TextExtractionError::Cancelled};
    if(image.isNull() || image.width()>8192 || image.height()>32000 || qint64(image.width())*image.height()>20000000 || deadlineMs<1 || deadlineMs>30000)
        return {{},TextExtractionError::InvalidInput};
    languages.removeDuplicates();
    const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_]+$"));
    if(languages.isEmpty() || languages.size()>4) return {{},TextExtractionError::InvalidLanguage};
    for(const auto &language:languages) if(!safe.match(language).hasMatch() || language==QStringLiteral("osd")) return {{},TextExtractionError::InvalidLanguage};
    static QSemaphore budget(2);
    if(!budget.tryAcquire()) return {{},TextExtractionError::Busy};
    const auto release=qScopeGuard([&]{budget.release();});
    try {
        tesseract::TessBaseAPI api;
        if(api.Init(nullptr,languages.join(u'+').toUtf8().constData())!=0) return {{},TextExtractionError::Unavailable};
        if(stopped()) return {{},TextExtractionError::Cancelled};
        // Work in physical pixels and flatten transparency onto white for OCR.
        auto content=image;content.setDevicePixelRatio(1);
        QImage opaque(image.size(),QImage::Format_RGB32);opaque.fill(Qt::white);
        {QPainter painter(&opaque);painter.drawImage(QPoint(),content);}
        auto rgb=opaque.convertToFormat(QImage::Format_RGB888);
        if(rgb.isNull()) return {{},TextExtractionError::InvalidInput};
        api.SetPageSegMode(tesseract::PSM_AUTO);
        api.SetImage(rgb.constBits(),rgb.width(),rgb.height(),3,rgb.bytesPerLine());
        tesseract::ETEXT_DESC monitor;monitor.set_deadline_msecs(deadlineMs);
        monitor.cancel_this=cancelled.get();
        monitor.cancel=[](void *state,int){return state && static_cast<std::atomic_bool *>(state)->load();};
        const auto status=api.Recognize(&monitor);
        if(stopped()) return {{},TextExtractionError::Cancelled};
        if(monitor.deadline_exceeded()) return {{},TextExtractionError::Timeout};
        if(status!=0) return {{},TextExtractionError::RecognitionFailed};
        QStringList lines;
        std::unique_ptr<tesseract::ResultIterator> it(api.GetIterator());
        if(it) do {
            std::unique_ptr<char[]> raw(it->GetUTF8Text(tesseract::RIL_TEXTLINE));
            if(raw) {const auto line=QString::fromUtf8(raw.get()).trimmed();if(!line.isEmpty()) lines.append(line);}
        } while(it->Next(tesseract::RIL_TEXTLINE));
        if(stopped()) return {{},TextExtractionError::Cancelled};
        return {lines.join(u'\n').trimmed(),TextExtractionError::None};
    } catch(const std::exception &) {return {{},TextExtractionError::RecognitionFailed};}
}
}
