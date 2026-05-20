#pragma once

#include <QString>

class CredentialProvider
{
public:
    QString accessToken(QString *errorMessage = nullptr) const;
    QString credentialsPath() const;
};
