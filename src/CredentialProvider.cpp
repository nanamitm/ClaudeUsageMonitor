#include "CredentialProvider.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStandardPaths>

QString CredentialProvider::credentialsPath() const
{
    const auto env = QProcessEnvironment::systemEnvironment();
    const QString configured = env.value("CLAUDE_CONFIG_DIR").trimmed();
    if (!configured.isEmpty()) {
        return QDir(configured).filePath(".credentials.json");
    }

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return QDir(home).filePath(".claude/.credentials.json");
}

QString CredentialProvider::accessToken(QString *errorMessage) const
{
    const QString path = credentialsPath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QString("Claude Code credentials were not found at %1.").arg(path);
        }
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QString("Claude Code credentials could not be parsed: %1.").arg(parseError.errorString());
        }
        return {};
    }

    const QJsonObject root = document.object();
    const QJsonObject oauth = root.value("claudeAiOauth").toObject();
    const QString token = oauth.value("accessToken").toString();
    if (token.isEmpty() && errorMessage) {
        *errorMessage = "Claude Code credentials do not contain claudeAiOauth.accessToken.";
    }
    return token;
}
