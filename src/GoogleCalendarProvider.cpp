#include "GoogleCalendarProvider.h"
#include "Credentials.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QTimer>

// ============================================================================
// Construction
// ============================================================================

GoogleCalendarProvider::GoogleCalendarProvider(QObject* parent)
    : CalendarProvider(parent)
{
    m_nam = new QNetworkAccessManager(this);
    setAccountKey("google/default");
    setStatusDetail("Ready to connect");

    // Auth timeout — if user doesn't complete within 2 minutes, cancel
    m_authTimeout = new QTimer(this);
    m_authTimeout->setSingleShot(true);
    m_authTimeout->setInterval(120000);
    connect(m_authTimeout, &QTimer::timeout, this, [this] {
        if (connectionState() == Connecting) {
            cleanupAuthServer();
            setError("Authentication timed out. Please try again.");
            emit authenticationComplete(false, errorMessage());
        }
    });

    loadTokens();

    // Set initial state based on stored tokens
    if (isAuthenticated())
        setConnectionState(Connected);
}

// ============================================================================
// Token storage
// ============================================================================

void GoogleCalendarProvider::loadTokens() {
    QSettings s("EduSync", "EduSync");
    m_accessToken  = s.value("google/access_token").toString();
    m_refreshToken = s.value("google/refresh_token").toString();
    m_tokenExpiry  = QDateTime::fromString(s.value("google/token_expiry").toString(), Qt::ISODate);
    setUserEmail(s.value("google/user_email").toString());
}

bool GoogleCalendarProvider::isAuthenticated() const {
    return !m_refreshToken.isEmpty();
}

void GoogleCalendarProvider::signOut() {
    m_accessToken.clear();
    m_refreshToken.clear();
    m_tokenExpiry = QDateTime();
    setUserEmail(QString());

    QSettings s("EduSync", "EduSync");
    s.remove("google/access_token");
    s.remove("google/refresh_token");
    s.remove("google/token_expiry");
    s.remove("google/user_email");

    setConnectionState(Disconnected);
    setStatusDetail("Not connected");
    emit statusMessage("Signed out of Google Calendar");
}

// ============================================================================
// OAuth2 authentication flow
// ============================================================================

void GoogleCalendarProvider::authenticate() {
    // If already authenticating, don't start another flow
    if (connectionState() == Connecting) return;

    // If we have a refresh token, just refresh
    if (!m_refreshToken.isEmpty()) {
        refreshAccessToken();
        return;
    }

    setConnectionState(Connecting);
    setStatusDetail("Opening browser");

    // Start local HTTP server for OAuth redirect
    cleanupAuthServer();
    m_authServer = new QTcpServer(this);

    if (!m_authServer->listen(QHostAddress::LocalHost, 0)) {
        setError("Failed to start local auth server");
        emit authenticationComplete(false, errorMessage());
        return;
    }
    m_redirectPort = m_authServer->serverPort();
    connect(m_authServer, &QTcpServer::newConnection,
            this, &GoogleCalendarProvider::onOAuthConnection);

    // Build authorization URL
    QUrl authUrl(kAuthUrl);
    QUrlQuery query;
    query.addQueryItem("client_id",     Credentials::googleClientId());
    query.addQueryItem("redirect_uri",  QString("http://localhost:%1/callback").arg(m_redirectPort));
    query.addQueryItem("response_type", "code");
    query.addQueryItem("scope",         kScope);
    query.addQueryItem("access_type",   "offline");
    query.addQueryItem("prompt",        "consent");
    authUrl.setQuery(query);

    emit statusMessage("Opening browser for Google sign-in...");
    QDesktopServices::openUrl(authUrl);
    setStatusDetail("Waiting for Google sign-in");
    m_authTimeout->start();
}

void GoogleCalendarProvider::cleanupAuthServer() {
    if (m_authServer) {
        m_authServer->close();
        m_authServer->deleteLater();
        m_authServer = nullptr;
    }
    m_authTimeout->stop();
}

void GoogleCalendarProvider::onOAuthConnection() {
    QTcpSocket* socket = m_authServer->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
        const QByteArray data = socket->readAll();
        const QString request = QString::fromUtf8(data);

        // Parse the GET request line for the auth code
        // Format: GET /callback?code=AUTH_CODE&scope=... HTTP/1.1
        QString authCode;
        const int queryStart = request.indexOf('?');
        const int httpEnd = request.indexOf(" HTTP/");
        if (queryStart >= 0 && httpEnd > queryStart) {
            const QString queryString = request.mid(queryStart + 1, httpEnd - queryStart - 1);
            QUrlQuery urlQuery(queryString);
            authCode = urlQuery.queryItemValue("code", QUrl::FullyDecoded);
        }

        if (authCode.isEmpty()) {
            // Check for error parameter
            QString error;
            if (queryStart >= 0 && httpEnd > queryStart) {
                const QString queryString = request.mid(queryStart + 1, httpEnd - queryStart - 1);
                QUrlQuery urlQuery(queryString);
                error = urlQuery.queryItemValue("error", QUrl::FullyDecoded);
            }

            const QString html =
                "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                "<html><body style='font-family:-apple-system,sans-serif;"
                "display:flex;justify-content:center;align-items:center;"
                "height:100vh;margin:0;background:#0f1115;color:#e6eaf0;'>"
                "<div style='text-align:center;'>"
                "<h1 style='color:#ef4444;'>Authentication Failed</h1>"
                "<p>Could not connect to Google Calendar.</p>"
                "<p style='color:#9ca3af;'>You can close this tab.</p>"
                "</div></body></html>";
            socket->write(html.toUtf8());
            socket->flush();
            socket->disconnectFromHost();
            socket->deleteLater();
            cleanupAuthServer();

            const QString msg = (error == "access_denied")
                ? "You cancelled the sign-in."
                : "No authorization code received.";
            setError(msg);
            emit authenticationComplete(false, msg);
            return;
        }

        // Send success response to browser
        const QString html =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
            "<html><body style='font-family:-apple-system,sans-serif;"
            "display:flex;justify-content:center;align-items:center;"
            "height:100vh;margin:0;background:#0f1115;color:#e6eaf0;'>"
            "<div style='text-align:center;'>"
            "<h1 style='color:#22c55e;'>Connected to Google Calendar</h1>"
            "<p>You can close this tab and return to EduSync.</p>"
            "</div></body></html>";
        socket->write(html.toUtf8());
        socket->flush();
        socket->disconnectFromHost();
        socket->deleteLater();
        cleanupAuthServer();

        // Exchange code for tokens
        exchangeCodeForTokens(authCode);
    });
}

void GoogleCalendarProvider::exchangeCodeForTokens(const QString& authCode) {
    QUrl tokenUrl(kTokenUrl);
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("code",          authCode);
    params.addQueryItem("client_id",     Credentials::googleClientId());
    params.addQueryItem("client_secret", Credentials::googleClientSecret());
    params.addQueryItem("redirect_uri",  QString("http://localhost:%1/callback").arg(m_redirectPort));
    params.addQueryItem("grant_type",    "authorization_code");

    QNetworkReply* reply = m_nam->post(request, params.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            setError(QString("Network error: %1").arg(reply->errorString()));
            emit authenticationComplete(false, errorMessage());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject obj = doc.object();

        if (obj.contains("error")) {
            setError(obj.value("error_description").toString("Token exchange failed"));
            emit authenticationComplete(false, errorMessage());
            return;
        }

        m_accessToken  = obj.value("access_token").toString();
        m_refreshToken = obj.value("refresh_token").toString();
        const int expiresIn = obj.value("expires_in").toInt(3600);
        m_tokenExpiry = QDateTime::currentDateTimeUtc().addSecs(expiresIn - 60);

        // Save tokens
        QSettings s("EduSync", "EduSync");
        s.setValue("google/access_token",  m_accessToken);
        s.setValue("google/refresh_token", m_refreshToken);
        s.setValue("google/token_expiry",  m_tokenExpiry.toString(Qt::ISODate));

        // Fetch user email, then emit success
        setStatusDetail("Finalizing connection");
        fetchUserEmail();
    });
}

void GoogleCalendarProvider::fetchUserEmail() {
    QUrl userInfoUrl(kUserInfoUrl);
    QNetworkRequest req(userInfoUrl);
    req.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString email = obj.value("email").toString();
        setUserEmail(email);

        if (!email.isEmpty())
            QSettings("EduSync", "EduSync").setValue("google/user_email", email);

        setConnectionState(Connected);
        setStatusDetail(email.isEmpty() ? "Connected" : email);
        emit statusMessage(QString("Connected to Google Calendar (%1)").arg(
            email.isEmpty() ? "authorized" : email));
        emit authenticationComplete(true, QString());
    });
}

void GoogleCalendarProvider::refreshAccessToken() {
    if (m_refreshToken.isEmpty()) {
        setError("No refresh token. Please sign in again.");
        emit authenticationComplete(false, errorMessage());
        return;
    }

    QUrl tokenUrl(kTokenUrl);
    QNetworkRequest request(tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery params;
    params.addQueryItem("refresh_token", m_refreshToken);
    params.addQueryItem("client_id",     Credentials::googleClientId());
    params.addQueryItem("client_secret", Credentials::googleClientSecret());
    params.addQueryItem("grant_type",    "refresh_token");

    QNetworkReply* reply = m_nam->post(request, params.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            setError(QString("Network error: %1").arg(reply->errorString()));
            emit authenticationComplete(false, errorMessage());
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject obj = doc.object();

        if (obj.contains("error")) {
            // Refresh token revoked — need full re-auth
            m_refreshToken.clear();
            QSettings("EduSync", "EduSync").remove("google/refresh_token");
            setError("Session expired. Please sign in again.");
            setStatusDetail("Action needed");
            emit authenticationComplete(false, errorMessage());
            return;
        }

        m_accessToken = obj.value("access_token").toString();
        const int expiresIn = obj.value("expires_in").toInt(3600);
        m_tokenExpiry = QDateTime::currentDateTimeUtc().addSecs(expiresIn - 60);

        // A new refresh token may be issued
        if (obj.contains("refresh_token"))
            m_refreshToken = obj.value("refresh_token").toString();

        QSettings s("EduSync", "EduSync");
        s.setValue("google/access_token",  m_accessToken);
        s.setValue("google/refresh_token", m_refreshToken);
        s.setValue("google/token_expiry",  m_tokenExpiry.toString(Qt::ISODate));

        setConnectionState(Connected);
        setStatusDetail(userEmail().isEmpty() ? "Connected" : userEmail());
        emit authenticationComplete(true, QString());
    });
}

// ============================================================================
// Authenticated request helper — auto-refreshes expired tokens
// ============================================================================

void GoogleCalendarProvider::makeAuthenticatedRequest(
    const QString& method,
    const QUrl& url,
    const QByteArray& body,
    std::function<void(const QJsonDocument&, int)> callback)
{
    // Check if token needs refresh
    if (m_tokenExpiry.isValid() && QDateTime::currentDateTimeUtc() >= m_tokenExpiry
        && !m_refreshToken.isEmpty())
    {
        QUrl savedUrl = url;
        QByteArray savedBody = body;
        QString savedMethod = method;

        QMetaObject::Connection* conn = new QMetaObject::Connection;
        *conn = connect(this, &CalendarProvider::authenticationComplete, this,
            [this, conn, savedMethod, savedUrl, savedBody, callback](bool ok, const QString&) {
                disconnect(*conn);
                delete conn;
                if (ok) makeAuthenticatedRequest(savedMethod, savedUrl, savedBody, callback);
                else    callback(QJsonDocument(), 401);
            });
        refreshAccessToken();
        return;
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", ("Bearer " + m_accessToken).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = nullptr;
    if (method == "GET")         reply = m_nam->get(request);
    else if (method == "POST")   reply = m_nam->post(request, body);
    else if (method == "PATCH")  reply = m_nam->sendCustomRequest(request, "PATCH", body);
    else if (method == "PUT")    reply = m_nam->put(request, body);
    else if (method == "DELETE") reply = m_nam->deleteResource(request);

    if (!reply) {
        callback(QJsonDocument(), 0);
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply, callback, method, url, body] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();

        // Auto-retry on 401 (token expired mid-flight)
        if (status == 401 && !m_refreshToken.isEmpty()) {
            QMetaObject::Connection* conn = new QMetaObject::Connection;
            *conn = connect(this, &CalendarProvider::authenticationComplete, this,
                [this, conn, method, url, body, callback](bool ok, const QString&) {
                    disconnect(*conn);
                    delete conn;
                    if (ok) makeAuthenticatedRequest(method, url, body, callback);
                    else    callback(QJsonDocument(), 401);
                });
            refreshAccessToken();
            return;
        }

        callback(QJsonDocument::fromJson(data), status);
    });
}

// ============================================================================
// Fetch events
// ============================================================================

void GoogleCalendarProvider::fetchEvents(const QDate& from, const QDate& to) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Not authenticated");
        return;
    }

    setConnectionState(Syncing);
    setStatusDetail("Syncing now");

    QUrl url(QString("%1/calendars/primary/events").arg(kCalendarApi));
    QUrlQuery query;
    query.addQueryItem("timeMin",       QDateTime(from, QTime(0,0)).toUTC().toString(Qt::ISODate));
    query.addQueryItem("timeMax",       QDateTime(to, QTime(23,59,59)).toUTC().toString(Qt::ISODate));
    query.addQueryItem("singleEvents",  "true");
    query.addQueryItem("orderBy",       "startTime");
    query.addQueryItem("maxResults",    "250");
    url.setQuery(query);

    emit statusMessage("Fetching Google Calendar events...");

    makeAuthenticatedRequest("GET", url, {}, [this](const QJsonDocument& doc, int status) {
        if (status < 200 || status >= 300) {
            setError(QString("Fetch failed (HTTP %1)").arg(status));
            emit operationComplete(false, errorMessage());
            return;
        }

        const QJsonArray items = doc.object().value("items").toArray();
        QVector<Event> events;
        events.reserve(items.size());

        for (const QJsonValue& val : items) {
            const QJsonObject obj = val.toObject();
            if (obj.value("status").toString() == "cancelled") continue;
            Event event = Event::fromGoogleJson(obj);
            event.setProviderKey(accountKey());
            events.push_back(event);
        }

        setConnectionState(Connected);
        setLastSyncTime(QDateTime::currentDateTime());
        setStatusDetail(QString("Last synced %1").arg(lastSyncTime().toString("h:mm AP")));
        emit statusMessage(QString("Fetched %1 events from Google Calendar").arg(events.size()));
        emit eventsFetched(events);
    });
}

// ============================================================================
// Create event
// ============================================================================

void GoogleCalendarProvider::createEvent(const Event& event) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Not authenticated");
        return;
    }

    QUrl url(QString("%1/calendars/primary/events").arg(kCalendarApi));
    const QByteArray body = QJsonDocument(event.toGoogleJson()).toJson(QJsonDocument::Compact);

    makeAuthenticatedRequest("POST", url, body, [this](const QJsonDocument& doc, int status) {
        if (status >= 200 && status < 300) {
            const QString newId = doc.object().value("id").toString();
            emit eventCreated(newId);
            emit operationComplete(true, QString());
        } else {
            emit operationComplete(false, QString("Create failed (HTTP %1)").arg(status));
        }
    });
}

// ============================================================================
// Update event
// ============================================================================

void GoogleCalendarProvider::updateEvent(const QString& externalId, const Event& event) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Not authenticated");
        return;
    }

    QUrl url(QString("%1/calendars/primary/events/%2").arg(kCalendarApi, externalId));
    const QByteArray body = QJsonDocument(event.toGoogleJson()).toJson(QJsonDocument::Compact);

    makeAuthenticatedRequest("PATCH", url, body, [this](const QJsonDocument&, int status) {
        if (status >= 200 && status < 300)
            emit operationComplete(true, QString());
        else
            emit operationComplete(false, QString("Update failed (HTTP %1)").arg(status));
    });
}

// ============================================================================
// Delete event
// ============================================================================

void GoogleCalendarProvider::deleteEvent(const QString& externalId) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Not authenticated");
        return;
    }

    QUrl url(QString("%1/calendars/primary/events/%2").arg(kCalendarApi, externalId));

    makeAuthenticatedRequest("DELETE", url, {}, [this](const QJsonDocument&, int status) {
        if (status >= 200 && status < 300)
            emit operationComplete(true, QString());
        else
            emit operationComplete(false, QString("Delete failed (HTTP %1)").arg(status));
    });
}
