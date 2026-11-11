// SPDX-License-Identifier: GPL-3.0-only
#include "NoteCard.h"
#include <QTextDocument>
#include <QTextOption>
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QFont>
#include <cmath>
namespace NkShot::Imaging {
QImage renderNoteCard(const QString &text,int style)
{
    if(text.trimmed().isEmpty() || text.size()>2000 || style<0 || style>2) return {};
    QTextDocument doc;doc.setDocumentMargin(0);
    QFont font; font.setPixelSize(28);doc.setDefaultFont(font);
    QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);doc.setDefaultTextOption(option);
    doc.setPlainText(text);doc.setTextWidth(624);
    const int height=qMax(320,int(std::ceil(doc.size().height()))+112);
    if(height>2400) return {};
    const QColor backgrounds[]={QColor("#fffdf6"),QColor("#edf4ff"),QColor("#20232c")};
    const QColor foregrounds[]={QColor("#27252a"),QColor("#18304c"),QColor("#f5f5f7")};
    QImage image(720,height,QImage::Format_ARGB32_Premultiplied);if(image.isNull())return {};
    image.fill(backgrounds[style]);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);p.setBrush(style==2?QColor("#a6bcff"):QColor("#587ad1"));p.drawRoundedRect(QRectF(48,28,36,4),2,2);
    p.translate(48,56);QAbstractTextDocumentLayout::PaintContext ctx;ctx.palette.setColor(QPalette::Text,foregrounds[style]);
    doc.documentLayout()->draw(&p,ctx);return image;
}
}
