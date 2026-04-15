#include "NotificationManager.h"

#import <UserNotifications/UserNotifications.h>
#import <Foundation/Foundation.h>

#include <QDateTime>

static NSString* const kCategoryId  = @"EDUSYNC_EVENT";
static NSString* const kIdPrefix    = @"edusync.event.";
static const int       kLeadMinutes = 15;   // remind 15 min before

NotificationManager& NotificationManager::instance() {
    static NotificationManager inst;
    return inst;
}

void NotificationManager::requestAuthorization() {
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound)
                          completionHandler:^(BOOL granted, NSError* error) {
        if (granted) {
            dispatch_async(dispatch_get_main_queue(), ^{
                // Store authorized flag (best-effort; center always checks real status)
            });
        }
        Q_UNUSED(error)
    }];
}

void NotificationManager::cancelAll() {
    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    // Remove all pending notifications with our prefix
    [center getPendingNotificationRequestsWithCompletionHandler:^(NSArray<UNNotificationRequest*>* requests) {
        NSMutableArray<NSString*>* ids = [NSMutableArray array];
        for (UNNotificationRequest* req in requests) {
            if ([req.identifier hasPrefix:kIdPrefix])
                [ids addObject:req.identifier];
        }
        if (ids.count > 0)
            [center removePendingNotificationRequestsWithIdentifiers:ids];
    }];
}

void NotificationManager::scheduleEventReminders(const QVector<Event>& events) {
    cancelAll();

    UNUserNotificationCenter* center = [UNUserNotificationCenter currentNotificationCenter];
    const QDateTime now = QDateTime::currentDateTime();

    for (const Event& ev : events) {
        const QDateTime fireAt = ev.getStartTime().addSecs(-kLeadMinutes * 60);
        if (fireAt <= now) continue;                    // already past or too soon
        if (ev.getStartTime() > now.addDays(7)) continue; // only next 7 days

        // Notification content
        UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
        content.title = ev.getTitle().toNSString();
        content.body  = [NSString stringWithFormat:@"Starts at %@",
                         ev.getStartTime().time().toString("h:mm ap").toNSString()];
        content.sound = [UNNotificationSound defaultSound];
        content.categoryIdentifier = kCategoryId;

        // Calendar trigger
        NSDate* nsFireDate = [NSDate dateWithTimeIntervalSince1970:
                              (double)fireAt.toSecsSinceEpoch()];
        NSCalendar* cal = [NSCalendar currentCalendar];
        NSDateComponents* comps = [cal components:
            (NSCalendarUnitYear | NSCalendarUnitMonth | NSCalendarUnitDay |
             NSCalendarUnitHour | NSCalendarUnitMinute | NSCalendarUnitSecond)
                                         fromDate:nsFireDate];
        UNCalendarNotificationTrigger* trigger =
            [UNCalendarNotificationTrigger triggerWithDateMatchingComponents:comps repeats:NO];

        // Unique ID: prefix + event id + start epoch
        NSString* notifId = [NSString stringWithFormat:@"%@%d.%lld",
                             kIdPrefix, ev.getId(),
                             (long long)ev.getStartTime().toSecsSinceEpoch()];

        UNNotificationRequest* req = [UNNotificationRequest
            requestWithIdentifier:notifId
                          content:content
                          trigger:trigger];

        [center addNotificationRequest:req withCompletionHandler:nil];
    }
}
