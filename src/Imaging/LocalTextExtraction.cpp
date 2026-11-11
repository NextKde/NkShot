// SPDX-License-Identifier: GPL-3.0-only
#include "LocalTextExtraction.h"
#include <QPainter>
#include <QRegularExpression>
#include <QSemaphore>
#include <QScopeGuard>
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>
#include <exception>
#include <algorithm>
namespace NkShot::Imaging {
QString selectedWordText(const QList<TextWord> &words, int first, int last)
{
    if (words.size() > 2000 || first < 0 || last < 0 || first >= words.size() || last >= words.size()) return {};
    if (first > last) std::swap(first, last);
    QString selected;
    for (int index = first; index <= last; ++index) {
        if (index != first) selected += words[index].separatorBefore;
        selected += words[index].text;
    }
    return selected;
}

TextExtractionResult extractLocalText(const QImage &image,QStringList languages,const std::shared_ptr<std::atomic_bool> &cancelled,int deadlineMs,int maxWords)
{
    const auto stopped=[&]{return cancelled && cancelled->load();};
    if(stopped()) return {{},TextExtractionError::Cancelled};
    if(image.isNull() || image.width()>8192 || image.height()>32000 || qint64(image.width())*image.height()>20000000 || deadlineMs<1 || deadlineMs>30000 || maxWords<1 || maxWords>2000)
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
        TextExtractionResult result{lines.join(u'\n').trimmed(), TextExtractionError::None};
        result.wordsComplete = true;
        it.reset(api.GetIterator());
        QString currentLine;
        qsizetype lineOffset = 0;
        qsizetype metadataCharacters = 0;
        int lineIndex = -1;
        if (it) do {
            if (stopped()) return {{}, TextExtractionError::Cancelled};
            std::unique_ptr<char[]> rawWord(it->GetUTF8Text(tesseract::RIL_WORD));
            if (!rawWord) continue;
            const QString word = QString::fromUtf8(rawWord.get()).trimmed();
            if (word.isEmpty()) continue;
            const bool newLine = lineIndex < 0 || it->IsAtBeginningOf(tesseract::RIL_TEXTLINE);
            if (newLine) {
                std::unique_ptr<char[]> rawLine(it->GetUTF8Text(tesseract::RIL_TEXTLINE));
                currentLine = rawLine ? QString::fromUtf8(rawLine.get()).trimmed() : QString();
                lineOffset = 0;
                ++lineIndex;
            }
            // Match each word against Tesseract's own logical line string rather
            // than inventing spaces from box positions. This preserves Chinese
            // joins and punctuation as well as ordinary inter-word whitespace.
            const qsizetype offset = currentLine.indexOf(word, lineOffset);
            int left = 0, top = 0, right = 0, bottom = 0;
            const bool boxFound = it->BoundingBox(tesseract::RIL_WORD, &left, &top, &right, &bottom);
            if (offset < lineOffset || !boxFound || left < 0 || top < 0
                || right <= left || bottom <= top || right > image.width() || bottom > image.height()
                || result.words.size() >= maxWords || word.size() > 1024) {
                result.wordsComplete = false;
                break;
            }
            QString separator = currentLine.mid(lineOffset, offset - lineOffset);
            if (newLine && !result.words.isEmpty()) separator.prepend(u'\n');
            metadataCharacters += word.size() + separator.size();
            if (metadataCharacters > 128 * 1024) {
                result.wordsComplete = false;
                break;
            }
            result.words.append({word, QRect(left, top, right - left, bottom - top), separator, lineIndex});
            lineOffset = offset + word.size();
        } while (it->Next(tesseract::RIL_WORD));
        if (stopped()) return {{}, TextExtractionError::Cancelled};
        // Never expose a partial word list as if it represented all the text.
        // Line-level text is intentionally retained for the existing reader.
        if (selectedWordText(result.words, 0, result.words.size() - 1) != result.text)
            result.wordsComplete = false;
        if (!result.wordsComplete) result.words.clear();
        return result;
    } catch(const std::exception &) {return {{},TextExtractionError::RecognitionFailed};}
}
}
