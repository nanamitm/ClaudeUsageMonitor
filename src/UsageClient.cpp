#include "UsageClient.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QThread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <winhttp.h>
#elif defined(Q_OS_LINUX)
#include <curl/curl.h>
#endif

namespace
{
#ifdef Q_OS_WIN
constexpr auto ApiHost = L"api.anthropic.com";
constexpr auto ApiPath = L"/api/oauth/usage";
#endif
constexpr auto ApiUrl = "https://api.anthropic.com/api/oauth/usage";
constexpr auto UserAgent = "claude-code/2.0.31";

UsageFailure makeFailure(UsageError error, const QString &message, int retryAfter = 0)
{
    UsageFailure failure;
    failure.error = error;
    failure.message = message;
    failure.retryAfterSeconds = retryAfter;
    return failure;
}

#ifdef Q_OS_LINUX
size_t writeBody(char *contents, size_t size, size_t nmemb, void *userData)
{
    auto *body = static_cast<QByteArray *>(userData);
    const size_t bytes = size * nmemb;
    body->append(contents, static_cast<qsizetype>(bytes));
    return bytes;
}

size_t writeHeader(char *contents, size_t size, size_t nmemb, void *userData)
{
    auto *retryAfter = static_cast<int *>(userData);
    const size_t bytes = size * nmemb;
    const QByteArray header(contents, static_cast<qsizetype>(bytes));
    const QByteArray prefix = "retry-after:";
    if (header.left(prefix.size()).toLower() == prefix) {
        bool ok = false;
        const int value = QString::fromLatin1(header.mid(prefix.size()).trimmed()).toInt(&ok);
        if (ok) {
            *retryAfter = value;
        }
    }
    return bytes;
}
#endif
}

UsageClient::UsageClient(QObject *parent)
    : QObject(parent)
{
}

void UsageClient::refresh()
{
    QString credentialError;
    const QString token = m_credentials.accessToken(&credentialError);
    if (token.isEmpty()) {
        emit usageFailed(makeFailure(UsageError::MissingCredentials, credentialError));
        return;
    }

    QThread *thread = QThread::create([this, token] {
        int statusCode = 0;
        int retryAfter = 0;
        QString errorMessage;
        const QByteArray body = fetchUsage(token, &statusCode, &retryAfter, &errorMessage);
        QMetaObject::invokeMethod(this, [this, statusCode, retryAfter, body, errorMessage] {
            handleResponse(statusCode, retryAfter, body, errorMessage);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

UsageWindow UsageClient::parseWindow(const QJsonObject &object)
{
    UsageWindow window;
    window.utilization = object.value("utilization").toDouble(0.0);

    const QString resetValue = object.value("resets_at").toString();
    if (!resetValue.isEmpty()) {
        window.resetsAt = QDateTime::fromString(resetValue, Qt::ISODateWithMs);
        if (!window.resetsAt.isValid()) {
            window.resetsAt = QDateTime::fromString(resetValue, Qt::ISODate);
        }
        window.hasReset = window.resetsAt.isValid();
    }

    return window;
}

QByteArray UsageClient::fetchUsage(const QString &token, int *statusCode, int *retryAfterSeconds, QString *errorMessage)
{
#ifndef Q_OS_WIN
#ifndef Q_OS_LINUX
    Q_UNUSED(token)
    if (statusCode) {
        *statusCode = 0;
    }
    if (retryAfterSeconds) {
        *retryAfterSeconds = 0;
    }
    if (errorMessage) {
        *errorMessage = "This build currently supports Windows WinHTTP only.";
    }
    return {};
#else
    QByteArray result;
    if (statusCode) {
        *statusCode = 0;
    }
    if (retryAfterSeconds) {
        *retryAfterSeconds = 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        if (errorMessage) {
            *errorMessage = "libcurl could not be initialized.";
        }
        return {};
    }

    struct curl_slist *headers = nullptr;
    const QByteArray authHeader = "Authorization: Bearer " + token.toUtf8();
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, authHeader.constData());
    headers = curl_slist_append(headers, "anthropic-beta: oauth-2025-04-20");

    curl_easy_setopt(curl, CURLOPT_URL, ApiUrl);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, UserAgent);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeHeader);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, retryAfterSeconds);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);

    const CURLcode code = curl_easy_perform(curl);
    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);
    if (statusCode) {
        *statusCode = static_cast<int>(httpStatus);
    }

    if (code != CURLE_OK && errorMessage) {
        *errorMessage = QString::fromUtf8(curl_easy_strerror(code));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
#endif
#else
    QByteArray result;
    if (statusCode) {
        *statusCode = 0;
    }
    if (retryAfterSeconds) {
        *retryAfterSeconds = 0;
    }

    const std::wstring agent = QString::fromLatin1(UserAgent).toStdWString();
    HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        if (errorMessage) {
            *errorMessage = "WinHTTP session could not be opened.";
        }
        return {};
    }

    HINTERNET connect = WinHttpConnect(session, ApiHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!connect) {
        if (errorMessage) {
            *errorMessage = "Connection to Claude usage API failed.";
        }
        WinHttpCloseHandle(session);
        return {};
    }

    HINTERNET request = WinHttpOpenRequest(connect, L"GET", ApiPath, nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) {
        if (errorMessage) {
            *errorMessage = "Usage API request could not be created.";
        }
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return {};
    }

    const QString headerText =
        QStringLiteral("Accept: application/json\r\n"
                       "Content-Type: application/json\r\n"
                       "Authorization: Bearer %1\r\n"
                       "anthropic-beta: oauth-2025-04-20\r\n")
            .arg(token);
    const std::wstring headers = headerText.toStdWString();

    BOOL ok = WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(headers.size()),
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) {
        ok = WinHttpReceiveResponse(request, nullptr);
    }
    if (!ok) {
        if (errorMessage) {
            *errorMessage = "Usage API request failed.";
        }
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return {};
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    if (statusCode) {
        *statusCode = static_cast<int>(status);
    }

    wchar_t retryBuffer[32] = {};
    DWORD retrySize = sizeof(retryBuffer);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CUSTOM, L"retry-after",
                            retryBuffer, &retrySize, WINHTTP_NO_HEADER_INDEX)) {
        bool retryOk = false;
        const int retry = QString::fromWCharArray(retryBuffer).toInt(&retryOk);
        if (retryOk && retryAfterSeconds) {
            *retryAfterSeconds = retry;
        }
    }

    DWORD available = 0;
    while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
        QByteArray chunk;
        chunk.resize(static_cast<int>(available));
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) {
            break;
        }
        chunk.resize(static_cast<int>(read));
        result += chunk;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return result;
#endif
}

void UsageClient::handleResponse(int statusCode, int retryAfterSeconds, const QByteArray &body, const QString &errorMessage)
{
    if (statusCode == 429) {
        emit usageFailed(makeFailure(UsageError::RateLimited, "Claude usage API rate limit reached.", retryAfterSeconds));
        return;
    }

    if (statusCode < 200 || statusCode >= 300) {
        const QString message = errorMessage.isEmpty()
            ? QString("Usage API returned HTTP %1.").arg(statusCode)
            : errorMessage;
        emit usageFailed(makeFailure(UsageError::Network, message));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit usageFailed(makeFailure(UsageError::Parse, QString("Usage response could not be parsed: %1.").arg(parseError.errorString())));
        return;
    }

    const QJsonObject root = document.object();
    UsageStatus status;
    status.fiveHour = parseWindow(root.value("five_hour").toObject());
    status.sevenDay = parseWindow(root.value("seven_day").toObject());
    status.sevenDayOpus = parseWindow(root.value("seven_day_opus").toObject());
    status.hasOpus = root.contains("seven_day_opus") && status.sevenDayOpus.utilization > 0.0;

    emit usageReady(status);
}
