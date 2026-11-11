// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QList>
#include <QRect>
#include <QStringList>
#include <atomic>
#include <memory>
namespace NkShot::Imaging {
enum class TextExtractionError { None, InvalidInput, InvalidLanguage, Unavailable, Busy, Cancelled, Timeout, RecognitionFailed };
struct TextWord {
    QString text;
    QRect bounds; // Physical image pixels; independent of image DPR.
    QString separatorBefore; // Exact OCR spacing, or a newline before a new line.
    int lineIndex = 0; // Logical reading order, not a geometric left-to-right sort.
};
struct TextExtractionResult {
    QString text;
    TextExtractionError error = TextExtractionError::None;
    QList<TextWord> words;
    // False means selectable metadata was unavailable, invalid or exceeded its
    // bound. In that case words is empty; whole-image text remains available.
    bool wordsComplete = false;
};
// Inclusive logical range, accepting either drag direction. Invalid indices
// produce no text. The first selected word never includes its leading separator.
QString selectedWordText(const QList<TextWord> &words, int first, int last);

// Worker-thread helper. Owns its Tesseract instance; never writes settings or
// clipboard. Cancellation is cooperative, not unsafe thread termination.
// maxWords can lower the 2000-word metadata cap; it cannot raise it. Metadata is
// also bounded to 128 Ki UTF-16 units. Text/error retain their existing behavior.
TextExtractionResult extractLocalText(const QImage &,QStringList languages,
    const std::shared_ptr<std::atomic_bool> &cancelled, int deadlineMs=15000, int maxWords=2000);
}
