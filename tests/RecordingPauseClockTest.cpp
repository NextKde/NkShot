// SPDX-License-Identifier: GPL-3.0-only
#include "Platforms/VideoPlatform.h"
#include <QtTest/QTest>
#include <QSignalSpy>
class ClockPlatform final : public VideoPlatform {
public:
    RecordingModes supportedRecordingModes() const override { return Region; }
    Formats supportedFormats() const override { return MP4_H264; }
    bool isRecordingAudio() const override { return false; }
    void startRecording(const QUrl &,RecordingMode,const QVariantMap &,bool) override {}
    void finishRecording() override {}
    void state(RecordingState value) { setRecordingState(value); }
    void mode(RecordingMode value) { setRecordingMode(value); }
};
class RecordingPauseClockTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void preservesStateAndActiveTime() {
        using State=VideoPlatform::RecordingState;
        ClockPlatform platform;
        QCOMPARE(int(State::Finished),3);QCOMPARE(int(State::Paused),4);
        QVERIFY(!platform.supportsPause());QVERIFY(!platform.pausePending());
        platform.mode(VideoPlatform::Region);platform.state(State::Recording);
        QTest::qWait(120);platform.state(State::Paused);
        const auto first=platform.recordedTime();
        QVERIFY(first>=100);QVERIFY(platform.isRecording());QVERIFY(platform.isPaused());
        QVERIFY(!platform.readyForRecording());QCOMPARE(platform.recordingMode(),VideoPlatform::Region);
        QSignalSpy changed(&platform,&VideoPlatform::recordedTimeChanged);
        QTest::qWait(1100);QCOMPARE(platform.recordedTime(),first);QCOMPARE(changed.size(),0);
        platform.state(State::Recording);
        QCOMPARE(platform.recordedTime(),first);QVERIFY(!platform.isPaused());
        QCOMPARE(platform.recordingMode(),VideoPlatform::Region);
        QTest::qWait(120);platform.state(State::Paused);
        QVERIFY(platform.recordedTime()>=first+100);
        const auto second=platform.recordedTime();
        platform.state(State::Paused);QCOMPARE(platform.recordedTime(),second);
        platform.state(State::Rendering);QCOMPARE(platform.recordedTime(),second);
        QVERIFY(!platform.isRecording());QVERIFY(!platform.readyForRecording());
        platform.state(State::Finished);QCOMPARE(platform.recordedTime(),second);
        platform.mode(VideoPlatform::Screen);platform.state(State::Recording);
        QCOMPARE(platform.recordedTime(),0);
        platform.state(State::NotRecording);QCOMPARE(platform.recordedTime(),0);
        QVERIFY(platform.readyForRecording());
    }
    void invalidPauseDoesNotCreateRecording() {
        ClockPlatform platform;
        platform.state(VideoPlatform::RecordingState::Paused);
        QCOMPARE(platform.recordingState(),VideoPlatform::RecordingState::NotRecording);
        platform.pauseRecording();platform.resumeRecording();
        QVERIFY(!platform.isRecording());
    }
};
QTEST_GUILESS_MAIN(RecordingPauseClockTest)
#include "RecordingPauseClockTest.moc"
