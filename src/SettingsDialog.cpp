#include "SettingsDialog.h"

#include "AppleNativeProvider.h"
#include "CalDAVProvider.h"
#include "Credentials.h"
#include "GoogleCalendarProvider.h"
#include "LLMAssistantService.h"
#include "OutlookCalendarProvider.h"
#include "Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>
#include <memory>

namespace {

QString providerStateLabel(CalendarProvider* provider) {
    if (!provider)
        return "Saved";

    switch (provider->connectionState()) {
    case CalendarProvider::Disconnected: return "Not connected";
    case CalendarProvider::Connecting:   return "Connecting";
    case CalendarProvider::Connected:    return "Connected";
    case CalendarProvider::Syncing:      return "Syncing";
    case CalendarProvider::Error:        return "Needs attention";
    }
    return "Unknown";
}

QColor providerStateColor(const Theme::Colors& colors, CalendarProvider* provider) {
    if (!provider)
        return colors.textSecondary;

    switch (provider->connectionState()) {
    case CalendarProvider::Disconnected: return colors.textTertiary;
    case CalendarProvider::Connecting:
    case CalendarProvider::Syncing:      return colors.warning;
    case CalendarProvider::Connected:    return colors.success;
    case CalendarProvider::Error:        return colors.danger;
    }
    return colors.textTertiary;
}

QString providerSecondaryText(CalendarProvider* provider) {
    if (!provider)
        return "Saved. Connect when you're ready.";

    if (provider->connectionState() == CalendarProvider::Error && !provider->errorMessage().isEmpty())
        return provider->errorMessage();

    if (!provider->statusDetail().isEmpty())
        return provider->statusDetail();

    QStringList parts;
    if (!provider->userEmail().isEmpty())
        parts << provider->userEmail();
    if (provider->lastSyncTime().isValid())
        parts << QString("Last sync %1").arg(provider->lastSyncTime().toString("h:mm AP"));
    return parts.join("  •  ");
}

void styleStateChip(QLabel* chip, const Theme::Colors& colors, CalendarProvider* provider) {
    QColor fg = providerStateColor(colors, provider);
    QColor bg = fg;
    bg.setAlpha(provider && provider->connectionState() == CalendarProvider::Connected ? 36 : 22);

    chip->setText(providerStateLabel(provider));
    chip->setStyleSheet(QString(
        "QLabel {"
        " background: %1;"
        " color: %2;"
        " border-radius: 999px;"
        " padding: 5px 11px;"
        " font-size: 11px;"
        " font-weight: 700;"
        "}"
    ).arg(bg.name(QColor::HexArgb), fg.name()));
}

QFrame* buildCard(QWidget* parent = nullptr) {
    auto* card = new QFrame(parent);
    card->setObjectName("settingsCard");
    return card;
}

}

SettingsDialog::SettingsDialog(GoogleCalendarProvider* google,
                               OutlookCalendarProvider* outlook,
                               AppleNativeProvider* apple,
                               QVector<CalDAVProvider*>* caldavProviders,
                               LLMAssistantService* llm,
                               bool isDarkTheme,
                               Page initialPage,
                               QWidget* parent)
    : QDialog(parent)
    , m_google(google)
    , m_outlook(outlook)
    , m_apple(apple)
    , m_caldavProviders(caldavProviders)
    , m_llm(llm)
    , m_isDark(isDarkTheme)
{
    setWindowTitle("Settings");
    setModal(true);
    resize(980, 700);
    setMinimumSize(900, 640);

    const auto colors = m_isDark ? Theme::dark() : Theme::light();

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_sidebar = new QListWidget;
    m_sidebar->setObjectName("settingsSidebar");
    m_sidebar->setFixedWidth(220);
    m_sidebar->setFrameShape(QFrame::NoFrame);
    m_sidebar->setFocusPolicy(Qt::NoFocus);
    for (const QString& label : {"General", "Appearance", "Accounts", "Assistant", "Advanced"}) {
        auto* item = new QListWidgetItem(label, m_sidebar);
        item->setSizeHint(QSize(-1, 46));
    }
    m_sidebar->setSpacing(2);
    root->addWidget(m_sidebar);

    auto* divider = new QFrame;
    divider->setFrameShape(QFrame::VLine);
    divider->setStyleSheet("background: " + colors.borderSubtle.name() + "; border: none;");
    root->addWidget(divider);

    m_stack = new QStackedWidget;
    m_stack->addWidget(buildGeneralPage());
    m_stack->addWidget(buildAppearancePage());
    m_stack->addWidget(buildAccountsPage());
    m_stack->addWidget(buildAIPage());
    m_stack->addWidget(buildAdvancedPage());
    root->addWidget(m_stack, 1);

    connect(m_sidebar, &QListWidget::currentRowChanged,
            m_stack, &QStackedWidget::setCurrentIndex);

    m_sidebar->setCurrentRow(static_cast<int>(initialPage));
}

QWidget* SettingsDialog::buildGeneralPage() {
    auto* page = new QWidget;
    page->setObjectName("settingsPage");
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(Theme::Space::XXL, Theme::Space::XXL,
                            Theme::Space::XXL, Theme::Space::XL);
    lay->setSpacing(Theme::Space::L);

    auto* eyebrow = new QLabel("PREFERENCES");
    eyebrow->setFont(Theme::Font::caption());
    lay->addWidget(eyebrow);

    auto* title = new QLabel("General");
    title->setFont(Theme::Font::heading());
    lay->addWidget(title);

    auto* desc = new QLabel("Choose the week start, default event length, and time format that match the way you work.");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* card = buildCard();
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(Theme::Space::XL, Theme::Space::XL,
                                Theme::Space::XL, Theme::Space::XL);
    cardLay->setSpacing(Theme::Space::L);

    auto* form = new QFormLayout;
    form->setSpacing(Theme::Space::M);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* firstDay = new QComboBox;
    firstDay->addItems({"Monday", "Sunday", "Saturday"});
    firstDay->setCurrentIndex(QSettings("EduSync", "EduSync").value("general/first_day", 0).toInt());
    connect(firstDay, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int index) {
        QSettings("EduSync", "EduSync").setValue("general/first_day", index);
    });
    form->addRow("First day of week", firstDay);

    auto* defaultDur = new QComboBox;
    defaultDur->addItems({"15 min", "30 min", "45 min", "60 min", "90 min"});
    defaultDur->setCurrentIndex(QSettings("EduSync", "EduSync").value("general/default_duration", 1).toInt());
    connect(defaultDur, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int index) {
        QSettings("EduSync", "EduSync").setValue("general/default_duration", index);
    });
    form->addRow("Default event length", defaultDur);

    auto* timeFormat = new QComboBox;
    timeFormat->addItems({"12-hour", "24-hour"});
    timeFormat->setCurrentIndex(QSettings("EduSync", "EduSync").value("general/time_format", 0).toInt());
    connect(timeFormat, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [](int index) {
        QSettings("EduSync", "EduSync").setValue("general/time_format", index);
    });
    form->addRow("Time format", timeFormat);

    cardLay->addLayout(form);
    lay->addWidget(card);
    lay->addStretch();
    return page;
}

QWidget* SettingsDialog::buildAppearancePage() {
    auto* page = new QWidget;
    page->setObjectName("settingsPage");
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(Theme::Space::XXL, Theme::Space::XXL,
                            Theme::Space::XXL, Theme::Space::XL);
    lay->setSpacing(Theme::Space::L);

    auto* eyebrow = new QLabel("LOOK & FEEL");
    eyebrow->setFont(Theme::Font::caption());
    lay->addWidget(eyebrow);

    auto* title = new QLabel("Appearance");
    title->setFont(Theme::Font::heading());
    lay->addWidget(title);

    auto* desc = new QLabel("Choose the theme that suits your workspace. Switches take effect instantly and apply everywhere in EduSync.");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* card = buildCard();
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(Theme::Space::XL, Theme::Space::XL,
                                Theme::Space::XL, Theme::Space::XL);
    cardLay->setSpacing(Theme::Space::L);

    auto* row = new QHBoxLayout;
    row->setSpacing(Theme::Space::S);

    auto* darkBtn = new QPushButton("Dark");
    darkBtn->setCheckable(true);
    darkBtn->setChecked(m_isDark);
    darkBtn->setObjectName(m_isDark ? "accentBtn" : "subtleBtn");

    auto* lightBtn = new QPushButton("Light");
    lightBtn->setCheckable(true);
    lightBtn->setChecked(!m_isDark);
    lightBtn->setObjectName(!m_isDark ? "accentBtn" : "subtleBtn");

    auto updateTheme = [this, darkBtn, lightBtn](bool dark) {
        m_isDark = dark;
        darkBtn->setChecked(dark);
        lightBtn->setChecked(!dark);
        darkBtn->setObjectName(dark ? "accentBtn" : "subtleBtn");
        lightBtn->setObjectName(!dark ? "accentBtn" : "subtleBtn");
        style()->unpolish(darkBtn);
        style()->polish(darkBtn);
        style()->unpolish(lightBtn);
        style()->polish(lightBtn);
        emit themeChanged(dark);
    };

    connect(darkBtn, &QPushButton::clicked, this, [updateTheme] { updateTheme(true); });
    connect(lightBtn, &QPushButton::clicked, this, [updateTheme] { updateTheme(false); });

    auto* note = new QLabel("Both themes are tuned for long sessions, with calm contrast and legible typography at every size.");
    note->setWordWrap(true);

    row->addWidget(darkBtn);
    row->addWidget(lightBtn);
    row->addStretch();
    cardLay->addLayout(row);
    cardLay->addWidget(note);

    lay->addWidget(card);
    lay->addStretch();
    return page;
}

QWidget* SettingsDialog::buildAccountsPage() {
    auto* page = new QWidget;
    page->setObjectName("settingsPage");
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    pageLayout->addWidget(scroll);

    auto* content = new QWidget;
    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(Theme::Space::XXL, Theme::Space::XXL,
                            Theme::Space::XXL, Theme::Space::XL);
    lay->setSpacing(Theme::Space::L);

    const Theme::Colors colors = m_isDark ? Theme::dark() : Theme::light();

    auto* eyebrow = new QLabel("ACCOUNT CENTER");
    eyebrow->setFont(Theme::Font::caption());
    lay->addWidget(eyebrow);

    auto* title = new QLabel("Accounts");
    title->setFont(Theme::Font::heading());
    lay->addWidget(title);

    auto* desc = new QLabel("Connect every calendar you use. EduSync keeps Apple, Google, Outlook, and CalDAV accounts syncing in the background.");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* hero = buildCard();
    auto* heroLay = new QVBoxLayout(hero);
    heroLay->setContentsMargins(Theme::Space::XL, Theme::Space::XL,
                                Theme::Space::XL, Theme::Space::XL);
    heroLay->setSpacing(Theme::Space::S);

    auto* heroTitle = new QLabel("Every calendar, one view");
    heroTitle->setFont(Theme::Font::title());
    auto* heroCopy = new QLabel("Apple Calendar and Reminders connect natively through macOS. CalDAV adds iCloud, Yahoo, Fastmail, Nextcloud, and any other standards-compliant calendar server.");
    heroCopy->setWordWrap(true);
    heroLay->addWidget(heroTitle);
    heroLay->addWidget(heroCopy);
    lay->addWidget(hero);

    auto buildProviderCard = [this, lay, colors](CalendarProvider* provider,
                                                 const QString& heading,
                                                 const QString& supporting,
                                                 const std::function<void()>& connectAction,
                                                 const QString& primaryLabel,
                                                 const QString& disconnectLabel) {
        auto* card = buildCard();
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(Theme::Space::XL, Theme::Space::XL,
                                       Theme::Space::XL, Theme::Space::XL);
        cardLayout->setSpacing(Theme::Space::M);

        auto* topRow = new QHBoxLayout;
        auto* titleLabel = new QLabel(heading);
        titleLabel->setFont(Theme::Font::subheading());
        auto* stateChip = new QLabel;
        styleStateChip(stateChip, colors, provider);
        topRow->addWidget(titleLabel);
        topRow->addStretch();
        topRow->addWidget(stateChip);
        cardLayout->addLayout(topRow);

        auto* intro = new QLabel(supporting);
        intro->setWordWrap(true);
        cardLayout->addWidget(intro);

        auto* detail = new QLabel(providerSecondaryText(provider));
        detail->setWordWrap(true);
        cardLayout->addWidget(detail);

        auto* buttonRow = new QHBoxLayout;
        buttonRow->setSpacing(Theme::Space::S);
        auto* primary = new QPushButton(primaryLabel);
        primary->setObjectName("accentBtn");
        auto* disconnect = new QPushButton(disconnectLabel);
        disconnect->setObjectName("subtleBtn");

        auto refresh = [provider, stateChip, detail, primary, disconnect, colors, primaryLabel]() {
            styleStateChip(stateChip, colors, provider);
            detail->setText(providerSecondaryText(provider));
            const bool busy = provider && (provider->connectionState() == CalendarProvider::Connecting
                                        || provider->connectionState() == CalendarProvider::Syncing);
            const bool connected = provider && provider->isAuthenticated();
            primary->setText(busy ? "Working..." : connected ? "Reconnect" : primaryLabel);
            primary->setEnabled(!busy);
            disconnect->setVisible(connected);
        };

        refresh();
        connect(primary, &QPushButton::clicked, this, [connectAction, refresh] {
            connectAction();
            refresh();
        });
        connect(disconnect, &QPushButton::clicked, this, [provider, refresh] {
            if (provider)
                provider->signOut();
            refresh();
        });
        if (provider) {
            connect(provider, &CalendarProvider::connectionStateChanged, this,
                    [refresh](CalendarProvider::ConnectionState) { refresh(); });
            connect(provider, &CalendarProvider::authenticationComplete, this,
                    [refresh](bool, const QString&) { refresh(); });
        }

        buttonRow->addWidget(primary);
        buttonRow->addWidget(disconnect);
        buttonRow->addStretch();
        cardLayout->addLayout(buttonRow);

        lay->addWidget(card);
    };

    buildProviderCard(
        m_apple,
        "Apple Calendar & Reminders",
        "Sync your Apple Calendar events and Reminders directly from macOS. No passwords, no extra setup — just grant access once.",
        [this] { if (m_apple) m_apple->authenticate(); },
        "Enable on This Mac",
        "Turn Off"
    );

    buildProviderCard(
        m_google,
        "Google Calendar",
        Credentials::isGoogleConfigured()
            ? "Sign in with Google to sync every calendar on your account in real time."
            : "Google sign-in needs integration credentials. Add them in Advanced to continue.",
        [this] {
            if (!Credentials::isGoogleConfigured()) {
                m_sidebar->setCurrentRow(static_cast<int>(Page::Advanced));
                return;
            }
            if (m_google && m_google->isAuthenticated())
                m_google->signOut();
            if (m_google)
                m_google->authenticate();
        },
        Credentials::isGoogleConfigured() ? "Connect Google" : "Open Advanced",
        "Disconnect"
    );

    buildProviderCard(
        m_outlook,
        "Outlook Calendar",
        Credentials::isOutlookConfigured()
            ? "Sign in with Microsoft to sync your Outlook and Microsoft 365 calendars in real time."
            : "Microsoft sign-in needs integration credentials. Add them in Advanced to continue.",
        [this] {
            if (!Credentials::isOutlookConfigured()) {
                m_sidebar->setCurrentRow(static_cast<int>(Page::Advanced));
                return;
            }
            if (m_outlook && m_outlook->isAuthenticated())
                m_outlook->signOut();
            if (m_outlook)
                m_outlook->authenticate();
        },
        Credentials::isOutlookConfigured() ? "Connect Outlook" : "Open Advanced",
        "Disconnect"
    );

    auto* caldavHeader = new QLabel("CalDAV");
    caldavHeader->setFont(Theme::Font::title());
    lay->addWidget(caldavHeader);

    auto* caldavIntro = new QLabel("Connect iCloud, Yahoo, Fastmail, Nextcloud, or any CalDAV-compatible server. Save your credentials once and EduSync reconnects automatically.");
    caldavIntro->setWordWrap(true);
    lay->addWidget(caldavIntro);

    auto* caldavFormCard = buildCard();
    auto* caldavFormLayout = new QVBoxLayout(caldavFormCard);
    caldavFormLayout->setContentsMargins(Theme::Space::XL, Theme::Space::XL,
                                         Theme::Space::XL, Theme::Space::XL);
    caldavFormLayout->setSpacing(Theme::Space::M);

    auto* presetRow = new QHBoxLayout;
    presetRow->setSpacing(Theme::Space::S);
    auto* iCloudPreset = new QPushButton("iCloud");
    auto* yahooPreset = new QPushButton("Yahoo");
    auto* fastmailPreset = new QPushButton("Fastmail");
    iCloudPreset->setObjectName("subtleBtn");
    yahooPreset->setObjectName("subtleBtn");
    fastmailPreset->setObjectName("subtleBtn");
    presetRow->addWidget(iCloudPreset);
    presetRow->addWidget(yahooPreset);
    presetRow->addWidget(fastmailPreset);
    presetRow->addStretch();
    caldavFormLayout->addLayout(presetRow);

    auto* form = new QGridLayout;
    form->setHorizontalSpacing(Theme::Space::M);
    form->setVerticalSpacing(Theme::Space::S);

    auto* nameEdit = new QLineEdit;
    auto* serverEdit = new QLineEdit;
    auto* usernameEdit = new QLineEdit;
    auto* passwordEdit = new QLineEdit;
    passwordEdit->setEchoMode(QLineEdit::Password);
    nameEdit->setPlaceholderText("Account label");
    serverEdit->setPlaceholderText("https://...");
    usernameEdit->setPlaceholderText("email@example.com");
    passwordEdit->setPlaceholderText("App-specific password");

    form->addWidget(new QLabel("Label"), 0, 0);
    form->addWidget(nameEdit, 1, 0);
    form->addWidget(new QLabel("Server URL"), 0, 1);
    form->addWidget(serverEdit, 1, 1);
    form->addWidget(new QLabel("Username"), 2, 0);
    form->addWidget(usernameEdit, 3, 0);
    form->addWidget(new QLabel("Password"), 2, 1);
    form->addWidget(passwordEdit, 3, 1);
    caldavFormLayout->addLayout(form);

    auto* caldavHint = new QLabel("iCloud and Yahoo require app-specific passwords, which you can generate in your provider's account settings. Credentials stay securely on this Mac.");
    caldavHint->setWordWrap(true);
    caldavFormLayout->addWidget(caldavHint);

    auto* caldavFormButtons = new QHBoxLayout;
    caldavFormButtons->setSpacing(Theme::Space::S);
    auto* saveCalDAV = new QPushButton("Save and Connect");
    saveCalDAV->setObjectName("accentBtn");
    auto* clearCalDAV = new QPushButton("Clear");
    clearCalDAV->setObjectName("subtleBtn");
    auto* formStatus = new QLabel;
    formStatus->setWordWrap(true);
    caldavFormButtons->addWidget(saveCalDAV);
    caldavFormButtons->addWidget(clearCalDAV);
    caldavFormButtons->addStretch();
    caldavFormLayout->addLayout(caldavFormButtons);
    caldavFormLayout->addWidget(formStatus);

    lay->addWidget(caldavFormCard);

    auto* caldavCardsContainer = new QWidget;
    auto* caldavCardsLayout = new QVBoxLayout(caldavCardsContainer);
    caldavCardsLayout->setContentsMargins(0, 0, 0, 0);
    caldavCardsLayout->setSpacing(Theme::Space::M);
    lay->addWidget(caldavCardsContainer);
    lay->addStretch();

    auto findProviderById = [this](const QString& accountId) -> CalDAVProvider* {
        if (!m_caldavProviders)
            return nullptr;
        for (CalDAVProvider* provider : *m_caldavProviders) {
            if (provider && provider->accountId() == accountId)
                return provider;
        }
        return nullptr;
    };

    auto clearForm = [nameEdit, serverEdit, usernameEdit, passwordEdit, formStatus]() {
        nameEdit->clear();
        serverEdit->clear();
        usernameEdit->clear();
        passwordEdit->clear();
        formStatus->clear();
    };

    auto* editingId = new QString;

    auto rebuildCalDAVCards = std::make_shared<std::function<void()>>();
    *rebuildCalDAVCards = [this, caldavCardsLayout, colors, nameEdit, serverEdit, usernameEdit, passwordEdit,
                           formStatus, editingId, clearForm, findProviderById, rebuildCalDAVCards]() {
        while (QLayoutItem* item = caldavCardsLayout->takeAt(0)) {
            if (QWidget* widget = item->widget())
                widget->deleteLater();
            delete item;
        }

        const QList<CalDAVProvider::AccountConfig> accounts = CalDAVProvider::loadAccounts();
        if (accounts.isEmpty()) {
            auto* emptyCard = buildCard();
            auto* emptyLay = new QVBoxLayout(emptyCard);
            emptyLay->setContentsMargins(Theme::Space::XL, Theme::Space::XL,
                                         Theme::Space::XL, Theme::Space::XL);
            auto* emptyTitle = new QLabel("No CalDAV accounts yet");
            emptyTitle->setFont(Theme::Font::subheading());
            auto* emptyCopy = new QLabel("Add your first account above to start syncing with iCloud, Yahoo, Fastmail, or any CalDAV server.");
            emptyCopy->setWordWrap(true);
            emptyLay->addWidget(emptyTitle);
            emptyLay->addWidget(emptyCopy);
            caldavCardsLayout->addWidget(emptyCard);
            return;
        }

        for (const auto& config : accounts) {
            auto* provider = findProviderById(config.id);
            auto* card = buildCard();
            auto* cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(Theme::Space::XL, Theme::Space::XL,
                                           Theme::Space::XL, Theme::Space::XL);
            cardLayout->setSpacing(Theme::Space::M);

            auto* topRow = new QHBoxLayout;
            auto* heading = new QLabel(config.name.isEmpty() ? config.username : config.name);
            heading->setFont(Theme::Font::subheading());
            auto* chip = new QLabel;
            styleStateChip(chip, colors, provider);
            topRow->addWidget(heading);
            topRow->addStretch();
            topRow->addWidget(chip);
            cardLayout->addLayout(topRow);

            auto* detail = new QLabel(
                QString("%1\n%2").arg(config.username, config.serverUrl));
            detail->setWordWrap(true);
            cardLayout->addWidget(detail);

            auto* buttonRow = new QHBoxLayout;
            buttonRow->setSpacing(Theme::Space::S);
            auto* connectBtn = new QPushButton(provider && provider->isAuthenticated() ? "Reconnect" : "Connect");
            connectBtn->setObjectName("accentBtn");
            auto* editBtn = new QPushButton("Edit");
            editBtn->setObjectName("subtleBtn");
            auto* removeBtn = new QPushButton("Remove");
            removeBtn->setObjectName("dangerBtn");

            auto refresh = [provider, chip, detail, config, colors, connectBtn]() {
                styleStateChip(chip, colors, provider);
                QString lines = QString("%1\n%2").arg(config.username, config.serverUrl);
                if (provider)
                    lines.append("\n" + providerSecondaryText(provider));
                detail->setText(lines);
                const bool busy = provider && (provider->connectionState() == CalendarProvider::Connecting
                                            || provider->connectionState() == CalendarProvider::Syncing);
                connectBtn->setText(busy ? "Working..." : provider && provider->isAuthenticated() ? "Reconnect" : "Connect");
                connectBtn->setEnabled(!busy);
            };

            refresh();
            if (provider) {
                connect(provider, &CalendarProvider::connectionStateChanged, this,
                        [refresh](CalendarProvider::ConnectionState) { refresh(); });
                connect(provider, &CalendarProvider::authenticationComplete, this,
                        [refresh](bool, const QString&) { refresh(); });
            }

            connect(connectBtn, &QPushButton::clicked, this, [this, config, formStatus, findProviderById] {
                emit accountsChanged();
                QTimer::singleShot(0, this, [this, config, formStatus, findProviderById] {
                    if (auto* provider = findProviderById(config.id)) {
                        provider->authenticate();
                        formStatus->setText(QString("Connecting %1…").arg(provider->accountDisplayName()));
                    } else {
                        formStatus->setText("Account saved. Reopen Settings to continue connecting.");
                    }
                });
            });

            connect(editBtn, &QPushButton::clicked, this, [config, nameEdit, serverEdit, usernameEdit, passwordEdit, editingId, formStatus] {
                *editingId = config.id;
                nameEdit->setText(config.name);
                serverEdit->setText(config.serverUrl);
                usernameEdit->setText(config.username);
                passwordEdit->setText(config.password);
                formStatus->setText("Editing this account. Make your changes and save to reconnect.");
            });

            connect(removeBtn, &QPushButton::clicked, this, [this, config, findProviderById, editingId, clearForm, rebuildCalDAVCards] {
                if (auto* provider = findProviderById(config.id))
                    provider->signOut();
                CalDAVProvider::removeAccount(config.id);
                if (*editingId == config.id) {
                    editingId->clear();
                    clearForm();
                }
                emit accountsChanged();
                QTimer::singleShot(0, this, [rebuildCalDAVCards] { (*rebuildCalDAVCards)(); });
            });

            buttonRow->addWidget(connectBtn);
            buttonRow->addWidget(editBtn);
            buttonRow->addWidget(removeBtn);
            buttonRow->addStretch();
            cardLayout->addLayout(buttonRow);
            caldavCardsLayout->addWidget(card);
        }
    };

    connect(iCloudPreset, &QPushButton::clicked, this, [nameEdit, serverEdit] {
        if (nameEdit->text().trimmed().isEmpty())
            nameEdit->setText("iCloud");
        serverEdit->setText("https://caldav.icloud.com");
    });
    connect(yahooPreset, &QPushButton::clicked, this, [nameEdit, serverEdit] {
        if (nameEdit->text().trimmed().isEmpty())
            nameEdit->setText("Yahoo");
        serverEdit->setText("https://caldav.calendar.yahoo.com");
    });
    connect(fastmailPreset, &QPushButton::clicked, this, [nameEdit, serverEdit] {
        if (nameEdit->text().trimmed().isEmpty())
            nameEdit->setText("Fastmail");
        serverEdit->setText("https://caldav.fastmail.com/dav/calendars/user/");
    });

    connect(clearCalDAV, &QPushButton::clicked, this, [editingId, clearForm] {
        editingId->clear();
        clearForm();
    });

    connect(saveCalDAV, &QPushButton::clicked, this, [this, nameEdit, serverEdit, usernameEdit, passwordEdit,
                                                       formStatus, editingId, clearForm, findProviderById, rebuildCalDAVCards] {
        const QString server = serverEdit->text().trimmed();
        const QString username = usernameEdit->text().trimmed();
        const QString password = passwordEdit->text();
        if (server.isEmpty() || username.isEmpty() || password.isEmpty()) {
            formStatus->setText("Please fill in the server URL, username, and password to continue.");
            return;
        }

        CalDAVProvider::AccountConfig updated;
        updated.id = editingId->isEmpty() ? CalDAVProvider::createAccountId() : *editingId;
        updated.name = nameEdit->text().trimmed();
        updated.serverUrl = server;
        updated.username = username;
        updated.password = password;

        const QList<CalDAVProvider::AccountConfig> existingAccounts = CalDAVProvider::loadAccounts();
        for (const auto& account : existingAccounts) {
            if (account.id != updated.id)
                continue;
            if (account.serverUrl == updated.serverUrl
                && account.username == updated.username
                && account.password == updated.password) {
                updated.principalUrl = account.principalUrl;
                updated.calendarHomePath = account.calendarHomePath;
                updated.authenticated = account.authenticated;
            }
            break;
        }

        CalDAVProvider::upsertAccount(updated);
        emit accountsChanged();
        formStatus->setText("Saved. Connecting your account…");
        const QString savedId = updated.id;
        editingId->clear();
        clearForm();

        QTimer::singleShot(0, this, [this, savedId, formStatus, findProviderById, rebuildCalDAVCards] {
            (*rebuildCalDAVCards)();
            if (auto* provider = findProviderById(savedId)) {
                provider->authenticate();
                formStatus->setText(QString("Connecting %1…").arg(provider->accountDisplayName()));
            } else {
                formStatus->setText("Account saved. Reopen Settings to finish connecting.");
            }
        });
    });

    (*rebuildCalDAVCards)();
    scroll->setWidget(content);
    return page;
}

QWidget* SettingsDialog::buildAIPage() {
    auto* page = new QWidget;
    page->setObjectName("settingsPage");
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(Theme::Space::XXL, Theme::Space::XXL,
                            Theme::Space::XXL, Theme::Space::XL);
    lay->setSpacing(Theme::Space::L);

    const Theme::Colors colors = m_isDark ? Theme::dark() : Theme::light();

    auto* eyebrow = new QLabel("ASSISTANT");
    eyebrow->setFont(Theme::Font::caption());
    lay->addWidget(eyebrow);

    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(Theme::Space::M);
    auto* title = new QLabel("Assistant");
    title->setFont(Theme::Font::heading());
    auto* statusChip = new QLabel;
    headerRow->addWidget(title);
    headerRow->addSpacing(Theme::Space::S);
    headerRow->addWidget(statusChip, 0, Qt::AlignVCenter);
    headerRow->addStretch();
    lay->addLayout(headerRow);

    auto refreshStatusChip = [statusChip, colors](bool active) {
        const QColor fg = active ? colors.success : colors.textTertiary;
        QColor bg = fg; bg.setAlpha(active ? 36 : 22);
        statusChip->setText(active ? "Active" : "Off");
        statusChip->setStyleSheet(QString(
            "QLabel {"
            " background: %1;"
            " color: %2;"
            " border-radius: 999px;"
            " padding: 5px 11px;"
            " font-size: 11px;"
            " font-weight: 700;"
            "}"
        ).arg(bg.name(QColor::HexArgb), fg.name()));
    };
    refreshStatusChip(m_llm && m_llm->isAvailable());

    auto* desc = new QLabel("Bring your own model to power smarter daily summaries, balance suggestions, and conflict-aware planning. Works with OpenAI, Anthropic via proxy, or any OpenAI-compatible endpoint — including local models.");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* card = buildCard();
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(Theme::Space::XL, Theme::Space::XL,
                                Theme::Space::XL, Theme::Space::XL);
    cardLay->setSpacing(Theme::Space::L);

    auto* cardHeading = new QLabel("Provider settings");
    cardHeading->setFont(Theme::Font::subheading());
    cardLay->addWidget(cardHeading);

    auto* form = new QFormLayout;
    form->setSpacing(Theme::Space::M);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* endpoint = new QLineEdit(m_llm ? m_llm->endpoint() : QString());
    endpoint->setPlaceholderText("https://api.openai.com/v1/chat/completions");
    auto* model = new QLineEdit(m_llm ? m_llm->model() : QString());
    model->setPlaceholderText("gpt-4o-mini");
    auto* apiKey = new QLineEdit(m_llm ? m_llm->apiKey() : QString());
    apiKey->setEchoMode(QLineEdit::Password);
    apiKey->setPlaceholderText("Paste your API key");

    form->addRow("Endpoint", endpoint);
    form->addRow("Model", model);
    form->addRow("API key", apiKey);
    cardLay->addLayout(form);

    auto* privacyNote = new QLabel("EduSync sends only the day's event titles, times, and categories to your provider. Notes and descriptions stay on this Mac. Your API key is stored locally and never shared.");
    privacyNote->setWordWrap(true);
    cardLay->addWidget(privacyNote);

    auto* save = new QPushButton("Save Assistant Settings");
    save->setObjectName("accentBtn");
    connect(save, &QPushButton::clicked, this, [=] {
        if (!m_llm)
            return;
        m_llm->setEndpoint(endpoint->text().trimmed());
        m_llm->setModel(model->text().trimmed());
        m_llm->setApiKey(apiKey->text().trimmed());
        m_llm->saveSettings();
        refreshStatusChip(m_llm->isAvailable());
        save->setText("Saved");
        QTimer::singleShot(1200, this, [save] { save->setText("Save Assistant Settings"); });
    });
    cardLay->addWidget(save, 0, Qt::AlignLeft);

    lay->addWidget(card);
    lay->addStretch();
    return page;
}

QWidget* SettingsDialog::buildAdvancedPage() {
    auto* page = new QWidget;
    page->setObjectName("settingsPage");
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(Theme::Space::XXL, Theme::Space::XXL,
                            Theme::Space::XXL, Theme::Space::XL);
    lay->setSpacing(Theme::Space::L);

    auto* eyebrow = new QLabel("CREDENTIALS");
    eyebrow->setFont(Theme::Font::caption());
    lay->addWidget(eyebrow);

    auto* title = new QLabel("Advanced");
    title->setFont(Theme::Font::heading());
    lay->addWidget(title);

    auto* desc = new QLabel("Bring your own OAuth credentials to connect EduSync to Google or Outlook through your organization's workspace or a personal developer project. Most people can skip this — the Accounts page handles everything out of the box.");
    desc->setWordWrap(true);
    lay->addWidget(desc);

    auto* card = buildCard();
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(Theme::Space::XL, Theme::Space::XL,
                                Theme::Space::XL, Theme::Space::XL);
    cardLay->setSpacing(Theme::Space::L);

    auto* cardHeading = new QLabel("Custom integration credentials");
    cardHeading->setFont(Theme::Font::subheading());
    cardLay->addWidget(cardHeading);

    auto* form = new QFormLayout;
    form->setSpacing(Theme::Space::S);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* googleId = new QLineEdit;
    auto* googleSecret = new QLineEdit;
    googleId->setPlaceholderText("Google client ID");
    googleSecret->setPlaceholderText("Google client secret");

    auto* outlookId = new QLineEdit;
    outlookId->setPlaceholderText("Outlook application ID");

    if (Credentials::isGoogleConfigured()) {
        googleId->setText(Credentials::googleClientId());
        googleSecret->setText("••••••••");
    }
    if (Credentials::isOutlookConfigured())
        outlookId->setText(Credentials::outlookClientId());

    form->addRow("Google client ID", googleId);
    form->addRow("Google secret", googleSecret);
    form->addRow("Outlook app ID", outlookId);
    cardLay->addLayout(form);

    auto* note = new QLabel("Credentials are stored securely on this Mac and used only when you connect from the Accounts page.");
    note->setWordWrap(true);
    cardLay->addWidget(note);

    auto* save = new QPushButton("Save Integration Credentials");
    save->setObjectName("subtleBtn");
    connect(save, &QPushButton::clicked, this, [=] {
        if (!googleId->text().trimmed().isEmpty() && googleSecret->text().trimmed() != "••••••••")
            Credentials::saveGoogleCredentials(googleId->text().trimmed(), googleSecret->text().trimmed());
        if (!outlookId->text().trimmed().isEmpty())
            Credentials::saveOutlookCredentials(outlookId->text().trimmed());
        save->setText("Saved");
        QTimer::singleShot(1200, this, [save] { save->setText("Save Integration Credentials"); });
    });
    cardLay->addWidget(save, 0, Qt::AlignLeft);

    lay->addWidget(card);
    lay->addStretch();
    return page;
}

void SettingsDialog::buildSidebar() {
}
