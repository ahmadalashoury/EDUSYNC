#include "Event.h"
#include <QJsonValue>

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
{
}

QJsonObject Event::toJson() const
{
    QJsonObject o;
    o["id"]        = m_id;
    o["title"]     = m_title;
    o["desc"]      = m_description;
    o["start"]     = m_startTime.toString(Qt::ISODate);
    o["end"]       = m_endTime.toString(Qt::ISODate);
    o["color_r"]   = m_color.red();
    o["color_g"]   = m_color.green();
    o["color_b"]   = m_color.blue();
    o["series_id"] = m_seriesId;
    return o;
}

Event Event::fromJson(const QJsonObject& o)
{
    Event e;
    e.m_id          = o.value("id").toInt(-1);
    e.m_title       = o.value("title").toString();
    e.m_description = o.value("desc").toString();
    e.m_startTime   = QDateTime::fromString(o.value("start").toString(), Qt::ISODate);
    e.m_endTime     = QDateTime::fromString(o.value("end").toString(),   Qt::ISODate);
    e.m_color       = QColor(o.value("color_r").toInt(120),
                             o.value("color_g").toInt(144),
                             o.value("color_b").toInt(156));
    e.m_seriesId    = o.value("series_id").toString();
    return e;
}
