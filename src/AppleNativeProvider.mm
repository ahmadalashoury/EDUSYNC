#include "AppleNativeProvider.h"

#include "Theme.h"

#include <QMetaObject>
#include <QPointer>
#include <QSettings>

#import <dispatch/dispatch.h>
#import <EventKit/EventKit.h>
#import <Foundation/Foundation.h>

struct AppleNativeProviderPrivate {
    EKEventStore* store = nil;
};

namespace {

QString fromNSString(NSString* value) {
    return value ? QString::fromUtf8([value UTF8String]) : QString();
}

QDateTime dateTimeFromNSDate(NSDate* date) {
    if (!date)
        return {};
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>([date timeIntervalSince1970])).toLocalTime();
}

QDateTime dateTimeFromComponents(NSDateComponents* comps, bool endOfDayFallback) {
    if (!comps)
        return {};

    QDate date(comps.year, comps.month, comps.day);
    if (!date.isValid())
        return {};

    const bool hasTime = comps.hour != NSDateComponentUndefined
                      && comps.minute != NSDateComponentUndefined;
    if (!hasTime)
        return QDateTime(date, endOfDayFallback ? QTime(10, 0) : QTime(9, 0));

    const int second = comps.second == NSDateComponentUndefined ? 0 : comps.second;
    return QDateTime(date, QTime(comps.hour, comps.minute, second));
}

Event eventFromNativeEvent(EKEvent* nativeEvent) {
    Event event;
    event.setTitle(fromNSString(nativeEvent.title).trimmed().isEmpty()
                   ? "Untitled event"
                   : fromNSString(nativeEvent.title));
    event.setDescription(fromNSString(nativeEvent.notes));
    event.setStartTime(dateTimeFromNSDate(nativeEvent.startDate));
    event.setEndTime(dateTimeFromNSDate(nativeEvent.endDate));
    event.setColor(QColor("#ff6b57"));
    event.setSource(Event::AppleNative);
    event.setExternalId(QString("event:%1").arg(fromNSString(nativeEvent.calendarItemIdentifier)));
    event.setLastModified(dateTimeFromNSDate(nativeEvent.lastModifiedDate));
    return event;
}

Event eventFromReminder(EKReminder* reminder) {
    Event event;
    event.setTitle(fromNSString(reminder.title).trimmed().isEmpty()
                   ? "Reminder"
                   : fromNSString(reminder.title));
    event.setDescription(fromNSString(reminder.notes));
    const QDateTime start = dateTimeFromComponents(reminder.dueDateComponents, false);
    QDateTime end = dateTimeFromComponents(reminder.dueDateComponents, true);
    if (!end.isValid() || end <= start)
        end = start.addSecs(3600);
    event.setStartTime(start);
    event.setEndTime(end);
    event.setColor(QColor("#f59e0b"));
    event.setSource(Event::AppleNative);
    event.setExternalId(QString("reminder:%1").arg(fromNSString(reminder.calendarItemIdentifier)));
    event.setLastModified(dateTimeFromNSDate(reminder.lastModifiedDate));
    return event;
}

QString providerError(NSError* error) {
    return error ? fromNSString(error.localizedDescription) : QString();
}

}

AppleNativeProvider::AppleNativeProvider(QObject* parent)
    : CalendarProvider(parent)
    , d(new AppleNativeProviderPrivate)
{
    d->store = [[EKEventStore alloc] init];
    setAccountKey("apple/native");
    setUserEmail("This Mac");
    loadState();
    updateStatusText();
    if (isAuthenticated())
        setConnectionState(Connected);
}

AppleNativeProvider::~AppleNativeProvider() {
    delete d;
}

bool AppleNativeProvider::isAuthenticated() const {
    return m_enabled && (m_hasCalendarAccess || m_hasReminderAccess);
}

QString AppleNativeProvider::accountDisplayName() const {
    if (m_hasCalendarAccess && m_hasReminderAccess)
        return "Apple Calendar & Reminders";
    if (m_hasCalendarAccess)
        return "Apple Calendar";
    if (m_hasReminderAccess)
        return "Apple Reminders";
    return "Apple on this Mac";
}

void AppleNativeProvider::loadState() {
    QSettings settings("EduSync", "EduSync");
    m_enabled = settings.value("accounts/apple_native/enabled", false).toBool();
    m_hasCalendarAccess = settings.value("accounts/apple_native/calendar_access", false).toBool();
    m_hasReminderAccess = settings.value("accounts/apple_native/reminder_access", false).toBool();
}

void AppleNativeProvider::persist() const {
    QSettings settings("EduSync", "EduSync");
    settings.setValue("accounts/apple_native/enabled", m_enabled);
    settings.setValue("accounts/apple_native/calendar_access", m_hasCalendarAccess);
    settings.setValue("accounts/apple_native/reminder_access", m_hasReminderAccess);
}

void AppleNativeProvider::updateStatusText() {
    if (!m_enabled) {
        setStatusDetail("Not connected");
        return;
    }

    if (m_hasCalendarAccess && m_hasReminderAccess) {
        setStatusDetail("Syncing Apple Calendar and Reminders from this Mac");
    } else if (m_hasCalendarAccess) {
        setStatusDetail("Syncing Apple Calendar from this Mac");
    } else if (m_hasReminderAccess) {
        setStatusDetail("Syncing Apple Reminders from this Mac");
    } else {
        setStatusDetail("Calendar and Reminders permissions are required");
    }
}

void AppleNativeProvider::authenticate() {
    m_enabled = true;
    setConnectionState(Connecting);
    setStatusDetail("Requesting Calendar and Reminders access");

    QPointer<AppleNativeProvider> guard(this);
    __block BOOL calendarGranted = NO;
    __block BOOL reminderGranted = NO;
    __block NSString* calendarError = nil;
    __block NSString* reminderError = nil;

    dispatch_group_t group = dispatch_group_create();

    dispatch_group_enter(group);
    if (@available(macOS 14.0, *)) {
        // macOS 14+: the legacy requestAccessToEntityType: returns write-only
        // access for Calendar, which breaks fetch. Request full access instead.
        [d->store requestFullAccessToEventsWithCompletion:^(BOOL granted, NSError* error) {
            calendarGranted = granted;
            calendarError = error.localizedDescription;
            dispatch_group_leave(group);
        }];
    } else {
        [d->store requestAccessToEntityType:EKEntityTypeEvent completion:^(BOOL granted, NSError* error) {
            calendarGranted = granted;
            calendarError = error.localizedDescription;
            dispatch_group_leave(group);
        }];
    }

    dispatch_group_enter(group);
    if (@available(macOS 14.0, *)) {
        [d->store requestFullAccessToRemindersWithCompletion:^(BOOL granted, NSError* error) {
            reminderGranted = granted;
            reminderError = error.localizedDescription;
            dispatch_group_leave(group);
        }];
    } else {
        [d->store requestAccessToEntityType:EKEntityTypeReminder completion:^(BOOL granted, NSError* error) {
            reminderGranted = granted;
            reminderError = error.localizedDescription;
            dispatch_group_leave(group);
        }];
    }

    dispatch_group_notify(group, dispatch_get_main_queue(), ^{
        if (!guard)
            return;

        guard->m_hasCalendarAccess = calendarGranted;
        guard->m_hasReminderAccess = reminderGranted;
        guard->m_enabled = calendarGranted || reminderGranted;
        guard->persist();
        guard->updateStatusText();

        if (guard->isAuthenticated()) {
            guard->setConnectionState(Connected);
            emit guard->statusMessage("Apple Calendar and Reminders connected");
            emit guard->authenticationComplete(true, QString());
            return;
        }

        QStringList errors;
        if (calendarError.length > 0)
            errors << QString::fromNSString(calendarError);
        if (reminderError.length > 0)
            errors << QString::fromNSString(reminderError);
        if (errors.isEmpty())
            errors << "Calendar and Reminders access was denied.";
        guard->setError(errors.join(' '));
        guard->persist();
        emit guard->authenticationComplete(false, guard->errorMessage());
    });
}

void AppleNativeProvider::signOut() {
    m_enabled = false;
    updateStatusText();
    setConnectionState(Disconnected);
    persist();
    emit statusMessage("Apple Calendar and Reminders disconnected");
}

void AppleNativeProvider::fetchEvents(const QDate& from, const QDate& to) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Apple Calendar and Reminders are not connected.");
        return;
    }

    setConnectionState(Syncing);
    setStatusDetail("Syncing Apple calendars");

    NSMutableArray<EKEvent*>* nativeEvents = [NSMutableArray array];
    if (m_hasCalendarAccess) {
        NSDate* startDate = [NSDate dateWithTimeIntervalSince1970:from.startOfDay().toSecsSinceEpoch()];
        NSDate* endDate = [NSDate dateWithTimeIntervalSince1970:to.addDays(1).startOfDay().toSecsSinceEpoch()];
        NSPredicate* predicate = [d->store predicateForEventsWithStartDate:startDate endDate:endDate calendars:nil];
        NSArray<EKEvent*>* fetched = [d->store eventsMatchingPredicate:predicate];
        if (fetched)
            [nativeEvents addObjectsFromArray:fetched];
    }

    __block QVector<Event> results;
    results.reserve(static_cast<int>(nativeEvents.count) + 16);
    for (EKEvent* nativeEvent in nativeEvents)
        results.push_back(eventFromNativeEvent(nativeEvent));

    if (!m_hasReminderAccess) {
        setConnectionState(Connected);
        setLastSyncTime(QDateTime::currentDateTime());
        updateStatusText();
        emit eventsFetched(results);
        return;
    }

    QPointer<AppleNativeProvider> guard(this);
    NSDate* startDate = [NSDate dateWithTimeIntervalSince1970:from.startOfDay().toSecsSinceEpoch()];
    NSDate* endDate = [NSDate dateWithTimeIntervalSince1970:to.addDays(1).startOfDay().toSecsSinceEpoch()];
    NSPredicate* predicate = [d->store predicateForIncompleteRemindersWithDueDateStarting:startDate
                                                                                ending:endDate
                                                                              calendars:nil];
    [d->store fetchRemindersMatchingPredicate:predicate completion:^(NSArray<EKReminder*>* reminders) {
        if (!guard)
            return;

        if (reminders) {
            for (EKReminder* reminder in reminders) {
                Event reminderEvent = eventFromReminder(reminder);
                if (reminderEvent.getStartTime().isValid())
                    results.push_back(reminderEvent);
            }
        }

        const QVector<Event> finalResults = results;
        QMetaObject::invokeMethod(guard, [guard, finalResults]() {
            if (!guard)
                return;
            guard->setConnectionState(Connected);
            guard->setLastSyncTime(QDateTime::currentDateTime());
            guard->updateStatusText();
            emit guard->eventsFetched(finalResults);
        }, Qt::QueuedConnection);
    }];
}

namespace {

NSDate* dateFromQDateTime(const QDateTime& dt) {
    if (!dt.isValid())
        return nil;
    return [NSDate dateWithTimeIntervalSince1970:static_cast<NSTimeInterval>(dt.toSecsSinceEpoch())];
}

NSDateComponents* reminderDueComponentsFromQDateTime(const QDateTime& dt) {
    if (!dt.isValid())
        return nil;
    NSDateComponents* comps = [[NSDateComponents alloc] init];
    comps.year   = dt.date().year();
    comps.month  = dt.date().month();
    comps.day    = dt.date().day();
    comps.hour   = dt.time().hour();
    comps.minute = dt.time().minute();
    comps.second = dt.time().second();
    return comps;
}

NSString* toNSString(const QString& value) {
    return [NSString stringWithUTF8String:value.toUtf8().constData()];
}

bool isReminderExternalId(const QString& id) {
    return id.startsWith("reminder:");
}

bool isCalendarEventExternalId(const QString& id) {
    return id.startsWith("event:");
}

}

void AppleNativeProvider::createEvent(const Event& event) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Apple Calendar and Reminders are not connected.");
        return;
    }

    // Convention: events routed to Apple Reminders carry the "reminder:" sentinel
    // in externalId (empty identifier after the colon). Calendar events use
    // "event:" or an empty externalId.
    const bool asReminder = isReminderExternalId(event.externalId());

    NSError* error = nil;

    if (asReminder) {
        if (!m_hasReminderAccess) {
            emit operationComplete(false, "Reminders access is required to create reminders.");
            return;
        }
        EKCalendar* list = d->store.defaultCalendarForNewReminders;
        if (!list) {
            emit operationComplete(false, "No default Reminders list is available on this Mac.");
            return;
        }

        EKReminder* reminder = [EKReminder reminderWithEventStore:d->store];
        reminder.title = toNSString(event.getTitle());
        reminder.notes = toNSString(event.getDescription());
        reminder.calendar = list;
        reminder.dueDateComponents = reminderDueComponentsFromQDateTime(event.getStartTime());
        if (event.getStartTime().isValid())
            reminder.startDateComponents = reminderDueComponentsFromQDateTime(event.getStartTime());

        if (![d->store saveReminder:reminder commit:YES error:&error]) {
            emit operationComplete(false, providerError(error).isEmpty()
                ? "Failed to create Apple reminder." : providerError(error));
            return;
        }

        const QString newExternalId = QString("reminder:%1").arg(fromNSString(reminder.calendarItemIdentifier));
        emit eventCreated(newExternalId);
        emit operationComplete(true, QString());
        emit statusMessage("Reminder added to Apple Reminders");
        return;
    }

    if (!m_hasCalendarAccess) {
        emit operationComplete(false, "Calendar access is required to create events.");
        return;
    }
    EKCalendar* calendar = d->store.defaultCalendarForNewEvents;
    if (!calendar) {
        emit operationComplete(false, "No default Calendar is available on this Mac.");
        return;
    }

    EKEvent* nativeEvent = [EKEvent eventWithEventStore:d->store];
    nativeEvent.title = toNSString(event.getTitle());
    nativeEvent.notes = toNSString(event.getDescription());
    nativeEvent.calendar = calendar;
    nativeEvent.startDate = dateFromQDateTime(event.getStartTime());
    nativeEvent.endDate   = dateFromQDateTime(event.getEndTime());

    if (![d->store saveEvent:nativeEvent span:EKSpanThisEvent commit:YES error:&error]) {
        emit operationComplete(false, providerError(error).isEmpty()
            ? "Failed to create Apple Calendar event." : providerError(error));
        return;
    }

    const QString newExternalId = QString("event:%1").arg(fromNSString(nativeEvent.calendarItemIdentifier));
    emit eventCreated(newExternalId);
    emit operationComplete(true, QString());
    emit statusMessage("Event added to Apple Calendar");
}

void AppleNativeProvider::updateEvent(const QString& externalId, const Event& event) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Apple Calendar and Reminders are not connected.");
        return;
    }

    const QString identifier = externalId.section(':', 1);
    if (identifier.isEmpty()) {
        // Treat as create path — caller passed a sentinel but no identifier
        createEvent(event);
        return;
    }

    NSError* error = nil;

    if (isCalendarEventExternalId(externalId)) {
        EKEvent* nativeEvent = [d->store eventWithIdentifier:toNSString(identifier)];
        if (!nativeEvent) {
            emit operationComplete(false, "Apple Calendar event no longer exists.");
            return;
        }
        nativeEvent.title = toNSString(event.getTitle());
        nativeEvent.notes = toNSString(event.getDescription());
        nativeEvent.startDate = dateFromQDateTime(event.getStartTime());
        nativeEvent.endDate   = dateFromQDateTime(event.getEndTime());

        if (![d->store saveEvent:nativeEvent span:EKSpanThisEvent commit:YES error:&error]) {
            emit operationComplete(false, providerError(error).isEmpty()
                ? "Failed to update Apple Calendar event." : providerError(error));
            return;
        }

        emit operationComplete(true, QString());
        emit statusMessage("Apple Calendar event updated");
        return;
    }

    if (isReminderExternalId(externalId)) {
        EKCalendarItem* item = [d->store calendarItemWithIdentifier:toNSString(identifier)];
        EKReminder* reminder = [item isKindOfClass:[EKReminder class]] ? (EKReminder*)item : nil;
        if (!reminder) {
            emit operationComplete(false, "Apple reminder no longer exists.");
            return;
        }
        reminder.title = toNSString(event.getTitle());
        reminder.notes = toNSString(event.getDescription());
        reminder.dueDateComponents = reminderDueComponentsFromQDateTime(event.getStartTime());
        if (event.getStartTime().isValid())
            reminder.startDateComponents = reminderDueComponentsFromQDateTime(event.getStartTime());

        if (![d->store saveReminder:reminder commit:YES error:&error]) {
            emit operationComplete(false, providerError(error).isEmpty()
                ? "Failed to update Apple reminder." : providerError(error));
            return;
        }

        emit operationComplete(true, QString());
        emit statusMessage("Apple reminder updated");
        return;
    }

    emit operationComplete(false, "Unsupported Apple item type.");
}

void AppleNativeProvider::deleteEvent(const QString& externalId) {
    if (!isAuthenticated()) {
        emit operationComplete(false, "Apple Calendar and Reminders are not connected.");
        return;
    }

    const QString identifier = externalId.section(':', 1);
    if (identifier.isEmpty()) {
        emit operationComplete(false, "Invalid Apple item identifier.");
        return;
    }

    NSError* error = nil;
    if (externalId.startsWith("event:")) {
        EKEvent* event = [d->store eventWithIdentifier:[NSString stringWithUTF8String:identifier.toUtf8().constData()]];
        if (!event || ![d->store removeEvent:event span:EKSpanThisEvent commit:YES error:&error]) {
            emit operationComplete(false, providerError(error).isEmpty() ? "Failed to delete Apple Calendar event." : providerError(error));
            return;
        }
    } else if (externalId.startsWith("reminder:")) {
        EKCalendarItem* item = [d->store calendarItemWithIdentifier:[NSString stringWithUTF8String:identifier.toUtf8().constData()]];
        EKReminder* reminder = [item isKindOfClass:[EKReminder class]] ? (EKReminder*)item : nil;
        if (!reminder || ![d->store removeReminder:reminder commit:YES error:&error]) {
            emit operationComplete(false, providerError(error).isEmpty() ? "Failed to delete Apple reminder." : providerError(error));
            return;
        }
    } else {
        emit operationComplete(false, "Unsupported Apple item type.");
        return;
    }

    emit operationComplete(true, QString());
    emit statusMessage("Apple item deleted");
}
