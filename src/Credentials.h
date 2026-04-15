#pragma once

#include <QString>
#include <QSettings>

// ============================================================================
// OAuth2 Credentials
//
// Resolution order:
//   1. QSettings (previously saved via developer setup dialog)
//   2. Compile-time constants below (for pre-built distribution)
//
// For development:
//   Leave the placeholders as-is. On first launch, a setup dialog will
//   prompt you to enter your credentials once. They're stored permanently.
//
// For distribution:
//   Replace the constants below with your real credentials before building.
//   End users will never be prompted.
//
// SETUP:
//   Google:
//     1. Go to https://console.cloud.google.com
//     2. Create project -> Enable Google Calendar API
//     3. Credentials -> Create OAuth 2.0 Client ID (type: Desktop)
//     4. Copy Client ID and Client Secret
//     5. Add http://localhost as authorized redirect URI
//
//   Microsoft/Outlook:
//     1. Go to https://portal.azure.com -> App registrations
//     2. New registration (type: public client / native)
//     3. Add redirect URI: http://localhost (type: Mobile and desktop)
//     4. API permissions -> Add: Microsoft Graph -> Calendars.ReadWrite
//     5. Copy Application (client) ID
//     6. No client secret needed (public client)
//
// ============================================================================

namespace Credentials {

// ── Compile-time defaults (replace for distribution builds) ─────────────────
inline constexpr const char* kDefaultGoogleClientId     = "YOUR_GOOGLE_CLIENT_ID.apps.googleusercontent.com";
inline constexpr const char* kDefaultGoogleClientSecret = "YOUR_GOOGLE_CLIENT_SECRET";
inline constexpr const char* kDefaultOutlookClientId    = "YOUR_OUTLOOK_APPLICATION_ID";

// ── Placeholder detection ───────────────────────────────────────────────────
inline bool isPlaceholder(const QString& val) {
    return val.isEmpty() || val.startsWith("YOUR_");
}

// ── Runtime accessors (QSettings first, then compile-time) ──────────────────
inline QString googleClientId() {
    QSettings s("EduSync", "EduSync");
    QString v = s.value("credentials/google_client_id").toString();
    if (!isPlaceholder(v)) return v;
    return QString(kDefaultGoogleClientId);
}

inline QString googleClientSecret() {
    QSettings s("EduSync", "EduSync");
    QString v = s.value("credentials/google_client_secret").toString();
    if (!isPlaceholder(v)) return v;
    return QString(kDefaultGoogleClientSecret);
}

inline QString outlookClientId() {
    QSettings s("EduSync", "EduSync");
    QString v = s.value("credentials/outlook_client_id").toString();
    if (!isPlaceholder(v)) return v;
    return QString(kDefaultOutlookClientId);
}

// ── Check if credentials are configured ─────────────────────────────────────
inline bool isGoogleConfigured() {
    return !isPlaceholder(googleClientId()) && !isPlaceholder(googleClientSecret());
}

inline bool isOutlookConfigured() {
    return !isPlaceholder(outlookClientId());
}

// ── Save credentials to QSettings ───────────────────────────────────────────
inline void saveGoogleCredentials(const QString& clientId, const QString& clientSecret) {
    QSettings s("EduSync", "EduSync");
    s.setValue("credentials/google_client_id", clientId);
    s.setValue("credentials/google_client_secret", clientSecret);
}

inline void saveOutlookCredentials(const QString& clientId) {
    QSettings s("EduSync", "EduSync");
    s.setValue("credentials/outlook_client_id", clientId);
}

} // namespace Credentials
