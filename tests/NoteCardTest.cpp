// SPDX-License-Identifier: GPL-3.0-only
#include "Imaging/NoteCard.h"
#include <QtTest>
using NkShot::Imaging::renderNoteCard;
class NoteCardTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void plainTextAndStyles() {
        const QString text=QStringLiteral("Review before sharing\n中文备注与截图\n<b>literal text</b>");
        for(int style=0;style<3;++style) {
            const auto image=renderNoteCard(text,style);QVERIFY(!image.isNull());QCOMPARE(image.width(),720);QVERIFY(image.height()>=320&&image.height()<=2400);
            QCOMPARE(image,renderNoteCard(text,style));QCOMPARE(image.pixelColor(0,0).alpha(),255);
            QVERIFY(image!=renderNoteCard(QStringLiteral("Review before sharing\n中文备注与截图\nliteral text"),style));
        }
        QVERIFY(renderNoteCard(text,0)!=renderNoteCard(text,2));
    }
    void limits() {
        QVERIFY(renderNoteCard(QString(),0).isNull());QVERIFY(renderNoteCard(QStringLiteral("   \n"),0).isNull());
        QVERIFY(renderNoteCard(QString(2001,u'x'),0).isNull());QVERIFY(renderNoteCard(QStringLiteral("line\n").repeated(300),0).isNull());
        QVERIFY(renderNoteCard(QStringLiteral("hello"),-1).isNull());QVERIFY(renderNoteCard(QStringLiteral("hello"),3).isNull());
        QVERIFY(!renderNoteCard(QString(1200,u'x'),1).isNull());
    }
};
QTEST_MAIN(NoteCardTest)
#include "NoteCardTest.moc"
