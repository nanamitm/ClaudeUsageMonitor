#pragma once

#include <QDateTime>
#include <QString>

struct UsageWindow
{
    double utilization = 0.0;
    QDateTime resetsAt;
    bool hasReset = false;
};

struct UsageStatus
{
    UsageWindow fiveHour;
    UsageWindow sevenDay;
    UsageWindow sevenDayOpus;
    bool hasOpus = false;
};

enum class UsageError
{
    MissingCredentials,
    Network,
    RateLimited,
    Parse
};

struct UsageFailure
{
    UsageError error = UsageError::Network;
    QString message;
    int retryAfterSeconds = 0;
};
