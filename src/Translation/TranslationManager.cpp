// SPDX-License-Identifier: GPL-3.0-only
#include "TranslationManager.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <KLocalizedString>

TranslationManager::TranslationManager(QObject *parent, int timeoutMs) : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(qMax(1, timeoutMs));
    connect(&m_timeout, &QTimer::timeout, this, [this] { fail(QStringLiteral("timeout")); });
}
TranslationManager::~TranslationManager() { stop(); }
void TranslationManager::stop()
{
    ++m_epoch;
    m_timeout.stop();
    auto process = m_process.data();
    m_process = nullptr;
    if (process) {
        process->disconnect(this);
        // The process owns its finished cleanup independently of this window.
        // No waitForFinished() on the GUI thread and no late result delivery.
        if (process->state() == QProcess::NotRunning) process->deleteLater();
        else process->kill();
    }
    m_output.clear();
}
void TranslationManager::cancel()
{
    stop(); m_text.clear(); m_status.clear(); Q_EMIT changed();
}
void TranslationManager::fail(const QString &code)
{
    stop(); m_text.clear();
    if (code == QLatin1String("runtime_missing") || code == QLatin1String("runtime_unavailable"))
        m_status = i18nc("@info", "Offline translation is not installed. Set up the local model pack first.");
    else if (code == QLatin1String("configuration_incomplete"))
        m_status = i18nc("@info", "Local translation path overrides are incomplete. Configure all three paths, or clear the overrides to use the installed offline pack. See the installation guide.");
    else if (code == QLatin1String("runner_missing"))
        m_status = i18nc("@info", "The local translation worker is not configured. See the offline translation installation guide.");
    else if (code == QLatin1String("model_missing") || code == QLatin1String("model_invalid"))
        m_status = i18nc("@info", "The local model for this direction is missing or invalid. Install the documented offline model pack; no model is downloaded automatically.");
    else if (code == QLatin1String("input_limit") || code == QLatin1String("input_too_long") || code == QLatin1String("sentence_too_long"))
        m_status = i18nc("@info", "This passage is too long for local translation. Use up to 4,000 characters with shorter sentences.");
    else if (code == QLatin1String("empty_input"))
        m_status = i18nc("@info", "There is no text to translate.");
    else if (code == QLatin1String("timeout"))
        m_status = i18nc("@info", "Local translation timed out. Try a shorter passage.");
    else if (code == QLatin1String("output_limit") || code == QLatin1String("output_too_long"))
        m_status = i18nc("@info", "The translation exceeded the output limit. Try a shorter passage.");
    else m_status = i18nc("@info", "Local translation could not complete. Check the installed runtime and model, then try again.");
    Q_EMIT changed();
}
void TranslationManager::translate(const QString &text, const QString &direction)
{
    // Replacement always invalidates the earlier request, even during startup.
    stop(); m_text.clear(); m_status.clear();
    if (direction != QLatin1String("en_zh") && direction != QLatin1String("zh_en")) { fail(QStringLiteral("input_invalid")); return; }
    if (text.trimmed().isEmpty()) { fail(QStringLiteral("empty_input")); return; }
    if (text.size() > 4000 || text.toUtf8().size() > 16000) { fail(QStringLiteral("input_limit")); return; }
    const bool pythonOverride = qEnvironmentVariableIsSet("NKSHOT_TRANSLATION_PYTHON");
    const bool runnerOverride = qEnvironmentVariableIsSet("NKSHOT_TRANSLATION_RUNNER");
    const bool modelOverride = qEnvironmentVariableIsSet("NKSHOT_TRANSLATION_MODEL_DIR");
    const bool overridden = pythonOverride || runnerOverride || modelOverride;
    // Keep interpreter, worker and models from one installation together. A
    // partial override must not silently combine incompatible runtime versions.
    if (overridden && !(pythonOverride && runnerOverride && modelOverride)) {
        fail(QStringLiteral("configuration_incomplete")); return;
    }
    const auto root = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/nkshot/translation");
    const auto python = overridden ? qEnvironmentVariable("NKSHOT_TRANSLATION_PYTHON") : root + QStringLiteral("/venv/bin/python");
    const auto runner = overridden ? qEnvironmentVariable("NKSHOT_TRANSLATION_RUNNER") : root + QStringLiteral("/translate.py");
    const auto model = overridden ? qEnvironmentVariable("NKSHOT_TRANSLATION_MODEL_DIR") : root + QStringLiteral("/models");
    const auto selectedModel = model + QLatin1Char('/') + direction;
    const QFileInfo pyInfo(python), runnerInfo(runner), modelInfo(model);
    if (!pyInfo.isAbsolute() || !pyInfo.isFile() || !pyInfo.isExecutable()) { fail(QStringLiteral("runtime_missing")); return; }
    if (!runnerInfo.isAbsolute() || !runnerInfo.isFile() || !runnerInfo.isReadable()) { fail(QStringLiteral("runner_missing")); return; }
    if (!modelInfo.isAbsolute() || !modelInfo.isDir()
        || !QFileInfo::exists(selectedModel + QStringLiteral("/model/model.bin"))
        || !QFileInfo::exists(selectedModel + QStringLiteral("/sentencepiece.model"))
        || (direction == QLatin1String("en_zh") && !QFileInfo::exists(selectedModel + QStringLiteral("/metadata.json")))) { fail(QStringLiteral("model_missing")); return; }
    auto process = new QProcess;
    m_process = process;
    const auto epoch = m_epoch;
    m_output.clear(); m_stderrBytes = 0;
    process->setProcessChannelMode(QProcess::SeparateChannels);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("NKSHOT_TRANSLATION_MODEL_DIR"), model);
    environment.insert(QStringLiteral("UCX_VFS_ENABLE"), QStringLiteral("n"));
    process->setProcessEnvironment(environment);
    // Only interpreter and worker paths are arguments. Text travels through stdin.
    process->setProgram(python);
    process->setArguments({QStringLiteral("-I"), QStringLiteral("-u"), runner});
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process, &QObject::deleteLater);
    connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) process->deleteLater();
    });
    connect(process, &QProcess::started, this, [this, process, epoch, text, direction] {
        if (epoch != m_epoch || m_process != process) return;
        const auto request = QJsonDocument(QJsonObject{{QStringLiteral("direction"), direction}, {QStringLiteral("text"), text}}).toJson(QJsonDocument::Compact) + '\n';
        if (process->write(request) != request.size()) { fail(QStringLiteral("worker_failed")); return; }
        process->closeWriteChannel();
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, epoch] {
        if (epoch != m_epoch || m_process != process) return;
        process->setReadChannel(QProcess::StandardOutput);
        m_output += process->read(65537 - m_output.size());
        if (m_output.size() > 65536) fail(QStringLiteral("output_limit"));
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process, epoch] {
        if (epoch != m_epoch || m_process != process) return;
        // Never expose backend diagnostics, which may accidentally contain text.
        process->setReadChannel(QProcess::StandardError);
        m_stderrBytes += process->read(16385 - m_stderrBytes).size();
        if (m_stderrBytes > 16384) fail(QStringLiteral("worker_failed"));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, epoch](QProcess::ProcessError) {
        if (epoch == m_epoch && m_process == process) fail(QStringLiteral("worker_failed"));
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, epoch](int code, QProcess::ExitStatus exitStatus) {
        if (epoch != m_epoch || m_process != process) return;
        process->setReadChannel(QProcess::StandardOutput);
        m_output += process->read(65537 - m_output.size());
        if (m_output.size() > 65536) { fail(QStringLiteral("output_limit")); return; }
        if (code != 0 || exitStatus != QProcess::NormalExit) { fail(QStringLiteral("worker_failed")); return; }
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(m_output.trimmed(), &error);
        const auto result = document.object();
        if (error.error != QJsonParseError::NoError || !document.isObject() || !result.value(QStringLiteral("ok")).isBool()) { fail(QStringLiteral("protocol_error")); return; }
        if (!result.value(QStringLiteral("ok")).toBool()) { fail(result.value(QStringLiteral("errorCode")).toString()); return; }
        const auto translation = result.value(QStringLiteral("text"));
        if (!translation.isString() || translation.toString().trimmed().isEmpty()) { fail(QStringLiteral("protocol_error")); return; }
        const auto completed = translation.toString();
        stop(); m_text = completed;
        m_status = i18nc("@info", "Translated offline. Review the translation before using it.");
        Q_EMIT changed();
    });
    m_timeout.start(); process->start(); Q_EMIT changed();
}
