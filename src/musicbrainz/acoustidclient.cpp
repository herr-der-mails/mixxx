/*****************************************************************************
 *  Copyright © 2012 John Maguire <john.maguire@gmail.com>                   *
 *                   David Sansome <me@davidsansome.com>                     *
 *  This work is free. You can redistribute it and/or modify it under the    *
 *  terms of the Do What The Fuck You Want To Public License, Version 2,     *
 *  as published by Sam Hocevar.                                             *
 *  See http://www.wtfpl.net/ for more details.                              *
 *****************************************************************************/

#include <QNetworkReply>
#include <QJsonDocument>
#include <QUrl>
#include <QUrlQuery>
#include <QtDebug>

#include "musicbrainz/acoustidclient.h"
#include "musicbrainz/gzip.h"
#include "musicbrainz/network.h"

#include "util/logger.h"

namespace {

mixxx::Logger kLogger("AcoustidClient");

// see API-KEY site here http://acoustid.org/application/496
// I registered the KEY for version 1.12 -- kain88 (may 2013)
// See also: https://acoustid.org/webservice
const QString CLIENT_APIKEY = "czKxnkyO";
const QString ACOUSTID_URL = "https://api.acoustid.org/v2/lookup";
constexpr int kDefaultTimeout = 5000; // msec

} // anonymous namespace

AcoustidClient::AcoustidClient(QObject* parent)
              : QObject(parent),
                m_network(this),
                m_timeouts(kDefaultTimeout, this) {
}

void AcoustidClient::setTimeout(int msec) {
    VERIFY_OR_DEBUG_ASSERT(msec > 0) {
        kLogger.warning() << "Invalid timeout" << msec << "[ms]";
        return;
    }
    m_timeouts.setTimeout(msec);
}

void AcoustidClient::start(int id, const QString& fingerprint, int duration) {
    QUrlQuery urlQuery;
    // The default format is JSON
    //urlQuery.addQueryItem("format", "json");
    urlQuery.addQueryItem("client", CLIENT_APIKEY);
    urlQuery.addQueryItem("duration", QString::number(duration));
    urlQuery.addQueryItem("meta", "recordingids");
    urlQuery.addQueryItem("fingerprint", fingerprint);
    // application/x-www-form-urlencoded request bodies must be percent encoded.
    QByteArray body = urlQuery.query(QUrl::FullyEncoded).toLatin1();

    QUrl url(ACOUSTID_URL);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    req.setRawHeader("Content-Encoding", "gzip");

    kLogger.debug()
            << "POST request:" << ACOUSTID_URL
            << "body:" << body;

    QNetworkReply* reply = m_network.post(req, gzipCompress(body));
    m_pendingReplies[reply] = id;
    connect(reply, &QNetworkReply::finished, this, &AcoustidClient::onReplyFinished);
    m_timeouts.addReply(reply);
}

void AcoustidClient::cancel(int id) {
    QNetworkReply* reply = m_pendingReplies.key(id);
    if (!reply) {
        return;
    }
    cancelPendingReply(reply);
}

void AcoustidClient::cancelPendingReply(QNetworkReply* reply) {
    DEBUG_ASSERT(reply);
    m_timeouts.removeReply(reply);
    m_pendingReplies.remove(reply);
    if (reply->isRunning()) {
        reply->abort();
    }
}

void AcoustidClient::cancelAll() {
    while (!m_pendingReplies.isEmpty()) {
        QNetworkReply* reply = m_pendingReplies.firstKey();
        cancelPendingReply(reply);
        DEBUG_ASSERT(!m_pendingReplies.contains(reply));
    }
}

void AcoustidClient::onReplyFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    VERIFY_OR_DEBUG_ASSERT(reply) {
        return;
    }
    reply->deleteLater();

    if (!m_pendingReplies.contains(reply)) {
        // Already Cancelled
        return;
    }

    m_timeouts.removeReply(reply);
    int id = m_pendingReplies.take(reply);

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    kLogger.debug()
            << "POST reply status:" << statusCode
            << "body:" << body;

    const auto jsonResponse = QJsonDocument::fromJson(body);
    QString statusText;
    if (jsonResponse.isObject()) {
        statusText = jsonResponse.object().value("status").toString();
    }

    if (statusCode != 200 || statusText != "ok") {
        QString errorMessage;
        int errorCode = 0;
        if (jsonResponse.isObject() && statusText == "error") {
            const auto error = jsonResponse.object().value("error").toObject();
            errorMessage = error.value("message").toString();
            errorCode = error.value("message").toInt(errorCode);
        }
        emit networkError(
             statusCode,
             "AcoustID",
             errorMessage,
             errorCode);
        return;
    }

    QStringList recordingIds;
    DEBUG_ASSERT(jsonResponse.isObject());
    DEBUG_ASSERT(jsonResponse.object().value(QStringLiteral("results")).isArray());
    const QJsonArray results = jsonResponse.object().value(QStringLiteral("results")).toArray();
    // Results are ordered by score (descending), only take the recordings
    // from the first result with maximum score.
    for (const auto result : results) {
        DEBUG_ASSERT(result.isObject());
        const auto recordingsValue = result.toObject().value(QStringLiteral("recordings"));
        if (recordingsValue.isUndefined()) {
            kLogger.info()
                    << "No recording(s) found";
            continue;
        } else {
            DEBUG_ASSERT(recordingsValue.isArray());
            const QJsonArray recordings = recordingsValue.toArray();
            DEBUG_ASSERT(recordingIds.isEmpty());
            for (const auto recording : recordings) {
                const auto recordingId =
                        recording.toObject().value(QStringLiteral("id")).toString();
                DEBUG_ASSERT(!recordingId.isEmpty());
                recordingIds.append(recordingId);
            }
            if (!recordingIds.isEmpty()) {
                // Only collect the recordings from the first result
                // with maximum score
                break;
            }
        }
    }
    emit finished(id, recordingIds);
}
