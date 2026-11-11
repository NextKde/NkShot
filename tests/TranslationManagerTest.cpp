// SPDX-License-Identifier: GPL-3.0-only
// Protocol/process tests only. Echo workers are never model-quality evidence.
#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <memory>
#ifdef Q_OS_UNIX
#include <signal.h>
#include <cerrno>
#endif
#include "Translation/TranslationManager.h"

class TranslationManagerTest : public QObject
{
    Q_OBJECT
    QMap<QByteArray, QByteArray> m_environment;
    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_python, m_root, m_runner, m_models, m_pid, m_args;
    const QList<QByteArray> m_keys{
        "XDG_DATA_HOME", "NKSHOT_TRANSLATION_PYTHON", "NKSHOT_TRANSLATION_RUNNER",
        "NKSHOT_TRANSLATION_MODEL_DIR", "NKSHOT_TEST_TRANSLATION_MODE",
        "NKSHOT_TEST_TRANSLATION_PID", "NKSHOT_TEST_TRANSLATION_ARGV"};
    static bool write(const QString &path, const QByteArray &bytes)
    {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }
    static QByteArray read(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }
    void overrides()
    {
        qputenv("NKSHOT_TRANSLATION_PYTHON", QFile::encodeName(m_python));
        qputenv("NKSHOT_TRANSLATION_RUNNER", QFile::encodeName(m_runner));
        qputenv("NKSHOT_TRANSLATION_MODEL_DIR", QFile::encodeName(m_models));
    }
    static bool gone(qint64 pid)
    {
#ifdef Q_OS_UNIX
        errno = 0;
        return pid > 0 && ::kill(pid_t(pid), 0) == -1 && errno == ESRCH;
#else
        Q_UNUSED(pid)
        return true; // Non-Unix runs still check no late delivery; no process-liveness claim.
#endif
    }
private Q_SLOTS:
    void initTestCase()
    {
        m_python = QStandardPaths::findExecutable(QStringLiteral("python3"));
        QVERIFY2(!m_python.isEmpty(), "Protocol tests require a local Python3 interpreter (no packages or model downloads)");
    }
    void init()
    {
        for (const auto &key : m_keys) { m_environment[key] = qgetenv(key.constData()); qunsetenv(key.constData()); }
        m_dir = std::make_unique<QTemporaryDir>(); QVERIFY(m_dir->isValid());
        qputenv("XDG_DATA_HOME", QFile::encodeName(m_dir->path()));
        m_root = m_dir->path() + QStringLiteral("/nkshot/translation");
        m_runner = m_root + QStringLiteral("/translate.py");
        m_models = m_root + QStringLiteral("/models");
        m_pid = m_dir->path() + QStringLiteral("/pid");
        m_args = m_dir->path() + QStringLiteral("/argv.json");
        qputenv("NKSHOT_TEST_TRANSLATION_PID", QFile::encodeName(m_pid));
        qputenv("NKSHOT_TEST_TRANSLATION_ARGV", QFile::encodeName(m_args));
        for (const auto &direction : {QStringLiteral("en_zh"), QStringLiteral("zh_en")}) {
            const auto base = m_models + QLatin1Char('/') + direction;
            QVERIFY(write(base + QStringLiteral("/model/model.bin"), "test-only-placeholder"));
            QVERIFY(write(base + QStringLiteral("/sentencepiece.model"), "test-only-placeholder"));
            QVERIFY(write(base + QStringLiteral("/metadata.json"), "{}"));
        }
        QVERIFY(QDir().mkpath(m_root + QStringLiteral("/venv/bin")));
        QVERIFY(QFile::link(m_python, m_root + QStringLiteral("/venv/bin/python")));
        QVERIFY(write(m_runner, R"PY(import sys,json,os,time
try:
 import resource
 resource.setrlimit(resource.RLIMIT_CORE,(0,0))
except ImportError: pass
# Test-only protocol echo/fault generator. No production model or translation.
with open(os.environ['NKSHOT_TEST_TRANSLATION_PID'],'w') as f: f.write(str(os.getpid()))
with open(os.environ['NKSHOT_TEST_TRANSLATION_ARGV'],'w') as f: json.dump(sys.argv,f)
request=json.loads(sys.stdin.readline())
assert sys.stdin.read()==''
assert request['direction'] in ('en_zh','zh_en')
mode=os.environ.get('NKSHOT_TEST_TRANSLATION_MODE','normal')
if mode=='slow': time.sleep(10)
if mode=='invalid': print('{oops'); sys.exit(0)
if mode=='multiple': print('{"ok":true,"text":"one"}\n{"ok":true,"text":"two"}'); sys.exit(0)
if mode=='empty': print('{"ok":true,"text":" "}'); sys.exit(0)
if mode=='nonstring': print('{"ok":true,"text":42}'); sys.exit(0)
if mode=='stdout': sys.stdout.write('X'*100000); sys.stdout.flush(); time.sleep(10)
if mode=='stderr': sys.stderr.write('PRIVATE_DIAGNOSTIC'*10000); sys.stderr.flush(); time.sleep(10)
if mode=='exit': sys.exit(3)
if mode=='crash': os.abort()
if mode=='runtime': print('{"ok":false,"errorCode":"runtime_unavailable"}'); sys.exit(0)
if mode=='model': print('{"ok":false,"errorCode":"model_invalid"}'); sys.exit(0)
if mode=='sentence': print('{"ok":false,"errorCode":"sentence_too_long"}'); sys.exit(0)
if mode=='unknown': print('{"ok":false,"errorCode":"PRIVATE_DIAGNOSTIC"}'); sys.exit(0)
print(json.dumps({'ok':True,'text':'translated:'+request['text']},ensure_ascii=False))
)PY"));
    }
    void cleanup()
    {
        for (const auto &key : m_keys) {
            const auto value = m_environment.value(key);
            if (value.isNull()) qunsetenv(key.constData()); else qputenv(key.constData(), value);
        }
        m_environment.clear(); m_dir.reset();
    }
    void defaultDiscoveryAndExplicitPaths()
    {
        TranslationManager manager;
        QSignalSpy changed(&manager, &TranslationManager::changed);
        const QString source = QStringLiteral("PRIVATE source <tag>\n中文 https://example.invalid/");
        manager.translate(source, QStringLiteral("en_zh")); QVERIFY(manager.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000);
        QCOMPARE(manager.translatedText(), QStringLiteral("translated:") + source);
        QVERIFY(changed.count() >= 2);
        const auto args = QJsonDocument::fromJson(read(m_args)).array();
        QCOMPARE(args.size(), 1); QCOMPARE(args.first().toString(), m_runner);
        QVERIFY(!read(m_args).contains("PRIVATE source"));
        // Explicit complete overrides win even if the default installation is absent.
        overrides(); qputenv("XDG_DATA_HOME", QFile::encodeName(m_dir->path() + QStringLiteral("/missing-default")));
        manager.translate(QStringLiteral("反向文本"), QStringLiteral("zh_en"));
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000);
        QCOMPARE(manager.translatedText(), QStringLiteral("translated:反向文本"));
        manager.cancel(); QVERIFY(manager.translatedText().isEmpty()); QVERIFY(manager.status().isEmpty());
    }
    void configurationAndInputBoundaries()
    {
        TranslationManager manager;
        qputenv("NKSHOT_TRANSLATION_RUNNER", QFile::encodeName(m_runner));
        manager.translate(QStringLiteral("text"), QStringLiteral("en_zh"));
        QVERIFY(!manager.busy()); QVERIFY(manager.status().contains(QStringLiteral("incomplete"))); QVERIFY(!QFileInfo::exists(m_pid));
        overrides(); qputenv("NKSHOT_TRANSLATION_PYTHON", "/missing/python");
        manager.translate(QStringLiteral("text"), QStringLiteral("en_zh")); QVERIFY(manager.status().contains(QStringLiteral("not installed")));
        overrides(); qputenv("NKSHOT_TRANSLATION_RUNNER", "/missing/worker.py");
        manager.translate(QStringLiteral("text"), QStringLiteral("en_zh")); QVERIFY(manager.status().contains(QStringLiteral("worker")));
        overrides(); qputenv("NKSHOT_TRANSLATION_MODEL_DIR", "/missing/model");
        manager.translate(QStringLiteral("text"), QStringLiteral("zh_en")); QVERIFY(manager.status().contains(QStringLiteral("model")));
        overrides();
        for (const auto &text : {QString(), QStringLiteral(" \n"), QString(4001, u'x')}) {
            manager.translate(text, QStringLiteral("en_zh")); QVERIFY(!manager.busy());
            QVERIFY(!manager.status().isEmpty()); QVERIFY(manager.translatedText().isEmpty()); QVERIFY(!QFileInfo::exists(m_pid));
        }
        manager.translate(QStringLiteral("text"), QStringLiteral("unsupported")); QVERIFY(!manager.busy()); QVERIFY(!QFileInfo::exists(m_pid));
        // Inclusive 4000-unit boundary accepted by the process protocol.
        manager.translate(QString(4000, u'x'), QStringLiteral("en_zh"));
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000); QCOMPARE(manager.translatedText().size(), 4011);
        // Filesystem checks pass, but exec itself must fail cleanly.
        const auto invalidExecutable = m_dir->path() + QStringLiteral("/not-python");
        QVERIFY(write(invalidExecutable, "not an executable format"));
        QVERIFY(QFile::setPermissions(invalidExecutable, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        qputenv("NKSHOT_TRANSLATION_PYTHON", QFile::encodeName(invalidExecutable));
        manager.translate(QStringLiteral("text"), QStringLiteral("en_zh"));
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000); QVERIFY(!manager.status().isEmpty()); QVERIFY(manager.translatedText().isEmpty());
    }
    void protocolFailuresAndOutputBounds()
    {
        overrides();
        for (const auto &mode : {"invalid", "multiple", "empty", "nonstring", "stdout", "stderr", "exit", "crash", "runtime", "model", "sentence", "unknown"}) {
            qputenv("NKSHOT_TEST_TRANSLATION_MODE", mode);
            QFile::remove(m_pid);
            TranslationManager manager;
            manager.translate(QStringLiteral("PRIVATE user content"), QStringLiteral("en_zh"));
            QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000);
            QVERIFY2(manager.translatedText().isEmpty(), mode); QVERIFY2(!manager.status().isEmpty(), mode);
            QVERIFY(!manager.status().contains(QStringLiteral("PRIVATE")));
            const auto pid = read(m_pid).toLongLong();
            QVERIFY2(pid > 0, mode); // A failed interpreter start cannot masquerade as a protocol test.
            QTRY_VERIFY_WITH_TIMEOUT(gone(pid), 3000);
            if (QByteArray(mode) == "stdout") QVERIFY(manager.status().contains(QStringLiteral("output limit")));
            if (QByteArray(mode) == "runtime") QVERIFY(manager.status().contains(QStringLiteral("not installed")));
            if (QByteArray(mode) == "model") QVERIFY(manager.status().contains(QStringLiteral("model")));
            if (QByteArray(mode) == "sentence") QVERIFY(manager.status().contains(QStringLiteral("too long")));
        }
    }
    void cancellationReplacementAndDestruction()
    {
        overrides(); qputenv("NKSHOT_TEST_TRANSLATION_MODE", "slow");
        TranslationManager manager;
        // Cancellation during startup must not resurrect busy/result state.
        manager.translate(QStringLiteral("starting"), QStringLiteral("en_zh")); manager.cancel();
        QVERIFY(!manager.busy()); QTest::qWait(80); QVERIFY(manager.translatedText().isEmpty());
        QFile::remove(m_pid);
        manager.translate(QStringLiteral("old"), QStringLiteral("en_zh"));
        QTRY_VERIFY_WITH_TIMEOUT(read(m_pid).toLongLong() > 0, 3000);
        const auto oldPid = read(m_pid).toLongLong();
        qputenv("NKSHOT_TEST_TRANSLATION_MODE", "normal");
        manager.translate(QStringLiteral("new"), QStringLiteral("zh_en"));
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000);
        QCOMPARE(manager.translatedText(), QStringLiteral("translated:new"));
        QTRY_VERIFY_WITH_TIMEOUT(gone(oldPid), 3000);
        QTest::qWait(100); QCOMPARE(manager.translatedText(), QStringLiteral("translated:new"));
        qputenv("NKSHOT_TEST_TRANSLATION_MODE", "slow"); QFile::remove(m_pid);
        manager.translate(QStringLiteral("cancel"), QStringLiteral("en_zh"));
        QTRY_VERIFY_WITH_TIMEOUT(read(m_pid).toLongLong() > 0, 3000);
        const auto cancelPid = read(m_pid).toLongLong(); manager.cancel();
        QVERIFY(!manager.busy()); QVERIFY(manager.translatedText().isEmpty()); QVERIFY(manager.status().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(gone(cancelPid), 3000);
        QFile::remove(m_pid);
        auto doomed = new TranslationManager;
        doomed->translate(QStringLiteral("destroy"), QStringLiteral("en_zh"));
        QTRY_VERIFY_WITH_TIMEOUT(read(m_pid).toLongLong() > 0, 3000);
        const auto doomedPid = read(m_pid).toLongLong();
        QElapsedTimer elapsed; elapsed.start(); delete doomed;
        QVERIFY(elapsed.elapsed() < 1000);
        QTRY_VERIFY_WITH_TIMEOUT(gone(doomedPid), 3000);
    }
    void deadline()
    {
        overrides(); qputenv("NKSHOT_TEST_TRANSLATION_MODE", "slow");
        TranslationManager manager(nullptr, 1000);
        manager.translate(QStringLiteral("deadline"), QStringLiteral("en_zh"));
        QTRY_VERIFY_WITH_TIMEOUT(read(m_pid).toLongLong() > 0, 3000);
        const auto pid = read(m_pid).toLongLong();
        QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 3000);
        QVERIFY(manager.status().contains(QStringLiteral("timed out"))); QVERIFY(manager.translatedText().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(gone(pid), 3000);
        QTest::qWait(100); QVERIFY(manager.translatedText().isEmpty());
    }
};
QTEST_GUILESS_MAIN(TranslationManagerTest)
#include "TranslationManagerTest.moc"
