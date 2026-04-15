#include "Event.h"
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimeZone>

Event::Event() = default;

Event::Event(const QString& title,
             const QString& description,
             const QDateTime& start,
             const QDateTime& end,
             const QColor& color,
             const QString& seriesId,
             int id)
    : m_id(id)
    , m_title(title)
    , m_description(description)
    , m_startTime(start)
    , m_endTime(end)
    , m_color(color)
    , m_seriesId(seriesId)
    , m_lastModified(QDateTime::currentDateTimeUtc())
{
}

// ============================================================================
// Local JSON serialization
// ============================================================================

QJsonObject Event::toJson() const
{
    QJsonObject o;
    o["id"]           = m_id;
    o["title"]        = m_title;
    o["desc"]         = m_description;
    o["start"]        = m_startTime.toString(Qt::ISODate);
    o["end"]          = m_endTime.toString(Qt::ISODate);
    o["color_r"]      = m_color.red();
    o["color_g"]      = m_color.green();
    o["color_b"]      = m_color.blue();
    o["series_id"]    = m_seriesId;
    o["source"]       = static_cast<int>(m_source);
    o["external_id"]  = m_externalId;
    o["provider_key"] = m_providerKey;
    o["last_modified"]= m_lastModified.toString(Qt::ISODate);
    o["dirty"]        = m_dirty;
    return o;
}

Event Event::fromJson(const QJsonObject& o)
{
    Event e;
    e.m_id           = o.value("id").toInt(-1);
    e.m_title        = o.value("title").toString();
    e.m_description  = o.value("desc").toString();
    e.m_startTime    = QDateTime::fromString(o.value("start").toString(), Qt::ISODate);
    e.m_endTime      = QDateTime::fromString(o.value("end").toString(),   Qt::ISODate);
    e.m_color        = QColor(o.value("color_r").toInt(120),
                              o.value("color_g").toInt(144),
                              o.value("color_b").toInt(156));
    e.m_seriesId     = o.value("series_id").toString();
    e.m_source       = static_cast<Source>(o.value("source").toInt(0));
    e.m_externalId   = o.value("external_id").toString();
    e.m_providerKey  = o.value("provider_key").toString();
    e.m_lastModified = QDateTime::fromString(o.value("last_modified").toString(), Qt::ISODate);
    e.m_dirty        = o.value("dirty").toBool(false);
    return e;
}

// ============================================================================
// Google Calendar API JSON mapping
// https://developers.google.com/calendar/api/v3/reference/events
// ============================================================================

QJsonObject Event::toGoogleJson() const
{
    QJsonObject o;
    o["summary"]     = m_title;
    o["description"] = m_description;

    // Google uses dateTime for timed events, date for all-day
    QJsonObject startObj, endObj;
    startObj["dateTime"] = m_startTime.toUTC().toString(Qt::ISODate);
    startObj["timeZone"] = QString::fromUtf8(QTimeZone::systemTimeZone().id());
    endObj["dateTime"]   = m_endTime.toUTC().toString(Qt::ISODate);
    endObj["timeZone"]   = QString::fromUtf8(QTimeZone::systemTimeZone().id());

    o["start"] = startObj;
    o["end"]   = endObj;

    return o;
}

Event Event::fromGoogleJson(const QJsonObject& o)
{
    Event e;
    e.m_externalId  = o.value("id").toString();
    e.m_title       = o.value("summary").toString();
    e.m_description = o.value("description").toString();
    e.m_source      = Google;

    // Parse start/end — handle both dateTime and date (all-day) formats
    const QJsonObject startObj = o.value("start").toObject();
    const QJsonObject endObj   = o.value("end").toObject();

    if (startObj.contains("dateTime")) {
        e.m_startTime = QDateTime::fromString(startObj.value("dateTime").toString(), Qt::ISODate);
    } else if (startObj.contains("date")) {
        e.m_startTime = QDateTime(QDate::fromString(startObj.value("date").toString(), Qt::ISODate),
                                  QTime(0, 0));
    }

    if (endObj.contains("dateTime")) {
        e.m_endTime = QDateTime::fromString(endObj.value("dateTime").toString(), Qt::ISODate);
    } else if (endObj.contains("date")) {
        e.m_endTime = QDateTime(QDate::fromString(endObj.value("date").toString(), Qt::ISODate),
                                QTime(23, 59));
    }

    // Convert UTC to local
    e.m_startTime = e.m_startTime.toLocalTime();
    e.m_endTime   = e.m_endTime.toLocalTime();

    // Parse updated timestamp
    if (o.contains("updated"))
        e.m_lastModified = QDateTime::fromString(o.value("updated").toString(), Qt::ISODate);

    // Color from colorId (Google uses numeric IDs 1-11)
    const int colorId = o.value("colorId").toString().toInt();
    static const QColor googleColors[] = {
        QColor("#7986cb"), QColor("#33b679"), QColor("#8e24aa"), QColor("#e67c73"),
        QColor("#f6bf26"), QColor("#f4511e"), QColor("#039be5"), QColor("#616161"),
        QColor("#3f51b5"), QColor("#0b8043"), QColor("#d50000")
    };
    if (colorId >= 1 && colorId <= 11)
        e.m_color = googleColors[colorId - 1];
    else
        e.m_color = QColor("#3b82f6");

    return e;
}

// ============================================================================
// Outlook / Microsoft Graph API JSON mapping
// https://learn.microsoft.com/en-us/graph/api/resources/event
// ============================================================================

QJsonObject Event::toOutlookJson() const
{
    QJsonObject o;
    o["subject"] = m_title;

    QJsonObject body;
    body["contentType"] = "text";
    body["content"]     = m_description;
    o["body"] = body;

    // Outlook uses dateTimeTimeZone objects
    QJsonObject startObj, endObj;
    startObj["dateTime"] = m_startTime.toString("yyyy-MM-ddTHH:mm:ss");
    startObj["timeZone"] = QString::fromUtf8(QTimeZone::systemTimeZone().id());
    endObj["dateTime"]   = m_endTime.toString("yyyy-MM-ddTHH:mm:ss");
    endObj["timeZone"]   = QString::fromUtf8(QTimeZone::systemTimeZone().id());

    o["start"] = startObj;
    o["end"]   = endObj;

    return o;
}

Event Event::fromOutlookJson(const QJsonObject& o)
{
    Event e;
    e.m_externalId  = o.value("id").toString();
    e.m_title       = o.value("subject").toString();
    e.m_source      = Outlook;

    // Body
    const QJsonObject body = o.value("body").toObject();
    e.m_description = body.value("content").toString();

    // Start/end
    const QJsonObject startObj = o.value("start").toObject();
    const QJsonObject endObj   = o.value("end").toObject();

    e.m_startTime = QDateTime::fromString(startObj.value("dateTime").toString(),
                                          "yyyy-MM-ddTHH:mm:ss.zzzzzzz");
    if (!e.m_startTime.isValid())
        e.m_startTime = QDateTime::fromString(startObj.value("dateTime").toString(),
                                              "yyyy-MM-ddTHH:mm:ss");

    e.m_endTime = QDateTime::fromString(endObj.value("dateTime").toString(),
                                        "yyyy-MM-ddTHH:mm:ss.zzzzzzz");
    if (!e.m_endTime.isValid())
        e.m_endTime = QDateTime::fromString(endObj.value("dateTime").toString(),
                                            "yyyy-MM-ddTHH:mm:ss");

    // lastModifiedDateTime
    if (o.contains("lastModifiedDateTime"))
        e.m_lastModified = QDateTime::fromString(
            o.value("lastModifiedDateTime").toString(), Qt::ISODate);

    e.m_color = QColor("#0078d4"); // Outlook blue

    return e;
}
