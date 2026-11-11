// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QImage>
#include <QStringList>
#include <atomic>
#include <memory>
namespace NkShot::Imaging {
enum class TextExtractionError { None, InvalidInput, InvalidLanguage, Unavailable, Busy, Cancelled, Timeout, RecognitionFailed };
struct TextExtractionResult {QString text;TextExtractionError error=TextExtractionError::None;};
// Worker-thread helper. Owns its Tesseract instance; never writes settings or
// clipboard. Cancellation is cooperative, not unsafe thread termination.
TextExtractionResult extractLocalText(const QImage &,QStringList languages,
    const std::shared_ptr<std::atomic_bool> &cancelled, int deadlineMs=15000);
}
