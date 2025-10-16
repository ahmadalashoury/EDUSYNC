#pragma once
#include <QString>
#include <QDate>
#include <QDateTime>
#include <QColor>
#include <QJsonObject>

class Event
{
public:
    Event();
    Event(const QString& title,
          const QString& description,
          const QDateTime& start,
          const QDateTime& end,
          const QColor& color,
          const QString& seriesId = QString(),
          int id = -1);

    // Getters
    int                getId()          const { return m_id; }
    const QString&     getTitle()       const { return m_title; }
    const QString&     getDescription() const { return m_description; }
    const QDateTime&   getStartTime()   const { return m_startTime; }
    const QDateTime&   getEndTime()     const { return m_endTime; }
    const QColor&      getColor()       const { return m_color; }
    const QString&     seriesId()       const { return m_seriesId; }

    // Setters
    void setId(int id)                             { m_id = id; }
    void setTitle(const QString& t)                { m_title = t; }
    void setDescription(const QString& d)          { m_description = d; }
    void setStartTime(const QDateTime& dt)         { m_startTime = dt; }
    void setEndTime(const QDateTime& dt)           { m_endTime = dt; }
    void setColor(const QColor& c)                 { m_color = c; }
    void setSeriesId(const QString& id)            { m_seriesId = id; }

    // Convenience
    bool isOnDate(const QDate& d) const {
        return m_startTime.date() <= d && d <= m_endTime.date();
    }

    // (De)serialization (optional, safe no-ops if unused)
    QJsonObject toJson() const;
    static Event fromJson(const QJsonObject& obj);

private:
    int         m_id = -1;
    QString     m_title;
    QString     m_description;
    QDateTime   m_startTime;
    QDateTime   m_endTime;
    QColor      m_color;
    QString     m_seriesId;   // empty for one-off events; same id across a series
};
