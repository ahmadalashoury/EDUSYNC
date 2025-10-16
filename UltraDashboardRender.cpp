// UltraDashboardRender.cpp
#include "UltraDashboardRender.h"
#include <algorithm>
#include <QDateTime>
#include <QTime>
#include <QtGlobal>

static inline QString percent(int n){ return QString::number(qBound(0,n,100)) + "%"; }

// --- progress bar helper (keeps bar inside its box)
static QString progressBar(int pct, const QString& track, const QString& fill, bool thin = true) {
    const QString h = thin ? "8px" : "10px";
    return QString()
      + "<div style='width:100%;height:" + h + ";"
      + "background:" + track + ";border:1px solid rgba(0,0,0,0.1);"
      + "border-radius:999px;overflow:hidden;box-sizing:border-box;'>"
      +   "<div style='width:" + percent(pct) + ";height:100%;background:" + fill + ";'></div>"
      + "</div>";
}




QString buildDashboardHtml(const DayStats& s, bool dark) {
    // Design tokens
    const QString bg       = dark ? "#15181b" : "#ffffff";
    const QString card     = dark ? "#202427" : "#ffffff";
    const QString border   = dark ? "rgba(255,255,255,0.06)" : "#e5e7eb";

    const QString text     = dark ? "#e6eaf0" : "#0b1220";
    const QString muted    = dark ? "#8f9ba7" : "#667085";
    const QString chipBg   = dark ? "#151a1f" : "#f9fafb";
    const QString chipTx   = dark ? "#e6eaf0" : "#1f2937";
    const QString brand    = "#2f6feb";
    const QString ok       = "#22c55e";   // consistent green in light & dark
    const QString warn     = dark ? "#fbbf24" : "#f59e0b";
    const QString goodFill = ok;

    auto chip = [&](const QString& label){
        return QString("<div style='background:%1;color:%2;border:1px solid %3;"
                       "padding:6px 10px;border-radius:999px;font-weight:600;'>%4</div>")
                .arg(chipBg, chipTx, border, label);
    };
    
    
auto metricRow = [&](const QString& label, const QString& value, int pct, const QString& fill){
    return QString()
    + "<div style='display:grid;align-items:center;"
      "grid-template-columns:auto min-content 1fr;"
      "column-gap:12px;margin:6px 0;'>"
    +   "<div style='color:" + muted + ";'>" + label + "</div>"
    +   "<div style='font-weight:600;white-space:nowrap;'>" + value + "</div>"
    +   "<div style='width:100%;padding-right:14px;'>"
    +       progressBar(qBound(0,pct,100), (dark ? "#151a1f" : "#f2f4f7"), fill)
    +   "</div>"
    + "</div>";
};







    auto timeMapRow = [&](const TimeBucket& b){
        return QString()
        + "<div style='display:contents;'>"
          "<div style='color:" + muted + ";'>" + b.label + "</div>"
          "<div>" + b.value + "</div>"
          "<div>" + progressBar(b.percent, dark? "#151a1f":"#f9fafb", brand) + "</div>"
          "</div>";
    };

    QString smartList;
    if (s.smartMoves.isEmpty()) {
        smartList = "<li>You’re set — cadence looks healthy.</li>";
    } else {
        for (const auto& it : s.smartMoves) smartList += "<li>" + it.toHtmlEscaped() + "</li>";
    }

    QString html;
    html.reserve(9000);
    html += "<div style='background:" + bg + ";color:" + text
          + ";font-family:-apple-system,system-ui,Segoe UI,Roboto,Arial;"
          "padding:12px;'>";

    // Top chips
    html += "<div style='display:flex;gap:8px;flex-wrap:wrap;margin-bottom:12px;'>";
    html += chip("● " + s.dateLabel);
    html += chip(QString::number(s.sessions) + " sessions");
    html += chip("Meetings: " + QString::number(s.meetings));
    html += chip("Defense: " + QString::number(s.defense));
    html += "</div>";

    // 3-up grid
    html += "<div style='display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px;'>";

    // Totals
    html += "<div style='background:" + card + ";border:1px solid " + border + ";border-radius:14px;padding:14px;overflow:hidden;position:relative;'>";
    html += "<div style='font-size:12px;color:" + muted + ";text-transform:uppercase;letter-spacing:.04em;'>Totals</div>";
    html += "<div style='margin-top:10px;font-size:14px;"
            "display:grid;grid-template-columns:auto 1fr;gap:8px 16px;align-items:center;'>";
    html += "<div>Focus</div><div>"     + QString(s.focusOn ? "On" : "Off") + "</div>";
    html += "<div>Breaks</div><div>"    + QString::number(s.breaksMin)   + "m</div>";
    html += "<div>Exercise</div><div>"  + QString::number(s.exerciseMin) + "m</div>";
    html += "<div>Free</div><div>"      + QString::number(s.freeMin)     + "m</div>";
    html += "</div></div>";

    // Schedule Health
    html += "<div style='background:" + card + ";border:1px solid " + border + ";border-radius:14px;padding:14px;overflow:hidden;position:relative;'>";
    html += "<div style='font-size:12px;color:" + muted + ";text-transform:uppercase;letter-spacing:.04em;'>Schedule Health</div>";
    html += "<div style='margin-top:10px;font-size:14px;'>";
 html += metricRow("Load",
              QString::number(s.loadMin) + "m",
              qMin(100, s.loadMin / 6),
              ok);
html += metricRow("Fragmentation",
              QString::number(s.fragmentation),
              qMin(100, s.fragmentation*15),
              ok);
    html += "<div style='display:flex;align-items:center;gap:10px;margin:6px 0;'>"
              "<div style='min-width:120px;color:" + muted + ";'>Context switches</div>"
              "<div style='flex:1;'><b>" + QString::number(s.contextSwitches) + "</b></div>"
            "</div>";
    html += "</div></div>";

    // Balance & Risk
    html += "<div style='background:" + card + ";border:1px solid " + border + ";border-radius:14px;padding:14px;overflow:hidden;position:relative;'>";
    html += "<div style='font-size:12px;color:" + muted + ";text-transform:uppercase;letter-spacing:.04em;'>Balance & Risk</div>";
    html += "<div style='margin-top:10px;'>";
    html += metricRow("Balance", QString::number(s.balancePercent) + "%", s.balancePercent, goodFill);
    html += "<div style='font-size:12px;color:" + muted + ";'>Status: "
              "<b style='color:" + (s.balancePercent>=70?goodFill:warn) + ";'>"
              + (s.balancePercent>=70? "Good" : (s.balancePercent>=40? "Fair" : "Poor")) + "</b></div>";
    html += metricRow("Risk", s.riskLabel, s.riskPercent, (s.riskPercent<=30?goodFill:warn));
    html += "<div style='font-size:12px;color:" + muted + ";'>Level: "
              "<b style='color:" + (s.riskPercent<=30?goodFill:warn) + ";'>" + QString::number(s.riskPercent) + "%</b></div>";
    html += "</div></div>";

    html += "</div>"; // end 3-up

    // Time map & Smart moves
    html += "<div style='display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:12px;'>";

    // Time Map
    html += "<div style='background:" + card + ";border:1px solid " + border + ";border-radius:14px;padding:14px;overflow:hidden;position:relative;'>";
    html += "<div style='font-size:12px;color:" + muted + ";text-transform:uppercase;letter-spacing:.04em;'>Time Map</div>";
    html += "<div style='margin-top:10px;font-size:14px;"
            "display:grid;grid-template-columns:100px 1fr 140px;gap:10px 16px;align-items:center;'>";
    for (const auto& b : s.timeMap) html += timeMapRow(b);
    html += "</div>";
    html += "<div style='margin-top:12px;color:" + muted + ";font-size:12px;'>"
            "First start: <b>" + s.firstStart + "</b> &nbsp; • &nbsp; "
            "Last end: <b>" + s.lastEnd + "</b> &nbsp; • &nbsp; "
            "Longest focus: <b>" + s.longestFocus + "</b>"
            "</div>";
    html += "</div>";

    // Smart Moves
    html += "<div style='background:" + card + ";border:1px solid " + border + ";border-radius:14px;padding:14px;overflow:hidden;position:relative;'>";
    html += "<div style='font-size:12px;color:" + muted + ";text-transform:uppercase;letter-spacing:.04em;'>Smart Moves</div>";
    html += "<ul style='margin:12px 0 0 18px;padding:0;line-height:1.55;'>";
    html += smartList;
    html += "</ul></div>";

    html += "</div>"; // end lower grid

  html += "</div>"; // root

// Wrap in a full document to control page background & margins
const char* baseCssLight = R"(
  html, body { margin:0; padding:0; background:#ffffff; color:#0b1220; }
  ::-webkit-scrollbar{ width:8px; height:8px; }
  ::-webkit-scrollbar-thumb{ background:rgba(0,0,0,.20); border-radius:8px; }
  ::-webkit-scrollbar-track{ background:transparent; }
)";
const char* baseCssDark = R"(
  html, body { margin:0; padding:0; background:#15181b; color:#e6eaf0; }
  ::-webkit-scrollbar{ width:8px; height:8px; }
  ::-webkit-scrollbar-thumb{ background:rgba(255,255,255,.15); border-radius:8px; }
  ::-webkit-scrollbar-track{ background:transparent; }
)";
const QString baseCss = dark ? baseCssDark : baseCssLight;

return QString(R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="color-scheme" content="dark light">
  <style>%1</style>
</head>
<body>%2</body>
</html>
)").arg(baseCss, html);

}

// ---- helpers for computing DayStats ----------------------------------------
static inline QString mmLocal(int minutes){
    if (minutes <= 0) return "0m";
    const int h = minutes/60, m = minutes%60;
    if (h && m) return QString("%1h %2m").arg(h).arg(m);
    if (h) return QString("%1h").arg(h);
    return QString("%1m").arg(m);
}
static inline QString descCategory(const Event& e) {
    const QString d = e.getDescription();
    const int i = d.indexOf("::");
    return (i < 0 ? d : d.left(i)).trimmed();
}
static inline bool isMeetingTitle(const QString& t){
    const QString s = t.toLower();
    return s.contains("meeting") || s.contains("standup") || s.contains("sync")
        || s.contains("review")  || s.contains("1:1")     || s.contains("retro")
        || s.contains("interview");
}

// Main computation: build stats then render
QString buildDailyDashboardHtml(const QVector<Event>& events, bool lightTheme, const QDate& day)
{
    DayStats st;
    st.dateLabel = day.toString("ddd, MMM d");

    // collect today's events
    QVector<const Event*> todays;
    todays.reserve(events.size());
    for (const auto& e : events) if (e.isOnDate(day)) todays.push_back(&e);
    std::sort(todays.begin(), todays.end(),
              [](const Event* a, const Event* b){ return a->getStartTime() < b->getStartTime(); });

    // aggregate
    int focusMin=0, breakMin=0, exerciseMin=0, sessions=0, longestFocus=0;
    int meetingCount=0, fragments=0;
    QTime firstStart, lastEnd;
    QDateTime prevEnd;

    for (const Event* e : todays) {
        const int dur = e->getStartTime().secsTo(e->getEndTime())/60;
        const QString cat = descCategory(*e).toLower();

        if (cat == "break")              breakMin    += dur;
        else if (cat == "exercise")      exerciseMin += dur;
        else                              { focusMin += dur; sessions++; longestFocus = std::max(longestFocus, dur); }

        if (isMeetingTitle(e->getTitle())) meetingCount++;

        if (!firstStart.isValid() || e->getStartTime().time() < firstStart) firstStart = e->getStartTime().time();
        if (!lastEnd.isValid()   || e->getEndTime().time()     > lastEnd)   lastEnd   = e->getEndTime().time();

        if (prevEnd.isValid()) {
            const int gap = prevEnd.secsTo(e->getStartTime())/60;
            if (gap > 0 && gap < 25) fragments++; // tiny gaps = fragmentation
        }
        prevEnd = e->getEndTime();
    }

    const int daySpan = (firstStart.isValid() && lastEnd.isValid())
                        ? QTime(0,0).secsTo(lastEnd) / 60 - QTime(0,0).secsTo(firstStart) / 60
                        : 0;
    const int activeMin = focusMin + breakMin + exerciseMin;
    const int freeMin   = std::max(0, daySpan - activeMin);

    // metrics
    const int contextSwitches = std::max(0, sessions + meetingCount + fragments - 1);
    const int load    = qBound(0, focusMin/9 + sessions*3 + meetingCount*4 + fragments*2, 100);
    const int balance = qBound(0, 70 + (exerciseMin/15) - (std::abs(focusMin - (breakMin*2))/10), 100);
    const int risk    = qBound(0, load - (breakMin/6) - (exerciseMin/10), 100);

    // time windows free minutes helper
    auto minutesFreeIn = [&](int startH, int endH){
        int used = 0;
        for (const Event* e : todays) {
            const auto s = e->getStartTime().time();
            const auto t = e->getEndTime().time();
            const int a = qBound(startH*60, s.hour()*60 + s.minute(), endH*60);
            const int b = qBound(startH*60, t.hour()*60 + t.minute(), endH*60);
            used += std::max(0, b - a);
        }
        return std::max(0, (endH-startH)*60 - used);
    };

    const int morningSpan   = (12-8)*60, afternoonSpan=(17-12)*60, eveningSpan=(21-17)*60;
    const int freeMorning   = minutesFreeIn(8,12);
    const int freeAfternoon = minutesFreeIn(12,17);
    const int freeEvening   = minutesFreeIn(17,21);

    // Build smart moves
    QStringList actions;
    if (breakMin < 20) actions << "Add 2×10m micro-breaks to reduce fatigue";
    if (exerciseMin < 30) actions << "Schedule a 30–45m exercise block";
    if (meetingCount >= 4 && fragments >= 2) actions << "Defragment: stack adjacent meetings or move one to tomorrow";
    if (freeAfternoon >= 60 && longestFocus < 60 && focusMin >= 90)
        actions << "Convert afternoon into a 90m deep-work block";
    if (freeMorning < 30 && freeEvening >= 60)
        actions << "Shift low-priority work to evening to free morning focus time";
    if (actions.isEmpty()) actions << "You’re set — cadence looks healthy";

    // Fill DayStats
    st.sessions         = sessions;
    st.meetings         = meetingCount;
    st.defense = (balance >= 70) ? 1 : 0;
    st.focusOn          = (focusMin > 0);
    st.breaksMin        = breakMin;
    st.exerciseMin      = exerciseMin;
    st.freeMin          = freeMin;
    st.loadMin          = activeMin;   // total active minutes (focus+break+exercise)
    st.fragmentation    = fragments;
    st.contextSwitches  = contextSwitches;
    st.balancePercent   = balance;
    st.riskPercent      = risk;
    st.riskLabel        = (risk >= 70 ? "High" : (risk >= 40 ? "Medium" : "Low"));
    st.firstStart       = firstStart.isValid()? firstStart.toString("hh:mm") : "--";
    st.lastEnd          = lastEnd.isValid()?   lastEnd.toString("hh:mm")     : "--";
    st.longestFocus     = mmLocal(longestFocus);
    st.smartMoves       = actions;

    st.timeMap = {
        TimeBucket{ "Morning",   mmLocal(freeMorning),   morningSpan>0   ? (freeMorning   *100)/morningSpan   : 0 },
        TimeBucket{ "Afternoon", mmLocal(freeAfternoon), afternoonSpan>0 ? (freeAfternoon *100)/afternoonSpan : 0 },
        TimeBucket{ "Evening",   mmLocal(freeEvening),   eveningSpan>0   ? (freeEvening   *100)/eveningSpan   : 0 },
    };

    // Compose final HTML
    const bool darkTheme = !lightTheme;
    return buildDashboardHtml(st, darkTheme);
}
