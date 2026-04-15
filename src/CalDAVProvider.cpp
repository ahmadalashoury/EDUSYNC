#include "CalDAVProvider.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <QUuid>
#include <QXmlStreamReader>
#include <algorithm>
#include <functional>
#include <memory>

namespace {
constexpr auto kSettingsKey = "accounts/caldav";

QString readHrefWithinProperty(QXmlStreamReader& xml, const QString& propertyName) {
    if (xml.name() != propertyName)
        return {};

    const QString targetName = propertyName;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == u"href")
            return xml.readElementText().trimmed();
        if (xml.isEndElement() && xml.name() == targetName)
            break;
    }
    return {};
}

QString unescapeICSText(QString value) {
    value.replace("\\n", "\n");
    value.replace("\\N", "\n");
    value.replace("\\,", ",");
    value.replace("\\;", ";");
    value.replace("\\\\", "\\");
    return value;
}

QDateTime parseICSDateTime(QString value, bool isEnd) {
    value = value.trimmed();
    if (value.isEmpty())
        return {};

    if (value.size() == 8) {
        const QDate date = QDate::fromString(value, "yyyyMMdd");
        if (!date.isValid())
            return {};
        return QDateTime(date, isEnd ? QTime(23, 59) : QTime(0, 0));
    }

    if (value.endsWith('Z')) {
        const QDateTime utc = QDateTime::fromString(value, "yyyyMMdd'T'HHmmss'Z'");
        return utc.isValid() ? utc.toLocalTime() : QDateTime();
    }

    QDateTime local = QDateTime::fromString(value, "yyyyMMdd'T'HHmmss");
    if (local.isValid())
        return local;

    local = QDateTime::fromString(value, Qt::ISODate);
    return local.isValid() ? local : QDateTime();
}

QVector<QString> unfoldICSLines(const QString& raw) {
    QStringList physical = raw.split(QRegularExpression("\r\n|\n|\r"));
    QVector<QString> result;
    for (const QString& line : physical) {
        if (!result.isEmpty() && (line.startsWith(' ') || line.startsWith('\t'))) {
            result.last().append(line.mid(1));
        } else {
            result.push_back(line);
        }
    }
    return result;
}

struct ParsedCalendarObject {
    QString href;
    QString etag;
    QString calendarData;
};

struct ParsedCollection {
    QString href;
    QString displayName;
};

QVector<ParsedCollection> parseCalendarCollections(const QByteArray& xmlBody) {
    QVector<ParsedCollection> collections;
    QXmlStreamReader xml(xmlBody);

    QString href;
    QString displayName;
    bool isCalendar = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == u"response") {
            href.clear();
            displayName.clear();
            isCalendar = false;
        } else if (xml.isStartElement() && xml.name() == u"href" && href.isEmpty()) {
            href = xml.readElementText().trimmed();
        } else if (xml.isStartElement() && xml.name() == u"displayname") {
            displayName = xml.readElementText().trimmed();
        } else if (xml.isStartElement() && xml.name() == u"calendar") {
            isCalendar = true;
        } else if (xml.isEndElement() && xml.name() == u"response") {
            if (isCalendar && !href.isEmpty())
                collections.push_back({href, displayName});
        }
    }

    return collections;
}

QVector<ParsedCalendarObject> parseCalendarObjects(const QByteArray& xmlBody) {
    QVector<ParsedCalendarObject> objects;
    QXmlStreamReader xml(xmlBody);

    ParsedCalendarObject current;
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == u"response") {
            current = {};
        } else if (xml.isStartElement() && xml.name() == u"href") {
            current.href = xml.readElementText().trimmed();
        } else if (xml.isStartElement() && xml.name() == u"getetag") {
            current.etag = xml.readElementText().trimmed();
        } else if (xml.isStartElement() && xml.name() == u"calendar-data") {
            current.calendarData = xml.readElementText();
        } else if (xml.isEndElement() && xml.name() == u"response") {
            if (!current.calendarData.isEmpty())
                objects.push_back(current);
        }
    }

    return objects;
}

QVector<Event> parseICalEvents(const QString& icalText, const QString& href) {
    QVector<Event> events;
    const QVector<QString> lines = unfoldICSLines(icalText);

    bool inEvent = false;
    QMap<QString, QString> fields;
    auto flush = [&]() {
        if (fields.isEmpty())
            return;

        Event event;
        event.setTitle(unescapeICSText(fields.value("SUMMARY")).trimmed().isEmpty()
                       ? "Untitled event"
                       : unescapeICSText(fields.value("SUMMARY")));
        event.setDescription(unescapeICSText(fields.value("DESCRIPTION")));

        QDateTime start = parseICSDateTime(fields.value("DTSTART"), false);
        QDateTime end = parseICSDateTime(fields.value("DTEND"), true);
        if (!start.isValid())
            return;
        if (!end.isValid() || end <= start)
            end = start.addSecs(3600);

        event.setStartTime(start);
        event.setEndTime(end);
        event.setColor(QColor("#3b82f6"));
        event.setSource(Event::CalDAV);
        event.setExternalId(href.isEmpty() ? fields.value("UID") : href);
        event.setLastModified(parseICSDateTime(fields.value("LAST-MODIFIED"), false));
        events.push_back(event);
    };

    for (const QString& line : lines) {
        if (line == "BEGIN:VEVENT") {
            inEvent = true;
            fields.clear();
            continue;
        }
        if (line == "END:VEVENT") {
            flush();
            inEvent = false;
            fields.clear();
            continue;
        }
        if (!inEvent)
            continue;

        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;

        const QString keyPart = line.left(colon);
        const QString value = line.mid(colon + 1);
        const QString baseKey = keyPart.section(';', 0, 0).trimmed().toUpper();
        fields[baseKey] = value.trimmed();
    }

    return events;
}
}

CalDAVProvider::CalDAVProvider(const AccountConfig& config, QObject* parent)
    : CalendarProvider(parent)
{
    m_nam = new QNetworkAccessManager(this);
    m_accountId = config.id.isEmpty() ? createAccountId() : config.id;
    m_accountName = config.name;
    m_serverUrl = config.serverUrl;
    m_username = config.username;
    m_password = config.password;
    m_calendarHomePath = config.calendarHomePath;
    m_principalUrl = config.principalUrl;
    m_authenticated = config.authenticated;

    setAccountKey(QString("caldav/%1").arg(m_accountId));
    setUserEmail(m_username);
    setStatusDetail(m_authenticated ? "Ready to sync" : "Not connected");

    if (m_authenticated)
        setConnectionState(Connected);
}

bool CalDAVProvider::isAuthenticated() const {
    return m_authenticated && !m_serverUrl.isEmpty() && !m_username.isEmpty();
}

QString CalDAVProvider::accountDisplayName() const {
    return !m_accountName.isEmpty() ? m_accountName : displayName();
}

void CalDAVProvider::setServerUrl(const QString& url) {
    m_serverUrl = url.trimmed();
}

void CalDAVProvider::setUsername(const QString& user) {
    m_username = user.trimmed();
    setUserEmail(m_username);
}

void CalDAVProvider::setPassword(const QString& pass) {
    m_password = pass;
}

QString CalDAVProvider::displayName() const {
    const QString host = QUrl(m_serverUrl).host().toLower();
    if (host.contains("icloud")) return "iCloud Calendar";
    if (host.contains("fastmail")) return "Fastmail";
    if (host.contains("yahoo")) return "Yahoo Calendar";
    if (host.contains("nextcloud")) return "Nextcloud";
    if (!host.isEmpty()) return host;
    return "CalDAV";
}

CalDAVProvider::AccountConfig CalDAVProvider::config() const {
    return AccountConfig{
        .id = m_accountId,
        .name = m_accountName,
        .serverUrl = m_serverUrl,
        .username = m_username,
        .password = m_password,
        .calendarHomePath = m_calendarHomePath,
        .principalUrl = m_principalUrl,
        .authenticated = m_authenticated
    };
}

QList<CalDAVProvider::AccountConfig> CalDAVProvider::loadAccounts() {
    QSettings settings("EduSync", "EduSync");
    const QByteArray raw = settings.value(kSettingsKey).toByteArray();
    if (raw.isEmpty())
        return {};

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray())
        return {};

    QList<AccountConfig> accounts;
    const QJsonArray array = doc.array();
    accounts.reserve(array.size());

    for (const auto& value : array) {
        const QJsonObject obj = value.toObject();
        AccountConfig config;
        config.id = obj.value("id").toString();
        config.name = obj.value("name").toString();
        config.serverUrl = obj.value("server_url").toString();
        config.username = obj.value("username").toString();
        config.password = obj.value("password").toString();
        config.calendarHomePath = obj.value("calendar_home").toString();
        config.principalUrl = obj.value("principal_url").toString();
        config.authenticated = obj.value("authenticated").toBool(false);
        if (!config.serverUrl.isEmpty() && !config.username.isEmpty())
            accounts.push_back(config);
    }

    return accounts;
}

void CalDAVProvider::saveAccounts(const QList<AccountConfig>& accounts) {
    QJsonArray array;
    for (const auto& account : accounts) {
        QJsonObject obj;
        obj["id"] = account.id;
        obj["name"] = account.name;
        obj["server_url"] = account.serverUrl;
        obj["username"] = account.username;
        obj["password"] = account.password;
        obj["calendar_home"] = account.calendarHomePath;
        obj["principal_url"] = account.principalUrl;
        obj["authenticated"] = account.authenticated;
        array.append(obj);
    }

    QSettings("EduSync", "EduSync").setValue(
        kSettingsKey, QJsonDocument(array).toJson(QJsonDocument::Compact));
}

void CalDAVProvider::upsertAccount(const AccountConfig& account) {
    QList<AccountConfig> accounts = loadAccounts();
    bool replaced = false;
    for (auto& existing : accounts) {
        if (existing.id == account.id) {
            existing = account;
            replaced = true;
            break;
        }
    }
    if (!replaced)
        accounts.push_back(account);
    saveAccounts(accounts);
}

void CalDAVProvider::removeAccount(const QString& id) {
    QList<AccountConfig> accounts = loadAccounts();
    accounts.erase(std::remove_if(accounts.begin(), accounts.end(),
                                  [&](const AccountConfig& config) {
                                      return config.id == id;
                                  }),
                   accounts.end());
    saveAccounts(accounts);
}

QString CalDAVProvider::createAccountId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QUrl CalDAVProvider::resolvedUrl(const QString& hrefOrUrl) const {
    const QUrl input(hrefOrUrl);
    if (input.isValid() && !input.scheme().isEmpty())
        return input;
    return QUrl(m_serverUrl).resolved(QUrl(hrefOrUrl));
}

QByteArray CalDAVProvider::basicAuthHeader() const {
    return "Basic " + (m_username + ":" + m_password).toUtf8().toBase64();
}

void CalDAVProvider::persist() const {
    upsertAccount(config());
}

void CalDAVProvider::discoverHomeAndContinue(const std::function<void(bool)>& continuation) {
    const QUrl baseUrl(m_serverUrl);
    if (!baseUrl.isValid()) {
        continuation(false);
        return;
    }

    auto requestProps = std::make_shared<std::function<void(const QUrl&, bool)>>();
    *requestProps = [this, continuation, requestProps](const QUrl& url, bool finalAttempt) {
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", basicAuthHeader());
        request.setRawHeader("Depth", "0");
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml; charset=utf-8");

        const QByteArray body =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
            "  <d:prop>"
            "    <d:current-user-principal/>"
            "    <c:calendar-home-set/>"
            "  </d:prop>"
            "</d:propfind>";

        QNetworkReply* reply = m_nam->sendCustomRequest(request, "PROPFIND", body);
        connect(reply, &QNetworkReply::finished, this, [this, reply, continuation, finalAttempt, requestProps] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray xmlBody = reply->readAll();

            if (reply->error() != QNetworkReply::NoError && status != 207) {
                continuation(false);
                return;
            }

            QXmlStreamReader xml(xmlBody);
            QString principal;
            QString home;
            while (!xml.atEnd()) {
                xml.readNext();
                if (!xml.isStartElement())
                    continue;
                if (xml.name() == u"current-user-principal" && principal.isEmpty())
                    principal = readHrefWithinProperty(xml, "current-user-principal");
                else if (xml.name() == u"calendar-home-set" && home.isEmpty())
                    home = readHrefWithinProperty(xml, "calendar-home-set");
            }

            if (!principal.isEmpty())
                m_principalUrl = principal;
            if (!home.isEmpty())
                m_calendarHomePath = home;

            if (!m_calendarHomePath.isEmpty()) {
                persist();
                continuation(true);
                return;
            }

            if (!finalAttempt && !m_principalUrl.isEmpty()) {
                (*requestProps)(resolvedUrl(m_principalUrl), true);
                return;
            }

            continuation(!m_principalUrl.isEmpty() || !m_calendarHomePath.isEmpty());
        });
    };

    (*requestProps)(baseUrl, false);
}

void CalDAVProvider::authenticate() {
    if (m_serverUrl.isEmpty() || m_username.isEmpty() || m_password.isEmpty()) {
        setError("Server URL, username, and app password are required.");
        setStatusDetail("Credentials incomplete");
        emit authenticationComplete(false, errorMessage());
        return;
    }

    setConnectionState(Connecting);
    setStatusDetail("Testing connection");
    discoverHomeAndContinue([this](bool discovered) {
        Q_UNUSED(discovered)
        testConnection();
    });
}

void CalDAVProvider::testConnection() {
    const QUrl url = resolvedUrl(m_calendarHomePath.isEmpty() ? m_serverUrl : m_calendarHomePath);
    if (!url.isValid()) {
        setError("Invalid CalDAV server URL.");
        setStatusDetail("Invalid server URL");
        emit authenticationComplete(false, errorMessage());
        return;
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", basicAuthHeader());
    request.setRawHeader("Depth", "0");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml; charset=utf-8");

    const QByteArray body =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
        "  <d:prop>"
        "    <d:current-user-principal/>"
        "    <c:calendar-home-set/>"
        "  </d:prop>"
        "</d:propfind>";

    QNetworkReply* reply = m_nam->sendCustomRequest(request, "PROPFIND", body);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status != 207) {
            m_authenticated = false;
            if (status == 401) {
                setError("The username or app password was rejected.");
                setStatusDetail("Credentials rejected");
            } else {
                setError(QString("Connection failed: %1").arg(reply->errorString()));
                setStatusDetail("Connection failed");
            }
            persist();
            emit authenticationComplete(false, errorMessage());
            return;
        }

        const QByteArray xmlBody = reply->readAll();
        QXmlStreamReader xml(xmlBody);
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement())
                continue;
            if (xml.name() == u"current-user-principal" && m_principalUrl.isEmpty())
                m_principalUrl = readHrefWithinProperty(xml, "current-user-principal");
            else if (xml.name() == u"calendar-home-set" && m_calendarHomePath.isEmpty())
                m_calendarHomePath = readHrefWithinProperty(xml, "calendar-home-set");
        }

        m_authenticated = true;
        setUserEmail(m_username);
        setConnectionState(Connected);
        setStatusDetail("Connected");
        persist();
        emit statusMessage(QString("Connected %1").arg(accountDisplayName()));
        emit authenticationComplete(true, QString());
    });
}

void CalDAVProvider::signOut() {
    m_authenticated = false;
    setConnectionState(Disconnected);
    setStatusDetail("Not connected");
    persist();
    emit statusMessage(QString("Disconnected %1").arg(accountDisplayName()));
}

void CalDAVProvider::fetchEvents(const QDate& from, const QDate& to) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Not authenticated");
        return;
    }

    setConnectionState(Syncing);
    setStatusDetail("Fetching events");

    discoverHomeAndContinue([this, from, to](bool discovered) {
        if (!discovered && m_calendarHomePath.isEmpty()) {
            setError("Could not discover CalDAV calendar home.");
            emit operationComplete(false, errorMessage());
            return;
        }

        QUrl collectionRoot = resolvedUrl(m_calendarHomePath.isEmpty() ? m_serverUrl : m_calendarHomePath);
        if (!collectionRoot.isValid()) {
            setError("Invalid CalDAV calendar home.");
            emit operationComplete(false, errorMessage());
            return;
        }

        QNetworkRequest request(collectionRoot);
        request.setRawHeader("Authorization", basicAuthHeader());
        request.setRawHeader("Depth", "1");
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml; charset=utf-8");

        const QByteArray body =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<d:propfind xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
            "  <d:prop>"
            "    <d:displayname/>"
            "    <d:resourcetype/>"
            "  </d:prop>"
            "</d:propfind>";

        QNetworkReply* reply = m_nam->sendCustomRequest(request, "PROPFIND", body);
        connect(reply, &QNetworkReply::finished, this, [this, reply, collectionRoot, from, to] {
            reply->deleteLater();

            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (reply->error() != QNetworkReply::NoError && status != 207) {
                setError(QString("Failed to list CalDAV calendars: %1").arg(reply->errorString()));
                emit operationComplete(false, errorMessage());
                return;
            }

            QVector<ParsedCollection> collections = parseCalendarCollections(reply->readAll());
            if (collections.isEmpty()) {
                collections.push_back({collectionRoot.path(), displayName()});
            }

            auto* pending = new int(collections.size());
            auto* aggregate = new QVector<Event>;
            auto* failure = new QString;

            auto finishOne = [this, pending, aggregate, failure]() {
                --(*pending);
                if (*pending > 0)
                    return;

                const QVector<Event> events = *aggregate;
                const QString error = *failure;
                delete pending;
                delete aggregate;
                delete failure;

                if (!error.isEmpty()) {
                    setError(error);
                    emit operationComplete(false, error);
                    return;
                }

                setConnectionState(Connected);
                setLastSyncTime(QDateTime::currentDateTime());
                setStatusDetail(QString("Last synced %1").arg(lastSyncTime().toString("h:mm AP")));
                emit eventsFetched(events);
            };

            const QString startStamp = from.startOfDay().toUTC().toString("yyyyMMdd'T'HHmmss'Z'");
            const QString endStamp = to.addDays(1).startOfDay().toUTC().toString("yyyyMMdd'T'HHmmss'Z'");

            for (const ParsedCollection& collection : collections) {
                QNetworkRequest report(resolvedUrl(collection.href));
                report.setRawHeader("Authorization", basicAuthHeader());
                report.setRawHeader("Depth", "1");
                report.setHeader(QNetworkRequest::ContentTypeHeader, "application/xml; charset=utf-8");

                const QByteArray reportBody = QString(
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                    "<c:calendar-query xmlns:d=\"DAV:\" xmlns:c=\"urn:ietf:params:xml:ns:caldav\">"
                    "  <d:prop><d:getetag/><c:calendar-data/></d:prop>"
                    "  <c:filter>"
                    "    <c:comp-filter name=\"VCALENDAR\">"
                    "      <c:comp-filter name=\"VEVENT\">"
                    "        <c:time-range start=\"%1\" end=\"%2\"/>"
                    "      </c:comp-filter>"
                    "    </c:comp-filter>"
                    "  </c:filter>"
                    "</c:calendar-query>")
                    .arg(startStamp, endStamp)
                    .toUtf8();

                QNetworkReply* reportReply = m_nam->sendCustomRequest(report, "REPORT", reportBody);
                connect(reportReply, &QNetworkReply::finished, this, [this, reportReply, aggregate, failure, finishOne] {
                    reportReply->deleteLater();

                    const int reportStatus = reportReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if (reportReply->error() != QNetworkReply::NoError && reportStatus != 207) {
                        if (failure->isEmpty())
                            *failure = QString("CalDAV sync failed: %1").arg(reportReply->errorString());
                        finishOne();
                        return;
                    }

                    const QVector<ParsedCalendarObject> objects = parseCalendarObjects(reportReply->readAll());
                    for (const ParsedCalendarObject& object : objects) {
                        QVector<Event> parsed = parseICalEvents(object.calendarData, object.href);
                        for (Event& event : parsed)
                            aggregate->push_back(event);
                    }

                    finishOne();
                });
            }
        });
    });
}

void CalDAVProvider::createEvent(const Event& event) {
    Q_UNUSED(event)
    emit operationComplete(false, "Creating events on CalDAV servers isn't available in this release. Changes stay on this Mac.");
}

void CalDAVProvider::updateEvent(const QString& externalId, const Event& event) {
    Q_UNUSED(externalId)
    Q_UNUSED(event)
    emit operationComplete(false, "Editing CalDAV events isn't available in this release. Changes stay on this Mac.");
}

void CalDAVProvider::deleteEvent(const QString& externalId) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Not authenticated");
        return;
    }

    const QUrl targetUrl = resolvedUrl(externalId);
    if (!targetUrl.isValid()) {
        emit operationComplete(false, "Invalid CalDAV event identifier.");
        return;
    }

    QNetworkRequest request(targetUrl);
    request.setRawHeader("Authorization", basicAuthHeader());
    QNetworkReply* reply = m_nam->sendCustomRequest(request, "DELETE");
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status != 200 && status != 204) {
            emit operationComplete(false, QString("CalDAV delete failed: %1").arg(reply->errorString()));
            return;
        }
        emit operationComplete(true, QString());
    });
}
