#pragma once
#include <QString>
#include <QDate>
#include <QDateTime>
#include <QColor>
#include <QJsonObject>

class Event
{
public:
    // Source tracking for sync
    enum Source { Local = 0, Google = 1, Outlook = 2, CalDAV = 3, AppleNative = 4 };

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
    Source             source()         const { return m_source; }
    const QString&     externalId()     const { return m_externalId; }
    const QString&     providerKey()    const { return m_providerKey; }
    const QDateTime&   lastModified()   const { return m_lastModified; }
    bool               isDirty()        const { return m_dirty; }

    // Setters
    void setId(int id)                             { m_id = id; }
    void setTitle(const QString& t)                { m_title = t; }
    void setDescription(const QString& d)          { m_description = d; }
    void setStartTime(const QDateTime& dt)         { m_startTime = dt; }
    void setEndTime(const QDateTime& dt)           { m_endTime = dt; }
    void setColor(const QColor& c)                 { m_color = c; }
    void setSeriesId(const QString& id)            { m_seriesId = id; }
    void setSource(Source s)                        { m_source = s; }
    void setExternalId(const QString& id)          { m_externalId = id; }
    void setProviderKey(const QString& key)        { m_providerKey = key; }
    void setLastModified(const QDateTime& dt)      { m_lastModified = dt; }
    void setDirty(bool d)                          { m_dirty = d; }

    // Convenience
    bool isOnDate(const QDate& d) const {
        return m_startTime.date() <= d && d <= m_endTime.date();
    }

    // (De)serialization
    QJsonObject toJson() const;
    static Event fromJson(const QJsonObject& obj);

    // Convert to Google Calendar API JSON
    QJsonObject toGoogleJson() const;
    static Event fromGoogleJson(const QJsonObject& obj);

    // Convert to Microsoft Graph API JSON
    QJsonObject toOutlookJson() const;
    static Event fromOutlookJson(const QJsonObject& obj);

private:
    int         m_id = -1;
    QString     m_title;
    QString     m_description;
    QDateTime   m_startTime;
    QDateTime   m_endTime;
    QColor      m_color;
    QString     m_seriesId;
    Source       m_source = Local;
    QString     m_externalId;      // provider's event ID (Google/Outlook/CalDAV/native)
    QString     m_providerKey;     // stable provider instance key for multi-account sync
    QDateTime   m_lastModified;    // for conflict resolution
    bool        m_dirty = false;   // local changes not yet pushed
};
