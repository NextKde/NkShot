// SPDX-License-Identifier: GPL-3.0-only
#include "OcrManager.h"
#include "settings.h"
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QSignalSpy>
#include <KLocalizedString>
#include <QTemporaryDir>
#include <QtTest>

class OcrManagerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initializationAndAsyncEnglishRecognition()
    {
        Settings::setOcrLanguages({QStringLiteral("eng")});
        OcrManager manager;
        QSignalSpy availability(&manager, &OcrManager::availabilityChanged);
        QSignalSpy statuses(&manager, &OcrManager::statusChanged);
        QSignalSpy recognized(&manager, &OcrManager::textRecognized);
        QVERIFY(availability.isValid());
        QVERIFY(!manager.isAvailable());
        QCOMPARE(manager.status(), OcrManager::OcrStatus::Ready);
        QTRY_VERIFY_WITH_TIMEOUT(manager.isAvailable(), 10000);
        QCOMPARE(availability.count(), 1);
        // Ready -> Ready must not fabricate a processing-status transition.
        QCOMPARE(statuses.count(), 0);
        QVERIFY(manager.availableLanguagesWithNames().contains(QStringLiteral("eng")));

        QImage fixture(1000, 200, QImage::Format_RGB32);
        fixture.fill(Qt::white);
        QPainter painter(&fixture);
        QFont font(QStringLiteral("DejaVu Sans"));
        font.setPixelSize(64);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(fixture.rect(), Qt::AlignCenter, QStringLiteral("HELLO NKSHOT 12345"));
        painter.end();

        manager.recognizeTextWithLanguage(fixture, QStringLiteral("eng"));
        QCOMPARE(manager.status(), OcrManager::OcrStatus::Processing);
        QCOMPARE(recognized.count(), 0); // Completion arrives from the queued worker.
        QTRY_COMPARE_WITH_TIMEOUT(recognized.count(), 1, 15000);
        const auto result = recognized.takeFirst();
        QVERIFY(result.at(2).toBool());
        QCOMPARE(result.at(1).toStringList(), QStringList{QStringLiteral("eng")});
        QCOMPARE(result.at(0).toString().simplified(), QStringLiteral("HELLO NKSHOT 12345"));
        QCOMPARE(QApplication::clipboard()->text(), result.at(0).toString());
        QCOMPARE(manager.status(), OcrManager::OcrStatus::Ready);
        QCOMPARE(statuses.count(), 2);
        QCOMPARE(qvariant_cast<OcrManager::OcrStatus>(statuses.at(0).at(0)), OcrManager::OcrStatus::Processing);
        QCOMPARE(qvariant_cast<OcrManager::OcrStatus>(statuses.at(1).at(0)), OcrManager::OcrStatus::Ready);
        QCOMPARE(availability.count(), 1); // Processing is not loss of availability.
        // Rejecting invalid input neither starts a worker nor hides OCR.
        manager.recognizeTextWithLanguage(QImage(), QStringLiteral("eng"));
        QCOMPARE(recognized.count(), 1);
        QVERIFY(!recognized.at(0).at(2).toBool());
        QCOMPARE(statuses.count(), 2);
        QVERIFY(manager.isAvailable());
    }
};

int main(int argc, char **argv)
{
    // Never read/write the developer's settings or real desktop clipboard.
    QTemporaryDir config;
    if (!config.isValid()) {
        return 1;
    }
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("nkshot-ocr-test"));
    KLocalizedString::setApplicationDomain("nkshot");
    OcrManagerTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "OcrManagerTest.moc"
