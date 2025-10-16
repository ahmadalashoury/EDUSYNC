// UltraDashboardRender.h
#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include "Event.h"

// UltraDashboardRender.h

struct TimeBucket {
    QString label;
    QString value;   // e.g. "4h" or "30m"
    int     percent; // 0..100
};

struct DayStats {
    // chips
    QString dateLabel;
    int sessions = 0;
    int meetings = 0;
    int defense  = 0;

    // totals
    bool focusOn = false;
    int  breaksMin = 0;
    int  exerciseMin = 0;
    int  freeMin = 0;

    // schedule health
    int  loadMin = 0;       // total active minutes (focus+break+exercise)
    int  fragmentation = 0; // small gaps
    int  contextSwitches = 0;

    // balance & risk
    int     balancePercent = 0;
    int     riskPercent    = 0;
    QString riskLabel      = "Low";

    // time map & meta
    QVector<TimeBucket> timeMap;
    QString firstStart = "--";
    QString lastEnd    = "--";
    QString longestFocus = "0m";

    // suggestions
    QStringList smartMoves;
};

// Renders a pretty dashboard page
QString buildDashboardHtml(const DayStats& s, bool dark);

// Computes stats for a given day then renders the page
QString buildDailyDashboardHtml(const QVector<Event>& events,
                                bool lightTheme,
                                const QDate& day);
