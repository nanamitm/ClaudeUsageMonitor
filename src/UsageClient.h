#pragma once

#include "CredentialProvider.h"
#include "UsageModel.h"

#include <QObject>

class QJsonObject;
class UsageClient : public QObject
{
    Q_OBJECT

public:
    explicit UsageClient(QObject *parent = nullptr);

public slots:
    void refresh();

signals:
    void usageReady(const UsageStatus &status);
    void usageFailed(const UsageFailure &failure);

private:
    static UsageWindow parseWindow(const QJsonObject &object);
    static QByteArray fetchUsage(const QString &token, int *statusCode, int *retryAfterSeconds, QString *errorMessage);
    void handleResponse(int statusCode, int retryAfterSeconds, const QByteArray &body, const QString &errorMessage);

    CredentialProvider m_credentials;
};
