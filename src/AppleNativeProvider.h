#pragma once

#include "CalendarProvider.h"

struct AppleNativeProviderPrivate;

class AppleNativeProvider : public CalendarProvider {
    Q_OBJECT
public:
    explicit AppleNativeProvider(QObject* parent = nullptr);
    ~AppleNativeProvider() override;

    QString name() const override { return "Apple"; }
    Type type() const override { return AppleNative; }
    bool isAuthenticated() const override;
    QString accountDisplayName() const override;

    void authenticate() override;
    void signOut() override;
    void fetchEvents(const QDate& from, const QDate& to) override;
    void createEvent(const Event& event) override;
    void updateEvent(const QString& externalId, const Event& event) override;
    void deleteEvent(const QString& externalId) override;

    bool hasCalendarAccess() const { return m_hasCalendarAccess; }
    bool hasReminderAccess() const { return m_hasReminderAccess; }
    bool isEnabled() const { return m_enabled; }

private:
    void loadState();
    void persist() const;
    void updateStatusText();

    AppleNativeProviderPrivate* d = nullptr;
    bool m_enabled = false;
    bool m_hasCalendarAccess = false;
    bool m_hasReminderAccess = false;
};
