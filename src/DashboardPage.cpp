#include "DashboardPage.h"
#include "PlatformFonts.h"
#include "core/AppSettings.h"
#include "core/FontManager.h"
#include "core/SessionManager.h"
#include "ui/Motion.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QInputDialog>
#include <QMessageBox>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QToolButton* makeSidebarNav(const QString& iconPath, const QString& text, QWidget* parent)
{
    auto* btn = new Motion::HoverFillButton(parent);
    btn->setText(text);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(14, 14));
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setCheckable(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setObjectName(QStringLiteral("sideNavBtn"));
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setFixedHeight(28);
    btn->setHoverFill(QColor(0x2c, 0x2c, 0x2c));
    return btn;
}

QToolButton* makeRowIcon(const QString& iconPath, const QString& tip, QWidget* parent)
{
    auto* btn = new Motion::HoverFillButton(parent);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(13, 13));
    btn->setToolTip(tip);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setObjectName(QStringLiteral("dashRowAction"));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(22, 22);
    btn->setHoverFill(QColor(0x3c, 0x3c, 0x3c));
    return btn;
}

void styleTable(QTableWidget* table)
{
    table->setObjectName(QStringLiteral("dashTable"));
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(34);
    table->horizontalHeader()->setStretchLastSection(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setShowGrid(false);
    table->setFocusPolicy(Qt::NoFocus);
    table->setAlternatingRowColors(false);
    table->setWordWrap(false);
    table->setIconSize(QSize(14, 14));
}
}

DashboardPage::DashboardPage(SessionManager* sessions, QWidget* parent)
    : QWidget(parent)
    , m_sessions(sessions)
{
    setObjectName(QStringLiteral("dashboardPage"));
    m_profiles = loadProfiles();

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- Sidebar ----
    m_sidebar = new QWidget(this);
    m_sidebar->setObjectName(QStringLiteral("dashSidebar"));
    m_sidebar->setFixedWidth(168);
    auto* sideLay = new QVBoxLayout(m_sidebar);
    sideLay->setContentsMargins(8, 12, 8, 10);
    sideLay->setSpacing(2);

    auto* brand = new QLabel(QStringLiteral("clientosh"), m_sidebar);
    brand->setObjectName(QStringLiteral("sideBrand"));
    sideLay->addWidget(brand);

    auto* brandSub = new QLabel(QStringLiteral("ssh client"), m_sidebar);
    brandSub->setObjectName(QStringLiteral("sideBrandSub"));
    sideLay->addWidget(brandSub);
    sideLay->addSpacing(12);

    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);

    m_navHosts = makeSidebarNav(QStringLiteral(":/icons/hosts.svg"), QStringLiteral("Hosts"), m_sidebar);
    m_navKeys = makeSidebarNav(QStringLiteral(":/icons/key.svg"), QStringLiteral("Keychain"), m_sidebar);
    m_navLogs = makeSidebarNav(QStringLiteral(":/icons/logs.svg"), QStringLiteral("Logs"), m_sidebar);
    m_navSettings = makeSidebarNav(QStringLiteral(":/icons/settings.svg"), QStringLiteral("Settings"), m_sidebar);

    m_navGroup->addButton(m_navHosts, static_cast<int>(NavPage::Hosts));
    m_navGroup->addButton(m_navKeys, static_cast<int>(NavPage::Keychain));
    m_navGroup->addButton(m_navLogs, static_cast<int>(NavPage::Logs));
    m_navGroup->addButton(m_navSettings, static_cast<int>(NavPage::Settings));

    sideLay->addWidget(m_navHosts);

    m_activeBadge = new QLabel(QStringLiteral(""), m_sidebar);
    m_activeBadge->setObjectName(QStringLiteral("sideBadge"));
    m_activeBadge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_activeBadge->hide();
    sideLay->addWidget(m_activeBadge);

    sideLay->addSpacing(6);
    auto* sideRule = new QFrame(m_sidebar);
    sideRule->setObjectName(QStringLiteral("sideRule"));
    sideRule->setFrameShape(QFrame::HLine);
    sideRule->setFixedHeight(1);
    sideLay->addWidget(sideRule);
    sideLay->addSpacing(6);

    sideLay->addWidget(m_navKeys);
    sideLay->addWidget(m_navLogs);
    sideLay->addStretch(1);
    sideLay->addWidget(m_navSettings);

    root->addWidget(m_sidebar);

    // ---- Main column ----
    auto* mainCol = new QWidget(this);
    mainCol->setObjectName(QStringLiteral("dashMain"));
    auto* mainLay = new QVBoxLayout(mainCol);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->setSpacing(0);

    m_topBar = new QWidget(mainCol);
    m_topBar->setObjectName(QStringLiteral("dashTopBar"));
    m_topBar->setFixedHeight(44);
    auto* topLay = new QHBoxLayout(m_topBar);
    topLay->setContentsMargins(16, 6, 16, 6);
    topLay->setSpacing(10);

    auto* titleCol = new QVBoxLayout;
    titleCol->setContentsMargins(0, 0, 0, 0);
    titleCol->setSpacing(0);
    m_pageTitle = new QLabel(QStringLiteral("Hosts"), m_topBar);
    m_pageTitle->setObjectName(QStringLiteral("dashPageTitle"));
    m_pageSub = new QLabel(QStringLiteral("saved connection profiles"), m_topBar);
    m_pageSub->setObjectName(QStringLiteral("dashPageSub"));
    titleCol->addWidget(m_pageTitle);
    titleCol->addWidget(m_pageSub);
    topLay->addLayout(titleCol);
    topLay->addStretch(1);

    m_searchEdit = new QLineEdit(m_topBar);
    m_searchEdit->setObjectName(QStringLiteral("dashSearch"));
    m_searchEdit->setPlaceholderText(QStringLiteral("filter hosts…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(200);
    m_searchEdit->setMaximumWidth(320);
    topLay->addWidget(m_searchEdit, 1);

    m_newSessionBtn = new QPushButton(QIcon(QStringLiteral(":/icons/plus.svg")),
                                      QStringLiteral("New Session"), m_topBar);
    m_newSessionBtn->setObjectName(QStringLiteral("dashPrimary"));
    m_newSessionBtn->setIconSize(QSize(13, 13));
    m_newSessionBtn->setFocusPolicy(Qt::NoFocus);
    topLay->addWidget(m_newSessionBtn);

    mainLay->addWidget(m_topBar);

    auto* topRule = new QFrame(mainCol);
    topRule->setObjectName(QStringLiteral("dashTabLine"));
    topRule->setFrameShape(QFrame::HLine);
    topRule->setFixedHeight(1);
    mainLay->addWidget(topRule);

    m_stack = new QStackedWidget(mainCol);
    mainLay->addWidget(m_stack, 1);

    // ---- Hosts page ----
    m_hostsPage = new QWidget;
    auto* hostsLay = new QVBoxLayout(m_hostsPage);
    hostsLay->setContentsMargins(16, 12, 16, 10);
    hostsLay->setSpacing(8);

    m_savedTable = new QTableWidget(0, 4, m_hostsPage);
    styleTable(m_savedTable);
    m_savedTable->setHorizontalHeaderLabels(
        {QStringLiteral("name"), QStringLiteral("type"), QStringLiteral("auth"), QStringLiteral("")});
    m_savedTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_savedTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_savedTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_savedTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_savedTable->setColumnWidth(3, 112);
    hostsLay->addWidget(m_savedTable, 1);

    m_savedEmpty = new QLabel(QStringLiteral("no hosts yet — create a session to get started"), m_hostsPage);
    m_savedEmpty->setObjectName(QStringLiteral("dashHint"));
    m_savedEmpty->setAlignment(Qt::AlignCenter);
    m_savedEmpty->hide();
    hostsLay->addWidget(m_savedEmpty);

    m_hint = new QLabel(QStringLiteral(""), m_hostsPage);
    m_hint->setObjectName(QStringLiteral("dashHint"));
    hostsLay->addWidget(m_hint);
    m_stack->addWidget(m_hostsPage);

    // ---- Keychain page ----
    m_keysPage = new QWidget;
    auto* keysLay = new QVBoxLayout(m_keysPage);
    keysLay->setContentsMargins(16, 12, 16, 10);
    keysLay->setSpacing(8);

    auto* keysHint = new QLabel(
        QStringLiteral("stored credentials by profile — secrets are never shown here"), m_keysPage);
    keysHint->setObjectName(QStringLiteral("dashHint"));
    keysLay->addWidget(keysHint);

    m_keysTable = new QTableWidget(0, 3, m_keysPage);
    styleTable(m_keysTable);
    m_keysTable->setHorizontalHeaderLabels(
        {QStringLiteral("profile"), QStringLiteral("password"), QStringLiteral("private key")});
    m_keysTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_keysTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_keysTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    keysLay->addWidget(m_keysTable, 1);

    m_keysEmpty = new QLabel(QStringLiteral("no stored credentials"), m_keysPage);
    m_keysEmpty->setObjectName(QStringLiteral("dashHint"));
    m_keysEmpty->setAlignment(Qt::AlignCenter);
    keysLay->addWidget(m_keysEmpty);
    m_stack->addWidget(m_keysPage);

    // ---- Logs page ----
    m_logsPage = new QWidget;
    auto* logsLay = new QVBoxLayout(m_logsPage);
    logsLay->setContentsMargins(16, 12, 16, 10);
    logsLay->setSpacing(8);

    m_logsView = new QPlainTextEdit(m_logsPage);
    m_logsView->setObjectName(QStringLiteral("dashLogs"));
    m_logsView->setReadOnly(true);
    m_logsView->setMaximumBlockCount(500);
    m_logsView->setPlaceholderText(QStringLiteral("session events appear here…"));
    logsLay->addWidget(m_logsView, 1);

    auto* clearLogs = new QPushButton(QStringLiteral("clear"), m_logsPage);
    clearLogs->setObjectName(QStringLiteral("dashButton"));
    clearLogs->setFocusPolicy(Qt::NoFocus);
    clearLogs->setFixedWidth(72);
    auto* logsRow = new QHBoxLayout;
    logsRow->addStretch(1);
    logsRow->addWidget(clearLogs);
    logsLay->addLayout(logsRow);
    m_stack->addWidget(m_logsPage);

    // ---- Settings page (categorized) ----
    m_settingsPage = new QWidget;
    auto* setOuter = new QVBoxLayout(m_settingsPage);
    setOuter->setContentsMargins(0, 0, 0, 0);
    setOuter->setSpacing(0);

    auto* setBody = new QWidget(m_settingsPage);
    auto* setBodyLay = new QHBoxLayout(setBody);
    setBodyLay->setContentsMargins(0, 0, 0, 0);
    setBodyLay->setSpacing(0);

    m_settingsNav = new QListWidget(setBody);
    m_settingsNav->setObjectName(QStringLiteral("settingsNav"));
    m_settingsNav->setFixedWidth(148);
    m_settingsNav->setFocusPolicy(Qt::NoFocus);
    m_settingsNav->setSpacing(1);
    m_settingsNav->addItems({QStringLiteral("General"), QStringLiteral("Appearance"),
                             QStringLiteral("Performance"), QStringLiteral("SSH / Sessions"),
                             QStringLiteral("SFTP"), QStringLiteral("Shortcuts")});
    m_settingsNav->setCurrentRow(0);

    m_settingsStack = new QStackedWidget(setBody);

    auto makeScrollPage = [&](QWidget* section) {
        auto* page = new QWidget;
        auto* pageLay = new QVBoxLayout(page);
        pageLay->setContentsMargins(0, 0, 0, 0);
        pageLay->setSpacing(0);

        auto* scroll = new QScrollArea(page);
        scroll->setObjectName(QStringLiteral("settingsScroll"));
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(section);

        pageLay->addWidget(scroll);
        m_settingsStack->addWidget(page);
    };

    // General
    {
        QWidget* sec = buildSettingsSection(QStringLiteral("General"), m_settingsStack);
        m_settingsSavePassDefault = new QCheckBox(QStringLiteral("Save passwords by default for new profiles"), sec);
        m_settingsDefaultHost = new QLineEdit(sec);
        m_settingsDefaultHost->setPlaceholderText(QStringLiteral("127.0.0.1"));
        m_settingsDefaultUser = new QLineEdit(sec);
        m_settingsDefaultUser->setPlaceholderText(QStringLiteral("optional"));
        auto* gLay = qobject_cast<QVBoxLayout*>(sec->layout());
        gLay->addWidget(m_settingsSavePassDefault);
        addSettingsField(sec, QStringLiteral("Default host"), m_settingsDefaultHost);
        addSettingsField(sec, QStringLiteral("Default username"), m_settingsDefaultUser);
        makeScrollPage(sec);
    }

    // Appearance
    {
        QWidget* sec = buildSettingsSection(QStringLiteral("Appearance / UI"), m_settingsStack);
        m_settingsTheme = new QComboBox(sec);
        m_settingsTheme->addItem(QStringLiteral("Dark"), QStringLiteral("dark"));
        m_settingsTheme->addItem(QStringLiteral("Light"), QStringLiteral("light"));

        m_settingsUiFontFamily = new QComboBox(sec);
        m_settingsUiFontSize = new QSpinBox(sec);
        m_settingsUiFontSize->setRange(9, 22);
        m_settingsUiFontSize->setSuffix(QStringLiteral(" pt"));

        m_settingsFontFamily = new QComboBox(sec);
        m_settingsFontSize = new QSpinBox(sec);
        m_settingsFontSize->setRange(9, 22);
        m_settingsFontSize->setSuffix(QStringLiteral(" pt"));

        m_settingsTermFgBtn = new QToolButton(sec);
        m_settingsTermFgBtn->setObjectName(QStringLiteral("colorSwatchBtn"));
        m_settingsTermFgBtn->setFixedSize(72, 24);
        m_settingsTermFgBtn->setCursor(Qt::PointingHandCursor);
        m_settingsTermFgBtn->setFocusPolicy(Qt::NoFocus);
        m_settingsTermBgBtn = new QToolButton(sec);
        m_settingsTermBgBtn->setObjectName(QStringLiteral("colorSwatchBtn"));
        m_settingsTermBgBtn->setFixedSize(72, 24);
        m_settingsTermBgBtn->setCursor(Qt::PointingHandCursor);
        m_settingsTermBgBtn->setFocusPolicy(Qt::NoFocus);

        m_settingsTermBgImageBtn = new QToolButton(sec);
        m_settingsTermBgImageBtn->setObjectName(QStringLiteral("dashSecondary"));
        m_settingsTermBgImageBtn->setText(QStringLiteral("Choose…"));
        m_settingsTermBgImageBtn->setCursor(Qt::PointingHandCursor);
        m_settingsTermBgImageBtn->setFocusPolicy(Qt::NoFocus);
        m_settingsTermBgImage = new QLabel(sec);
        m_settingsTermBgImage->setObjectName(QStringLiteral("dashHint"));
        m_settingsTermBgImage->setWordWrap(true);
        m_settingsTermBgImage->setMinimumHeight(24);
        m_settingsTermBgOpacity = new QSpinBox(sec);
        m_settingsTermBgOpacity->setRange(0, 100);
        m_settingsTermBgOpacity->setSuffix(QStringLiteral("%"));
        m_settingsTermBgOpacity->setSingleStep(5);
        m_settingsTermBgBlur = new QSpinBox(sec);
        m_settingsTermBgBlur->setRange(0, 100);
        m_settingsTermBgBlur->setSuffix(QStringLiteral(" px"));
        m_settingsTermBgBlur->setSingleStep(5);

        addSettingsField(sec, QStringLiteral("Theme"), m_settingsTheme);
        addSettingsField(sec, QStringLiteral("UI font"), m_settingsUiFontFamily);
        addSettingsField(sec, QStringLiteral("UI font size"), m_settingsUiFontSize);

            auto* secLay = qobject_cast<QVBoxLayout*>(sec->layout());
        auto* guiSub = new QLabel(QStringLiteral("GUI appearance"), sec);
        guiSub->setObjectName(QStringLiteral("settingsSubsection"));
        secLay->addSpacing(2);
        secLay->addWidget(guiSub);

        auto* terminalSub = new QLabel(QStringLiteral("Terminal appearance"), sec);
        terminalSub->setObjectName(QStringLiteral("settingsSubsection"));
        auto* terminalDiv = new QFrame(sec);
        terminalDiv->setObjectName(QStringLiteral("settingsDivider"));
        terminalDiv->setFrameShape(QFrame::HLine);
        secLay->addWidget(terminalSub);
        secLay->addWidget(terminalDiv);
        secLay->addSpacing(2);

        addSettingsField(sec, QStringLiteral("Terminal font"), m_settingsFontFamily);
        addSettingsField(sec, QStringLiteral("Terminal font size"), m_settingsFontSize);
        addSettingsField(sec, QStringLiteral("Terminal text color"), m_settingsTermFgBtn);
        addSettingsField(sec, QStringLiteral("Terminal background"), m_settingsTermBgBtn);
        addSettingsField(sec, QStringLiteral("Terminal background image"), m_settingsTermBgImage);
        addSettingsField(sec, QStringLiteral(""), m_settingsTermBgImageBtn);
        addSettingsField(sec, QStringLiteral("Image opacity"), m_settingsTermBgOpacity);
        addSettingsField(sec, QStringLiteral("Image blur radius"), m_settingsTermBgBlur);

        auto* uiPreview = new QLabel(QStringLiteral("UI preview: Hosts · Settings · Connect"), sec);
        uiPreview->setObjectName(QStringLiteral("settingsFontPreview"));
        m_settingsTermPreview = new QLabel(QStringLiteral("Terminal preview: Abc 123 → ~/src $"), sec);
        m_settingsTermPreview->setObjectName(QStringLiteral("settingsTermPreview"));
        m_settingsTermPreview->setContentsMargins(8, 8, 8, 8);

        m_settingsFontStatus = new QLabel(QStringLiteral(""), sec);
        m_settingsFontStatus->setObjectName(QStringLiteral("dashHint"));
        m_settingsFontStatus->setWordWrap(true);

        m_settingsHighlightAddresses = new QCheckBox(QStringLiteral("Colorize IP and MAC addresses"), sec);
        m_settingsHighlightKeywords = new QCheckBox(
            QStringLiteral("Colorize log keywords (ERROR, WARN, OK, INFO, DEBUG)"), sec);

        secLay->addWidget(uiPreview);
        secLay->addWidget(m_settingsTermPreview);
        secLay->addWidget(m_settingsFontStatus);
        secLay->addSpacing(8);
        secLay->addWidget(m_settingsHighlightAddresses);
        secLay->addWidget(m_settingsHighlightKeywords);
        secLay->addSpacing(8);
        auto* resetAppearanceBtn = new QPushButton(QStringLiteral("Reset appearance to defaults"), sec);
        resetAppearanceBtn->setObjectName(QStringLiteral("dashSecondary"));
        connect(resetAppearanceBtn, &QPushButton::clicked, this, [this]() {
            AppSettings::resetTerminalAppearance();
            loadSettingsUi();
            emit settingsApplied();
        });
        secLay->addWidget(resetAppearanceBtn);

        auto refreshPreviews = [this, uiPreview]() {
            uiPreview->setFont(
                clientoshUiFont(m_settingsUiFontSize->value(), m_settingsUiFontFamily->currentData().toString()));
            if (m_settingsTermPreview) {
                m_settingsTermPreview->setFont(
                    clientoshMonospaceFont(m_settingsFontSize->value(),
                                           m_settingsFontFamily->currentData().toString()));
                m_settingsTermPreview->setStyleSheet(
                    QStringLiteral("QLabel#settingsTermPreview { color: %1; background: %2; padding: 8px; }")
                        .arg(m_termFg.name(), m_termBg.name()));
            }
        };

        connect(m_settingsTheme, &QComboBox::currentIndexChanged, this, [this, refreshPreviews](int) {
            const bool light = m_settingsTheme->currentData().toString() == QLatin1String("light");
            m_termFg = AppSettings::defaultTerminalFgForTheme(light);
            m_termBg = AppSettings::defaultTerminalBgForTheme(light);
            syncColorSwatch(m_settingsTermFgBtn, m_termFg);
            syncColorSwatch(m_settingsTermBgBtn, m_termBg);
            refreshPreviews();
            persistAppearanceLive();
        });
        connect(m_settingsTermFgBtn, &QToolButton::clicked, this, [this]() { pickTerminalColor(true); });
        connect(m_settingsTermBgBtn, &QToolButton::clicked, this, [this]() { pickTerminalColor(false); });
        connect(m_settingsTermBgImageBtn, &QToolButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(this,
                QStringLiteral("Select background image"),
                QString(),
                QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp);;All files (*.*)"));
            if (!path.isEmpty()) {
                m_settingsTermBgImage->setText(QFileInfo(path).fileName());
                AppSettings::setTerminalBgImage(path);
                persistAppearanceLive();
            }
        });
        connect(m_settingsTermBgOpacity, qOverload<int>(&QSpinBox::valueChanged), this,
                [this](int value) {
                    AppSettings::setTerminalBgOpacity(qreal(value) / 100.0);
                    persistAppearanceLive();
                });
        connect(m_settingsTermBgBlur, qOverload<int>(&QSpinBox::valueChanged), this,
                [this](int value) {
                    AppSettings::setTerminalBgBlur(value);
                    persistAppearanceLive();
                });

        connect(m_settingsUiFontFamily, &QComboBox::currentIndexChanged, this, [this, refreshPreviews](int) {
            ensureSelectedFonts();
            refreshPreviews();
            persistAppearanceLive();
        });
        connect(m_settingsFontFamily, &QComboBox::currentIndexChanged, this, [this, refreshPreviews](int) {
            ensureSelectedFonts();
            refreshPreviews();
            persistAppearanceLive();
        });
        connect(m_settingsUiFontSize, qOverload<int>(&QSpinBox::valueChanged), this,
                [this, refreshPreviews](int) {
                    refreshPreviews();
                    persistAppearanceLive();
                });
        connect(m_settingsFontSize, qOverload<int>(&QSpinBox::valueChanged), this,
                [this, refreshPreviews](int) {
                    refreshPreviews();
                    persistAppearanceLive();
                });
        connect(m_settingsHighlightAddresses, &QCheckBox::toggled, this, [this](bool) {
            persistAppearanceLive();
        });
        connect(m_settingsHighlightKeywords, &QCheckBox::toggled, this, [this](bool) {
            persistAppearanceLive();
        });

        connect(FontManager::instance(), &FontManager::downloadStarted, this,
                [this](const QString& family) {
                    m_settingsFontStatus->setText(QStringLiteral("Downloading %1…").arg(family));
                });
        connect(FontManager::instance(), &FontManager::downloadProgress, this,
                [this](const QString& family, qint64 received, qint64 total) {
                    if (total > 0) {
                        m_settingsFontStatus->setText(
                            QStringLiteral("Downloading %1… %2%")
                                .arg(family)
                                .arg(int(100.0 * double(received) / double(total))));
                    }
                });
        connect(FontManager::instance(), &FontManager::fontReady, this,
                [this, refreshPreviews](const QString& family) {
                    if (!family.isEmpty()) {
                        m_settingsFontStatus->setText(QStringLiteral("%1 ready").arg(family));
                    } else {
                        m_settingsFontStatus->clear();
                    }
                    const QString ui = m_settingsUiFontFamily->currentData().toString();
                    const QString term = m_settingsFontFamily->currentData().toString();
                    populateFontCombos();
                    auto reselect = [](QComboBox* box, const QString& fam) {
                        const int idx = box->findData(fam);
                        box->blockSignals(true);
                        box->setCurrentIndex(idx >= 0 ? idx : 0);
                        box->blockSignals(false);
                    };
                    reselect(m_settingsUiFontFamily, ui);
                    reselect(m_settingsFontFamily, term);
                    refreshPreviews();
                });
        connect(FontManager::instance(), &FontManager::fontFailed, this,
                [this](const QString& family, const QString& error) {
                    m_settingsFontStatus->setText(QStringLiteral("%1: %2").arg(family, error));
                });

        populateFontCombos();
        m_termFg = AppSettings::terminalFg();
        m_termBg = AppSettings::terminalBg();
        syncColorSwatch(m_settingsTermFgBtn, m_termFg);
        syncColorSwatch(m_settingsTermBgBtn, m_termBg);
        refreshPreviews();
        makeScrollPage(sec);
    }

    // Performance
    {
        QWidget* sec = buildSettingsSection(QStringLiteral("Performance"), m_settingsStack);
        m_settingsAnimations = new QCheckBox(QStringLiteral("Enable UI animations"), sec);
        auto* animHint = new QLabel(
            QStringLiteral("Off = low memory mode — transitions snap and animation caches are released."),
            sec);
        animHint->setObjectName(QStringLiteral("dashHint"));
        animHint->setWordWrap(true);
        m_settingsShowStats = new QCheckBox(QStringLiteral("Show live server stats in session header"), sec);
        m_settingsStatsInterval = new QSpinBox(sec);
        m_settingsStatsInterval->setRange(1, 30);
        m_settingsStatsInterval->setSuffix(QStringLiteral(" s"));
        auto* pLay = qobject_cast<QVBoxLayout*>(sec->layout());
        pLay->addWidget(m_settingsAnimations);
        pLay->addWidget(animHint);
        pLay->addSpacing(6);
        pLay->addWidget(m_settingsShowStats);
        addSettingsField(sec, QStringLiteral("Server stats refresh interval"), m_settingsStatsInterval);
        makeScrollPage(sec);
    }

    // SSH / Sessions
    {
        QWidget* sec = buildSettingsSection(QStringLiteral("SSH / Sessions"), m_settingsStack);
        m_settingsDefaultPort = new QSpinBox(sec);
        m_settingsDefaultPort->setRange(1, 65535);
        addSettingsField(sec, QStringLiteral("Default SSH port"), m_settingsDefaultPort);
        auto* sshHint = new QLabel(QStringLiteral("Used when creating new session profiles."), sec);
        sshHint->setObjectName(QStringLiteral("dashHint"));
        qobject_cast<QVBoxLayout*>(sec->layout())->addWidget(sshHint);
        makeScrollPage(sec);
    }

    // SFTP
    {
        QWidget* sec = buildSettingsSection(QStringLiteral("SFTP"), m_settingsStack);
        m_settingsHideDotfiles = new QCheckBox(QStringLiteral("Hide dotfiles by default"), sec);
        m_settingsSftpView = new QComboBox(sec);
        m_settingsSftpView->addItem(QStringLiteral("Details (name, size, type)"), QStringLiteral("details"));
        m_settingsSftpView->addItem(QStringLiteral("Compact (name, size)"), QStringLiteral("compact"));
        m_settingsSftpVerbose = new QCheckBox(QStringLiteral("Verbose / debug logging for transfers and connection"), sec);
        auto* verboseHint = new QLabel(
            QStringLiteral("When enabled, SFTP writes detailed trace lines to Logs and to the console (qWarning). "
                           "Also enables libssh packet logging. Useful to diagnose upload/download failures — "
                           "turn off for normal use."),
            sec);
        verboseHint->setObjectName(QStringLiteral("dashHint"));
        verboseHint->setWordWrap(true);
        auto* sLay = qobject_cast<QVBoxLayout*>(sec->layout());
        sLay->addWidget(m_settingsHideDotfiles);
        addSettingsField(sec, QStringLiteral("Default file list view"), m_settingsSftpView);
        sLay->addSpacing(6);
        sLay->addWidget(m_settingsSftpVerbose);
        sLay->addWidget(verboseHint);
        connect(m_settingsSftpVerbose, &QCheckBox::toggled, this, [this](bool on) {
            AppSettings::setSftpVerboseLogging(on);
            emit settingsApplied();
            appendLog(on ? QStringLiteral("sftp verbose logging enabled")
                         : QStringLiteral("sftp verbose logging disabled"));
        });
        makeScrollPage(sec);
    }

    // Shortcuts
    {
        QWidget* sec = buildSettingsSection(QStringLiteral("Shortcuts"), m_settingsStack);
        m_settingsCtrlScrollZoom = new QCheckBox(
            QStringLiteral("Ctrl+Scroll zooms terminal font (when terminal is focused)"), sec);

        m_shortcutNewSession = new QKeySequenceEdit(sec);
        m_shortcutSettings = new QKeySequenceEdit(sec);
        m_shortcutDashboard = new QKeySequenceEdit(sec);
        m_shortcutClosePanel = new QKeySequenceEdit(sec);
        m_shortcutOpenSftp = new QKeySequenceEdit(sec);
        m_shortcutFontLarger = new QKeySequenceEdit(sec);
        m_shortcutFontSmaller = new QKeySequenceEdit(sec);
        m_shortcutFontReset = new QKeySequenceEdit(sec);

        m_enableNewSession = new QCheckBox(sec);
        m_enableSettings = new QCheckBox(sec);
        m_enableDashboard = new QCheckBox(sec);
        m_enableClosePanel = new QCheckBox(sec);
        m_enableOpenSftp = new QCheckBox(sec);
        m_enableFontLarger = new QCheckBox(sec);
        m_enableFontSmaller = new QCheckBox(sec);
        m_enableFontReset = new QCheckBox(sec);

        auto* sLay = qobject_cast<QVBoxLayout*>(sec->layout());
        sLay->addWidget(m_settingsCtrlScrollZoom);
        sLay->addSpacing(6);

        // Each shortcut gets an enable toggle next to its key field.
        auto addShortcutRow = [this, sLay](const QString& label, QKeySequenceEdit* edit, QCheckBox* enable) {
            auto* row = new QWidget(this);
            auto* rowLay = new QHBoxLayout(row);
            rowLay->setContentsMargins(0, 0, 0, 0);
            rowLay->setSpacing(8);
            auto* lab = new QLabel(label, row);
            lab->setObjectName(QStringLiteral("fieldLabel"));
            rowLay->addWidget(lab);
            rowLay->addStretch(1);
            rowLay->addWidget(enable);
            rowLay->addWidget(edit);
            edit->setFixedWidth(180);
            enable->setText(QStringLiteral("enabled"));
            enable->setFocusPolicy(Qt::NoFocus);

            connect(enable, &QCheckBox::toggled, this, [this, edit](bool on) {
                edit->setEnabled(on);
                if (!on) {
                    edit->clearFocus();
                }
                persistShortcutsLive();
            });
            sLay->addWidget(row);
        };
        addShortcutRow(QStringLiteral("New session"), m_shortcutNewSession, m_enableNewSession);
        addShortcutRow(QStringLiteral("Open settings"), m_shortcutSettings, m_enableSettings);
        addShortcutRow(QStringLiteral("Show dashboard"), m_shortcutDashboard, m_enableDashboard);
        addShortcutRow(QStringLiteral("Close active panel"), m_shortcutClosePanel, m_enableClosePanel);
        addShortcutRow(QStringLiteral("Open SFTP"), m_shortcutOpenSftp, m_enableOpenSftp);
        addShortcutRow(QStringLiteral("Terminal font larger"), m_shortcutFontLarger, m_enableFontLarger);
        addShortcutRow(QStringLiteral("Terminal font smaller"), m_shortcutFontSmaller, m_enableFontSmaller);
        addShortcutRow(QStringLiteral("Terminal font reset"), m_shortcutFontReset, m_enableFontReset);

        auto* resetBtn = new QPushButton(QStringLiteral("Reset shortcuts to defaults"), sec);
        resetBtn->setObjectName(QStringLiteral("dashButton"));
        resetBtn->setFocusPolicy(Qt::NoFocus);
        sLay->addSpacing(8);
        sLay->addWidget(resetBtn);

        auto* hint = new QLabel(
            QStringLiteral("Untick a shortcut to disable it. Click a field and press the new keys to rebind."),
            sec);
        hint->setObjectName(QStringLiteral("dashHint"));
        hint->setWordWrap(true);
        sLay->addWidget(hint);

        connect(m_settingsCtrlScrollZoom, &QCheckBox::toggled, this, [this](bool) {
            persistShortcutsLive();
        });
        const auto wireEdit = [this](QKeySequenceEdit* edit) {
            connect(edit, &QKeySequenceEdit::keySequenceChanged, this, [this](const QKeySequence&) {
                persistShortcutsLive();
            });
        };
        wireEdit(m_shortcutNewSession);
        wireEdit(m_shortcutSettings);
        wireEdit(m_shortcutDashboard);
        wireEdit(m_shortcutClosePanel);
        wireEdit(m_shortcutOpenSftp);
        wireEdit(m_shortcutFontLarger);
        wireEdit(m_shortcutFontSmaller);
        wireEdit(m_shortcutFontReset);
        connect(resetBtn, &QPushButton::clicked, this, &DashboardPage::resetShortcutsToDefaults);

        makeScrollPage(sec);
    }

    setBodyLay->addWidget(m_settingsNav);
    auto* setRule = new QFrame(setBody);
    setRule->setObjectName(QStringLiteral("sideRule"));
    setRule->setFrameShape(QFrame::VLine);
    setRule->setFixedWidth(1);
    setBodyLay->addWidget(setRule);
    setBodyLay->addWidget(m_settingsStack, 1);

    setOuter->addWidget(setBody, 1);

    auto* setFooter = new QWidget(m_settingsPage);
    setFooter->setObjectName(QStringLiteral("settingsFooter"));
    auto* setFootLay = new QHBoxLayout(setFooter);
    setFootLay->setContentsMargins(16, 8, 16, 10);
    setFootLay->setSpacing(8);
    auto* setSave = new QPushButton(QIcon(QStringLiteral(":/icons/settings.svg")),
                                    QStringLiteral("Save settings"), setFooter);
    setSave->setObjectName(QStringLiteral("dashPrimary"));
    setSave->setIconSize(QSize(13, 13));
    setSave->setFocusPolicy(Qt::NoFocus);
    setFootLay->addStretch(1);
    setFootLay->addWidget(setSave);
    setOuter->addWidget(setFooter);

    m_stack->addWidget(m_settingsPage);

    // ---- Form page ----
    m_formPage = new QWidget;
    auto* formLay = new QVBoxLayout(m_formPage);
    formLay->setContentsMargins(16, 12, 16, 10);
    formLay->setSpacing(6);

    m_formTitle = new QLabel(QStringLiteral("New Session"), m_formPage);
    m_formTitle->setObjectName(QStringLiteral("dashPageTitle"));
    formLay->addWidget(m_formTitle);

    auto addLabeled = [&](const QString& label, QWidget* field) {
        auto* lab = new QLabel(label, m_formPage);
        lab->setObjectName(QStringLiteral("fieldLabel"));
        formLay->addWidget(lab);
        formLay->addWidget(field);
    };

    m_nameEdit = new QLineEdit(m_formPage);
    m_nameEdit->setPlaceholderText(QStringLiteral("optional display name"));
    m_connectionModeCombo = new QComboBox(m_formPage);
    m_connectionModeCombo->addItem(QStringLiteral("SSH (terminal + SFTP)"),
                                   static_cast<int>(ConnectionMode::Ssh));
    m_connectionModeCombo->addItem(QStringLiteral("SFTP only (file manager)"),
                                   static_cast<int>(ConnectionMode::SftpOnly));
    m_hostEdit = new QLineEdit(m_formPage);
    m_hostEdit->setPlaceholderText(QStringLiteral("host"));
    m_hostEdit->setText(AppSettings::defaultHost());
    m_portSpin = new QSpinBox(m_formPage);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(AppSettings::defaultPort());
    m_userEdit = new QLineEdit(m_formPage);
    m_userEdit->setPlaceholderText(QStringLiteral("user"));
    m_userEdit->setText(AppSettings::defaultUser());
    m_passEdit = new QLineEdit(m_formPage);
    m_passEdit->setPlaceholderText(QStringLiteral("password (optional if using a key)"));
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_savePass = new QCheckBox(QStringLiteral("save password with profile"), m_formPage);

    m_keyPathEdit = new QLineEdit(m_formPage);
    m_keyPathEdit->setPlaceholderText(QStringLiteral("path to private key (optional)"));
    m_browseKeyBtn = new QPushButton(QStringLiteral("browse…"), m_formPage);
    m_browseKeyBtn->setObjectName(QStringLiteral("dashButton"));
    m_browseKeyBtn->setFocusPolicy(Qt::NoFocus);
    auto* keyRow = new QWidget(m_formPage);
    auto* keyLay = new QHBoxLayout(keyRow);
    keyLay->setContentsMargins(0, 0, 0, 0);
    keyLay->setSpacing(6);
    keyLay->addWidget(m_keyPathEdit, 1);
    keyLay->addWidget(m_browseKeyBtn);

    // Keyring: choose a pre-saved key, or import a new one into the keyring.
    m_keyringCombo = new QComboBox(m_formPage);
    m_importKeyBtn = new QPushButton(QStringLiteral("import…"), m_formPage);
    m_importKeyBtn->setObjectName(QStringLiteral("dashButton"));
    m_importKeyBtn->setFocusPolicy(Qt::NoFocus);
    m_removeKeyBtn = new QPushButton(QStringLiteral("remove"), m_formPage);
    m_removeKeyBtn->setObjectName(QStringLiteral("dashButton"));
    m_removeKeyBtn->setFocusPolicy(Qt::NoFocus);
    auto* keyringRow = new QWidget(m_formPage);
    auto* keyringLay = new QHBoxLayout(keyringRow);
    keyringLay->setContentsMargins(0, 0, 0, 0);
    keyringLay->setSpacing(6);
    keyringLay->addWidget(m_keyringCombo, 1);
    keyringLay->addWidget(m_importKeyBtn);
    keyringLay->addWidget(m_removeKeyBtn);

    m_keyPassEdit = new QLineEdit(m_formPage);
    m_keyPassEdit->setPlaceholderText(QStringLiteral("key passphrase (if encrypted)"));
    m_keyPassEdit->setEchoMode(QLineEdit::Password);
    m_saveKeyPass = new QCheckBox(QStringLiteral("save key passphrase with profile"), m_formPage);

    auto* authHint = new QLabel(
        QStringLiteral("auth: private key is tried first when a path is set; otherwise password"),
        m_formPage);
    authHint->setObjectName(QStringLiteral("dashHint"));
    authHint->setWordWrap(true);

    addLabeled(QStringLiteral("name"), m_nameEdit);
    addLabeled(QStringLiteral("connection type"), m_connectionModeCombo);
    addLabeled(QStringLiteral("host"), m_hostEdit);
    addLabeled(QStringLiteral("port"), m_portSpin);
    addLabeled(QStringLiteral("user"), m_userEdit);
    addLabeled(QStringLiteral("password"), m_passEdit);
    formLay->addWidget(m_savePass);
    addLabeled(QStringLiteral("key from keyring"), keyringRow);
    addLabeled(QStringLiteral("private key file"), keyRow);
    addLabeled(QStringLiteral("key passphrase"), m_keyPassEdit);
    formLay->addWidget(m_saveKeyPass);
    formLay->addWidget(authHint);

    auto* formRow = new QHBoxLayout;
    auto* backBtn = new QPushButton(QStringLiteral("Cancel"), m_formPage);
    auto* saveBtn = new QPushButton(QStringLiteral("Save profile"), m_formPage);
    auto* connectBtn = new QPushButton(QIcon(QStringLiteral(":/icons/connect.svg")),
                                       QStringLiteral("Connect"), m_formPage);
    m_saveProfileBtn = saveBtn;
    backBtn->setObjectName(QStringLiteral("dashButton"));
    saveBtn->setObjectName(QStringLiteral("dashButton"));
    connectBtn->setObjectName(QStringLiteral("dashPrimary"));
    for (auto* b : {backBtn, saveBtn, connectBtn}) {
        b->setIconSize(QSize(13, 13));
        b->setFocusPolicy(Qt::NoFocus);
    }
    formRow->addWidget(backBtn);
    formRow->addStretch(1);
    formRow->addWidget(saveBtn);
    formRow->addWidget(connectBtn);
    formLay->addStretch(1);
    formLay->addLayout(formRow);
    m_stack->addWidget(m_formPage);

    root->addWidget(mainCol, 1);

    // ---- Wiring ----
    connect(m_navGroup, &QButtonGroup::idClicked, this, [this](int id) {
        setNavPage(static_cast<NavPage>(id));
    });
    connect(m_newSessionBtn, &QPushButton::clicked, this, &DashboardPage::showNewSessionForm);
    connect(backBtn, &QPushButton::clicked, this, &DashboardPage::showHome);
    connect(saveBtn, &QPushButton::clicked, this, &DashboardPage::saveCurrentFormAsProfile);
    connect(connectBtn, &QPushButton::clicked, this, &DashboardPage::connectFromForm);
    connect(m_browseKeyBtn, &QPushButton::clicked, this, &DashboardPage::browsePrivateKey);
    connect(m_importKeyBtn, &QPushButton::clicked, this, &DashboardPage::importKeyIntoKeyring);
    connect(m_removeKeyBtn, &QPushButton::clicked, this, &DashboardPage::removeSelectedKeyringKey);
    connect(m_keyringCombo, &QComboBox::currentIndexChanged, this, &DashboardPage::onKeyringSelectionChanged);
    reloadKeyringCombo();
    connect(setSave, &QPushButton::clicked, this, &DashboardPage::saveSettingsUi);
    connect(m_settingsNav, &QListWidget::currentRowChanged, this, &DashboardPage::setSettingsCategory);
    connect(m_settingsAnimations, &QCheckBox::toggled, this, [](bool on) {
        Motion::setEnabled(on);
    });
    connect(clearLogs, &QPushButton::clicked, this, [this]() { m_logsView->clear(); });
    connect(m_passEdit, &QLineEdit::returnPressed, this, &DashboardPage::connectFromForm);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString&) { applySavedFilter(); });

    connect(m_savedTable, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (auto* item = m_savedTable->item(row, 0)) {
            openSavedProfile(item->data(Qt::UserRole).toString());
        }
    });
    connect(m_sessions, &SessionManager::sessionOpened, this, [this](const QString& id) {
        rebuildActiveList();
        if (const auto* live = m_sessions->session(id)) {
            appendLog(QStringLiteral("opened %1").arg(live->profile.displayTitle()));
        }
    });
    connect(m_sessions, &SessionManager::sessionClosed, this, [this](const QString&) {
        rebuildActiveList();
        appendLog(QStringLiteral("session closed"));
    });
    connect(m_sessions, &SessionManager::sessionStatusChanged, this,
            [this](const QString& id, const QString& status) {
                rebuildActiveList();
                if (const auto* live = m_sessions->session(id)) {
                    appendLog(QStringLiteral("%1 · %2").arg(live->profile.displayTitle(), status));
                }
            });
    connect(m_sessions, &SessionManager::sessionConnectionChanged, this,
            [this](const QString&, bool) { rebuildActiveList(); });

    loadSettingsUi();
    m_navHosts->setChecked(true);
    setNavPage(NavPage::Hosts);
    refresh();
}

QToolButton* DashboardPage::makeNavButton(const QString& iconPath, const QString& text, QWidget* parent)
{
    return makeSidebarNav(iconPath, text, parent);
}

QToolButton* DashboardPage::makeRowAction(const QString& iconPath, const QString& tip, QWidget* parent)
{
    return makeRowIcon(iconPath, tip, parent);
}

QWidget* DashboardPage::buildSettingsSection(const QString& title, QWidget* parent)
{
    auto* sec = new QWidget(parent);
    auto* lay = new QVBoxLayout(sec);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(6);
    // Keep the section at its natural height (scrolls when it overflows) instead
    // of letting the scroll area stretch it and pad every row far apart.
    sec->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* heading = new QLabel(title, sec);
    heading->setObjectName(QStringLiteral("settingsSectionTitle"));
    lay->addWidget(heading);
    return sec;
}

void DashboardPage::addSettingsField(QWidget* section, const QString& label, QWidget* field)
{
    auto* lay = qobject_cast<QVBoxLayout*>(section->layout());
    if (!lay || !field) {
        return;
    }
    auto* lab = new QLabel(label, section);
    lab->setObjectName(QStringLiteral("fieldLabel"));
    field->setParent(section);
    lay->addSpacing(4);
    lay->addWidget(lab);
    lay->addWidget(field);
}

void DashboardPage::setSettingsCategory(int index)
{
    if (!m_settingsStack || index < 0 || index >= m_settingsStack->count()) {
        return;
    }
    m_settingsStack->setCurrentIndex(index);
}

void DashboardPage::populateFontCombos()
{
    auto* fonts = FontManager::instance();

    auto fillUi = [&](QComboBox* box) {
        const QString current = box->currentData().toString();
        box->blockSignals(true);
        box->clear();
        box->addItem(QStringLiteral("Auto (system UI)"), QString());
        for (const FontCatalogEntry& e : fonts->uiCatalog()) {
            const QString suffix = fonts->isFamilyLoaded(e.family)
                ? QString()
                : QStringLiteral("  · download");
            box->addItem(e.label + suffix, e.family);
        }
        // Common system UI faces
        const QStringList systemUi = {
#ifdef Q_OS_WIN
            QStringLiteral("Segoe UI"), QStringLiteral("Calibri"), QStringLiteral("Arial"),
#elif defined(Q_OS_MACOS)
            QStringLiteral("SF Pro Text"), QStringLiteral("Helvetica Neue"), QStringLiteral("Arial"),
#else
            QStringLiteral("Ubuntu"), QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans"),
#endif
        };
        for (const QString& family : systemUi) {
            if (box->findData(family) < 0 && QFontDatabase::hasFamily(family)) {
                box->addItem(family, family);
            }
        }
        int idx = box->findData(current);
        box->setCurrentIndex(idx >= 0 ? idx : 0);
        box->blockSignals(false);
    };

    auto fillTerm = [&](QComboBox* box) {
        const QString current = box->currentData().toString();
        box->blockSignals(true);
        box->clear();
        box->addItem(QStringLiteral("Auto (system monospace)"), QString());
        for (const FontCatalogEntry& e : fonts->terminalCatalog()) {
            const QString suffix = fonts->isFamilyLoaded(e.family)
                ? QString()
                : QStringLiteral("  · download");
            box->addItem(e.label + suffix, e.family);
        }
        for (const QString& family : clientoshMonospaceFamilies()) {
            if (box->findData(family) < 0) {
                box->addItem(family, family);
            }
        }
        int idx = box->findData(current);
        box->setCurrentIndex(idx >= 0 ? idx : 0);
        box->blockSignals(false);
    };

    fillUi(m_settingsUiFontFamily);
    fillTerm(m_settingsFontFamily);
}

void DashboardPage::ensureSelectedFonts()
{
    auto* fonts = FontManager::instance();
    const QString ui = m_settingsUiFontFamily->currentData().toString();
    const QString term = m_settingsFontFamily->currentData().toString();
    if (fonts->needsDownload(ui)) {
        fonts->ensureFamily(ui);
    }
    if (fonts->needsDownload(term)) {
        fonts->ensureFamily(term);
    }
}

void DashboardPage::refreshFontPreviews()
{
    // Previews refresh via connected lambdas on the appearance page.
}

void DashboardPage::syncColorSwatch(QAbstractButton* btn, const QColor& color)
{
    if (!btn) {
        return;
    }
    btn->setStyleSheet(QStringLiteral(
                           "QToolButton#colorSwatchBtn {"
                           "  background: %1;"
                           "  border: 1px solid #555555;"
                           "  color: %2;"
                           "  font-size: 10px;"
                           "}")
                           .arg(color.name(),
                                (color.lightness() > 140) ? QStringLiteral("#111111")
                                                         : QStringLiteral("#eeeeee")));
    btn->setText(color.name(QColor::HexRgb).toUpper());
    btn->setToolTip(color.name(QColor::HexRgb));
}

void DashboardPage::pickTerminalColor(bool foreground)
{
    const QColor current = foreground ? m_termFg : m_termBg;
    const QColor chosen = QColorDialog::getColor(current, this,
                                                 foreground ? QStringLiteral("Terminal text color")
                                                            : QStringLiteral("Terminal background"),
                                                 QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) {
        return;
    }
    QColor solid = chosen;
    solid.setAlpha(255);
    if (foreground) {
        m_termFg = solid;
        syncColorSwatch(m_settingsTermFgBtn, m_termFg);
    } else {
        m_termBg = solid;
        syncColorSwatch(m_settingsTermBgBtn, m_termBg);
    }
    if (m_settingsTermPreview) {
        m_settingsTermPreview->setStyleSheet(
            QStringLiteral("QLabel#settingsTermPreview { color: %1; background: %2; padding: 8px; }")
                .arg(m_termFg.name(), m_termBg.name()));
    }
    persistAppearanceLive();
}

void DashboardPage::persistAppearanceLive()
{
    QSettings s;
    s.setValue(QLatin1String(AppSettings::kTheme), m_settingsTheme->currentData().toString());
    s.setValue(QLatin1String(AppSettings::kUiFontFamily), m_settingsUiFontFamily->currentData().toString());
    s.setValue(QLatin1String(AppSettings::kUiFontSize), m_settingsUiFontSize->value());
    s.setValue(QLatin1String(AppSettings::kFontFamily), m_settingsFontFamily->currentData().toString());
    AppSettings::setFontSize(m_settingsFontSize->value());
    AppSettings::setColorSetting(AppSettings::kTerminalFg, m_termFg);
    AppSettings::setColorSetting(AppSettings::kTerminalBg, m_termBg);
    s.setValue(QLatin1String(AppSettings::kHighlightAddresses), m_settingsHighlightAddresses->isChecked());
    s.setValue(QLatin1String(AppSettings::kHighlightLogKeywords), m_settingsHighlightKeywords->isChecked());
    s.sync();
    emit settingsApplied();
}

void DashboardPage::persistShortcutsLive()
{
    QSettings s;
    s.setValue(QLatin1String(AppSettings::kCtrlScrollFontZoom), m_settingsCtrlScrollZoom->isChecked());
    AppSettings::setShortcutSetting(AppSettings::kShortcutNewSession, m_shortcutNewSession->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutSettings, m_shortcutSettings->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutDashboard, m_shortcutDashboard->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutClosePanel, m_shortcutClosePanel->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutOpenSftp, m_shortcutOpenSftp->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutFontLarger, m_shortcutFontLarger->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutFontSmaller, m_shortcutFontSmaller->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutFontReset, m_shortcutFontReset->keySequence());

    AppSettings::setShortcutEnabled(AppSettings::kShortcutNewSessionEnabled, m_enableNewSession->isChecked());
    AppSettings::setShortcutEnabled(AppSettings::kShortcutSettingsEnabled, m_enableSettings->isChecked());
    AppSettings::setShortcutEnabled(AppSettings::kShortcutDashboardEnabled, m_enableDashboard->isChecked());
    AppSettings::setShortcutEnabled(AppSettings::kShortcutClosePanelEnabled, m_enableClosePanel->isChecked());
    AppSettings::setShortcutEnabled(AppSettings::kShortcutOpenSftpEnabled, m_enableOpenSftp->isChecked());
    AppSettings::setShortcutEnabled(AppSettings::kShortcutFontLargerEnabled, m_enableFontLarger->isChecked());
    AppSettings::setShortcutEnabled(AppSettings::kShortcutFontSmallerEnabled, m_enableFontSmaller->isChecked());
    AppSettings::setShortcutEnabled(AppSettings::kShortcutFontResetEnabled, m_enableFontReset->isChecked());
    s.sync();
    emit settingsApplied();
}

void DashboardPage::resetShortcutsToDefaults()
{
    const QList<QKeySequenceEdit*> edits = {
        m_shortcutNewSession, m_shortcutSettings, m_shortcutDashboard, m_shortcutClosePanel,
        m_shortcutOpenSftp,   m_shortcutFontLarger, m_shortcutFontSmaller, m_shortcutFontReset};
    for (QKeySequenceEdit* e : edits) {
        if (e) {
            e->blockSignals(true);
        }
    }
    m_settingsCtrlScrollZoom->blockSignals(true);
    m_settingsCtrlScrollZoom->setChecked(true);
    const QList<QCheckBox*> enables = {
        m_enableNewSession, m_enableSettings, m_enableDashboard, m_enableClosePanel,
        m_enableOpenSftp,   m_enableFontLarger, m_enableFontSmaller, m_enableFontReset};
    for (QCheckBox* c : enables) {
        if (c) {
            c->blockSignals(true);
            c->setChecked(true);
        }
    }
    m_shortcutNewSession->setKeySequence(QKeySequence(QStringLiteral("Ctrl+N")));
    m_shortcutSettings->setKeySequence(QKeySequence(QStringLiteral("Ctrl+,")));
    m_shortcutDashboard->setKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+D")));
    m_shortcutClosePanel->setKeySequence(QKeySequence(QStringLiteral("Ctrl+W")));
    m_shortcutOpenSftp->setKeySequence(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    m_shortcutFontLarger->setKeySequence(QKeySequence(QStringLiteral("Ctrl+=")));
    m_shortcutFontSmaller->setKeySequence(QKeySequence(QStringLiteral("Ctrl+-")));
    m_shortcutFontReset->setKeySequence(QKeySequence(QStringLiteral("Ctrl+0")));
    m_settingsCtrlScrollZoom->blockSignals(false);
    for (QKeySequenceEdit* e : edits) {
        if (e) {
            e->blockSignals(false);
        }
    }
    for (QCheckBox* c : enables) {
        if (c) {
            c->blockSignals(false);
        }
    }
    persistShortcutsLive();
}

void DashboardPage::syncTerminalFontSizeUi(int points)
{
    if (!m_settingsFontSize) {
        return;
    }
    const int clamped = qBound(9, points, 22);
    AppSettings::setFontSize(clamped);
    if (m_settingsFontSize->value() == clamped) {
        return;
    }
    m_settingsFontSize->blockSignals(true);
    m_settingsFontSize->setValue(clamped);
    m_settingsFontSize->blockSignals(false);
}

void DashboardPage::updateTopBar()
{
    const bool hosts = (m_currentNav == NavPage::Hosts);
    const bool form = (m_currentNav == NavPage::Form);
    m_searchEdit->setVisible(hosts);
    m_newSessionBtn->setVisible(!form);

    switch (m_currentNav) {
    case NavPage::Hosts:
        m_pageTitle->setText(QStringLiteral("Hosts"));
        m_pageSub->setText(QStringLiteral("saved connection profiles"));
        break;
    case NavPage::Active: // unreachable (entry removed)
    case NavPage::Keychain:
        m_pageTitle->setText(QStringLiteral("Keychain"));
        m_pageSub->setText(QStringLiteral("stored credentials overview"));
        break;
    case NavPage::Logs:
        m_pageTitle->setText(QStringLiteral("Logs"));
        m_pageSub->setText(QStringLiteral("recent session events"));
        break;
    case NavPage::Settings:
        m_pageTitle->setText(QStringLiteral("Settings"));
        m_pageSub->setText(QStringLiteral("preferences by category"));
        break;
    case NavPage::Form:
        m_pageTitle->setText(m_formTitle->text());
        m_pageSub->setText(QStringLiteral("connection details"));
        break;
    }
}

void DashboardPage::setNavPage(NavPage page)
{
    m_currentNav = page;
    switch (page) {
    case NavPage::Hosts:
    case NavPage::Active: // deprecated entry; fall back to the Hosts page
        m_stack->setCurrentWidget(m_hostsPage);
        m_navHosts->setChecked(true);
        break;
    case NavPage::Keychain:
        rebuildKeychainList();
        m_stack->setCurrentWidget(m_keysPage);
        m_navKeys->setChecked(true);
        break;
    case NavPage::Logs:
        m_stack->setCurrentWidget(m_logsPage);
        m_navLogs->setChecked(true);
        break;
    case NavPage::Settings:
        loadSettingsUi();
        m_stack->setCurrentWidget(m_settingsPage);
        m_navSettings->setChecked(true);
        break;
    case NavPage::Form:
        m_stack->setCurrentWidget(m_formPage);
        break;
    }
    updateTopBar();
}

void DashboardPage::appendLog(const QString& line)
{
    if (!m_logsView) {
        return;
    }
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
    m_logsView->appendPlainText(QStringLiteral("[%1] %2").arg(stamp, line));
}

void DashboardPage::refresh()
{
    m_profiles = loadProfiles();
    rebuildSavedList();
    rebuildActiveList();
    if (m_currentNav == NavPage::Keychain) {
        rebuildKeychainList();
    }
}

void DashboardPage::showHome()
{
    refresh();
    setNavPage(NavPage::Hosts);
}

void DashboardPage::showNewSessionForm()
{
    clearForm();
    m_editingId.clear();
    m_formTitle->setText(QStringLiteral("New Session"));
    m_saveProfileBtn->setText(QStringLiteral("Save profile"));
    m_savePass->setChecked(AppSettings::savePasswordDefault());
    setNavPage(NavPage::Form);
    m_hostEdit->setFocus();
}

void DashboardPage::showEditSessionForm(const QString& profileId)
{
    const int row = profileIndexById(profileId);
    if (row < 0) {
        m_hint->setText(QStringLiteral("session not found"));
        return;
    }
    loadProfileIntoForm(m_profiles[row]);
    m_editingId = m_profiles[row].id;
    m_formTitle->setText(QStringLiteral("Edit Session"));
    m_saveProfileBtn->setText(QStringLiteral("Save changes"));
    setNavPage(NavPage::Form);
    m_nameEdit->setFocus();
}

void DashboardPage::clearForm()
{
    m_nameEdit->clear();
    m_connectionModeCombo->setCurrentIndex(0);
    m_hostEdit->setText(AppSettings::defaultHost());
    m_portSpin->setValue(AppSettings::defaultPort());
    m_userEdit->setText(AppSettings::defaultUser());
    m_passEdit->clear();
    m_savePass->setChecked(AppSettings::savePasswordDefault());
    m_keyPathEdit->clear();
    reloadKeyringCombo();
    m_keyringCombo->setCurrentIndex(0); // "none"
    onKeyringSelectionChanged(m_keyringCombo->currentIndex());
    m_keyPassEdit->clear();
    m_saveKeyPass->setChecked(false);
}

void DashboardPage::loadProfileIntoForm(const SessionProfile& profile)
{
    m_nameEdit->setText(profile.name);
    const int modeIdx = m_connectionModeCombo->findData(static_cast<int>(profile.connectionMode));
    m_connectionModeCombo->setCurrentIndex(modeIdx >= 0 ? modeIdx : 0);
    m_hostEdit->setText(profile.host);
    m_portSpin->setValue(profile.port);
    m_userEdit->setText(profile.user);
    m_passEdit->setText(profile.password);
    m_savePass->setChecked(profile.savePassword);
    m_keyPathEdit->setText(profile.privateKeyPath);
    reloadKeyringCombo();
    if (!profile.privateKeyId.trimmed().isEmpty()) {
        // A pre-saved keyring key takes precedence over the file path.
        const int idx = m_keyringCombo->findData(profile.privateKeyId);
        m_keyringCombo->setCurrentIndex(idx >= 0 ? idx : 0);
        if (idx >= 0) {
            m_keyPathEdit->clear();
        }
    } else {
        m_keyringCombo->setCurrentIndex(0);
    }
    onKeyringSelectionChanged(m_keyringCombo->currentIndex());
    m_keyPassEdit->setText(profile.keyPassphrase);
    m_saveKeyPass->setChecked(profile.saveKeyPassphrase);
}

void DashboardPage::browsePrivateKey()
{
    const QString startDir = m_keyPathEdit->text().trimmed().isEmpty()
        ? QDir::homePath() + QStringLiteral("/.ssh")
        : QFileInfo(m_keyPathEdit->text()).absolutePath();

    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Select private key"),
        startDir,
        QStringLiteral("Private keys (id_rsa id_ed25519 id_ecdsa id_dsa *);;All files (*)"));
    if (!path.isEmpty()) {
        m_keyPathEdit->setText(QDir::toNativeSeparators(path));
        // Manually choosing a file means "this path" is the chosen key — drop
        // any keyring selection so the path is used.
        m_keyringCombo->setCurrentIndex(0);
    }
}

void DashboardPage::reloadKeyringCombo()
{
    if (!m_keyringCombo) {
        return;
    }
    const QString prevId = m_keyringCombo->currentData().toString();
    m_keyringCombo->blockSignals(true);
    m_keyringCombo->clear();
    m_keyringCombo->addItem(QStringLiteral("— none (use file path) —"), QString());
    VaultManager vault;
    const QVector<StoredKey> keys = vault.listStoredKeys();
    for (const StoredKey& k : keys) {
        QString label = k.name.trimmed().isEmpty() ? QStringLiteral("(unnamed)") : k.name;
        if (!k.type.trimmed().isEmpty()) {
            label += QStringLiteral("  ·  %1").arg(k.type);
        }
        if (k.hasPassphrase) {
            label += QStringLiteral("  ·  passphrase");
        }
        m_keyringCombo->addItem(label, k.id);
    }
    const int idx = m_keyringCombo->findData(prevId);
    m_keyringCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_keyringCombo->blockSignals(false);
}

void DashboardPage::onKeyringSelectionChanged(int index)
{
    const bool hasKeyringKey = m_keyringCombo->currentData().toString().length() > 0;
    m_removeKeyBtn->setEnabled(hasKeyringKey);
    if (hasKeyringKey) {
        // Keyring key takes precedence; a selected path would be ignored/confusing.
        m_keyPathEdit->clear();
    }
}

void DashboardPage::importKeyIntoKeyring()
{
    const QString startDir = m_keyPathEdit->text().trimmed().isEmpty()
        ? QDir::homePath() + QStringLiteral("/.ssh")
        : QFileInfo(m_keyPathEdit->text()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Import private key into keyring"),
        startDir,
        QStringLiteral("Private keys (id_rsa id_ed25519 id_ecdsa id_dsa *);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_hint->setText(QStringLiteral("cannot read key file: %1").arg(path));
        return;
    }
    const QByteArray pem = f.readAll();
    const QString baseName = QFileInfo(path).fileName();

    bool ok = false;
    const QString displayName = QInputDialog::getText(
        this,
        QStringLiteral("Keyring key name"),
        QStringLiteral("Give this key a memorable name:"),
        QLineEdit::Normal,
        baseName,
        &ok);
    if (!ok) {
        return;
    }

    StoredKey key;
    key.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    key.name = displayName.trimmed().isEmpty() ? baseName : displayName.trimmed();
    // Detect type from the PEM header so we can label it in the picker/keychain.
    if (pem.contains(QByteArrayLiteral("OPENSSH PRIVATE KEY"))) {
        key.type = QStringLiteral("openssh");
    } else if (pem.contains(QByteArrayLiteral("EC PRIVATE KEY"))) {
        key.type = QStringLiteral("ec");
    } else {
        key.type = QStringLiteral("pem");
    }

    VaultManager vault;
    if (!vault.storeStoredKey(key)) {
        m_hint->setText(QStringLiteral("failed to store key in the keyring"));
        return;
    }
    reloadKeyringCombo();
    const int idx = m_keyringCombo->findData(key.id);
    if (idx >= 0) {
        m_keyringCombo->setCurrentIndex(idx);
    }
    onKeyringSelectionChanged(m_keyringCombo->currentIndex());
    m_hint->setText(QStringLiteral("key '%1' saved to keyring").arg(key.name));
    appendLog(QStringLiteral("imported key '%1' into keyring").arg(key.name));
}

void DashboardPage::removeSelectedKeyringKey()
{
    const QString id = m_keyringCombo->currentData().toString();
    if (id.isEmpty()) {
        return;
    }
    const QString label = m_keyringCombo->currentText();
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("Remove keyring key"),
        QStringLiteral("Remove \"%1\" from the keyring?").arg(label));
    if (answer != QMessageBox::Yes) {
        return;
    }
    VaultManager vault;
    vault.removeStoredKey(id);
    reloadKeyringCombo();
    m_keyringCombo->setCurrentIndex(0);
    onKeyringSelectionChanged(m_keyringCombo->currentIndex());
    m_hint->setText(QStringLiteral("key removed from keyring"));
    appendLog(QStringLiteral("removed keyring key '%1'").arg(label));
}

void DashboardPage::fillProfileFromForm(SessionProfile* profile) const
{
    if (!profile) {
        return;
    }
    profile->name = m_nameEdit->text().trimmed();
    profile->connectionMode = static_cast<ConnectionMode>(m_connectionModeCombo->currentData().toInt());
    profile->host = m_hostEdit->text().trimmed();
    profile->port = m_portSpin->value();
    profile->user = m_userEdit->text().trimmed();
    profile->password = m_passEdit->text();
    profile->savePassword = m_savePass->isChecked();
    const QString keyringId = m_keyringCombo->currentData().toString();
    if (!keyringId.isEmpty()) {
        profile->privateKeyId = keyringId;
        profile->privateKeyPath.clear();
    } else {
        profile->privateKeyId.clear();
        profile->privateKeyPath = m_keyPathEdit->text().trimmed();
    }
    profile->keyPassphrase = m_keyPassEdit->text();
    profile->saveKeyPassphrase = m_saveKeyPass->isChecked();
    if (!profile->savePassword) {
        profile->password.clear();
    }
}

void DashboardPage::showSettings()
{
    setNavPage(NavPage::Settings);
}

int DashboardPage::profileIndexById(const QString& id) const
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id == id) {
            return i;
        }
    }
    return -1;
}

void DashboardPage::openSavedProfile(const QString& profileId)
{
    const int row = profileIndexById(profileId);
    if (row < 0) {
        return;
    }
    const SessionProfile& p = m_profiles[row];
    if (p.isSftpOnly()) {
        emit openSftpForProfile(p);
    } else {
        emit openProfile(p);
    }
}

void DashboardPage::sftpSavedProfile(const QString& profileId)
{
    const int row = profileIndexById(profileId);
    if (row < 0) {
        return;
    }
    // Standalone SFTP: always open a new independent pane (own SSH session inside
    // SftpClient). Never require or reuse a live SSH tab — multiple SFTPs to the
    // same host/port/user can coexist.
    emit openSftpForProfile(m_profiles[row]);
}

void DashboardPage::deleteSavedProfile(const QString& profileId)
{
    const int row = profileIndexById(profileId);
    if (row < 0) {
        return;
    }
    const QString title = m_profiles[row].displayTitle();
    m_profiles.removeAt(row);
    saveProfiles(m_profiles);
    rebuildSavedList();
    rebuildKeychainList();
    m_hint->setText(QStringLiteral("deleted %1").arg(title));
    appendLog(QStringLiteral("deleted profile %1").arg(title));
}

void DashboardPage::rebuildSavedList()
{
    m_savedTable->setRowCount(0);

    for (const SessionProfile& p : m_profiles) {
        const int row = m_savedTable->rowCount();
        m_savedTable->insertRow(row);

        auto* nameItem = new QTableWidgetItem(p.displayTitle());
        nameItem->setData(Qt::UserRole, p.id);
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        auto* typeItem = new QTableWidgetItem(p.connectionTypeLabel());
        typeItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        typeItem->setForeground(p.isSftpOnly() ? QColor(0x9a, 0x9a, 0x9a) : QColor(0x8a, 0x8a, 0x8a));

        QString auth = QStringLiteral("password");
        if (p.usesPrivateKey() && p.savePassword) {
            auth = QStringLiteral("key + pass");
        } else if (p.usesPrivateKey()) {
            auth = QStringLiteral("private key");
        } else if (!p.savePassword) {
            auth = QStringLiteral("prompt");
        }
        auto* authItem = new QTableWidgetItem(auth);
        authItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        authItem->setForeground(QColor(0x7a, 0x7a, 0x7a));

        m_savedTable->setItem(row, 0, nameItem);
        m_savedTable->setItem(row, 1, typeItem);
        m_savedTable->setItem(row, 2, authItem);

        auto* actions = new QWidget(m_savedTable);
        actions->setObjectName(QStringLiteral("dashRowActions"));
        auto* lay = new QHBoxLayout(actions);
        lay->setContentsMargins(2, 0, 4, 0);
        lay->setSpacing(1);
        lay->addStretch(1);

        const QString id = p.id;
        auto* openBtn = makeRowIcon(p.isSftpOnly() ? QStringLiteral(":/icons/folder.svg")
                                                   : QStringLiteral(":/icons/connect.svg"),
                                    p.isSftpOnly() ? QStringLiteral("open sftp")
                                                   : QStringLiteral("connect"),
                                    actions);
        auto* sftpBtn = makeRowIcon(QStringLiteral(":/icons/folder.svg"), QStringLiteral("sftp"), actions);
        auto* editBtn = makeRowIcon(QStringLiteral(":/icons/edit.svg"), QStringLiteral("edit"), actions);
        auto* delBtn = makeRowIcon(QStringLiteral(":/icons/close.svg"), QStringLiteral("delete"), actions);
        sftpBtn->setVisible(!p.isSftpOnly());

        connect(openBtn, &QToolButton::clicked, this, [this, id]() { openSavedProfile(id); });
        connect(sftpBtn, &QToolButton::clicked, this, [this, id]() { sftpSavedProfile(id); });
        connect(editBtn, &QToolButton::clicked, this, [this, id]() { showEditSessionForm(id); });
        connect(delBtn, &QToolButton::clicked, this, [this, id]() { deleteSavedProfile(id); });

        lay->addWidget(openBtn);
        lay->addWidget(sftpBtn);
        lay->addWidget(editBtn);
        lay->addWidget(delBtn);
        m_savedTable->setCellWidget(row, 3, actions);
    }

    applySavedFilter();
}

void DashboardPage::applySavedFilter()
{
    const QString q = m_searchEdit->text().trimmed();
    int visible = 0;
    for (int row = 0; row < m_savedTable->rowCount(); ++row) {
        bool match = q.isEmpty();
        if (!match) {
            for (int col = 0; col < 3; ++col) {
                if (auto* item = m_savedTable->item(row, col)) {
                    if (item->text().contains(q, Qt::CaseInsensitive)) {
                        match = true;
                        break;
                    }
                }
            }
            // Also match hidden host/user (IP not shown in the table).
            if (!match) {
                if (auto* item = m_savedTable->item(row, 0)) {
                    const int idx = profileIndexById(item->data(Qt::UserRole).toString());
                    if (idx >= 0) {
                        const SessionProfile& p = m_profiles[idx];
                        if (p.host.contains(q, Qt::CaseInsensitive)
                            || p.user.contains(q, Qt::CaseInsensitive)
                            || p.endpoint().contains(q, Qt::CaseInsensitive)) {
                            match = true;
                        }
                    }
                }
            }
        }
        m_savedTable->setRowHidden(row, !match);
        if (match) {
            ++visible;
        }
    }

    if (m_profiles.isEmpty()) {
        m_savedTable->hide();
        m_savedEmpty->setText(QStringLiteral("no hosts yet — create a session to get started"));
        m_savedEmpty->show();
    } else if (visible == 0) {
        m_savedTable->show();
        m_savedEmpty->setText(QStringLiteral("no hosts match the filter"));
        m_savedEmpty->show();
    } else {
        m_savedTable->show();
        m_savedEmpty->hide();
    }
}

void DashboardPage::rebuildActiveList()
{
    const QStringList ids = m_sessions->sessionIds();
    const int n = ids.size();

    // With the dedicated "Active" page removed, this only drives the small
    // "N live" badge shown under the Hosts entry in the sidebar.
    if (n == 0) {
        m_activeBadge->hide();
    } else {
        m_activeBadge->setText(QStringLiteral("%1 live").arg(n));
        m_activeBadge->show();
    }
}

void DashboardPage::rebuildKeychainList()
{
    m_keysTable->setRowCount(0);
    int stored = 0;
    for (const SessionProfile& p : m_profiles) {
        const bool hasPass = p.savePassword;
        const bool hasKey = p.usesPrivateKey();
        if (!hasPass && !hasKey) {
            continue;
        }
        ++stored;
        const int row = m_keysTable->rowCount();
        m_keysTable->insertRow(row);

        auto* nameItem = new QTableWidgetItem(p.displayTitle());
        nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        auto* passItem = new QTableWidgetItem(hasPass ? QStringLiteral("stored") : QStringLiteral("—"));
        passItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        passItem->setForeground(QColor(0x8a, 0x8a, 0x8a));

        QString keyText = QStringLiteral("—");
        if (hasKey) {
            if (!p.privateKeyId.trimmed().isEmpty()) {
                VaultManager vault;
                StoredKey stored;
                if (vault.retrieveStoredKey(p.privateKeyId, stored)) {
                    keyText = QStringLiteral("%1 (keyring)").arg(
                        stored.name.trimmed().isEmpty() ? QStringLiteral("stored key") : stored.name);
                } else {
                    keyText = QStringLiteral("stored key (missing)");
                }
            } else {
                keyText = QFileInfo(p.privateKeyPath).fileName();
                if (keyText.isEmpty()) {
                    keyText = p.privateKeyPath;
                }
            }
        }
        auto* keyItem = new QTableWidgetItem(keyText);
        keyItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        keyItem->setForeground(QColor(0x8a, 0x8a, 0x8a));

        m_keysTable->setItem(row, 0, nameItem);
        m_keysTable->setItem(row, 1, passItem);
        m_keysTable->setItem(row, 2, keyItem);
    }

    if (stored == 0) {
        m_keysTable->hide();
        m_keysEmpty->show();
    } else {
        m_keysTable->show();
        m_keysEmpty->hide();
    }
}

void DashboardPage::saveCurrentFormAsProfile()
{
    const QString host = m_hostEdit->text().trimmed();
    const QString user = m_userEdit->text().trimmed();
    if (host.isEmpty() || user.isEmpty()) {
        m_hint->setText(QStringLiteral("host and user required"));
        setNavPage(NavPage::Hosts);
        return;
    }
    const bool hasKey = !m_keyPathEdit->text().trimmed().isEmpty()
        || !m_keyringCombo->currentData().toString().isEmpty();
    if (m_passEdit->text().isEmpty() && !hasKey) {
        m_hint->setText(QStringLiteral("provide a password or a private key"));
        return;
    }

    SessionProfile p;
    if (!m_editingId.isEmpty()) {
        p.id = m_editingId;
    } else {
        p = makeProfile(host, m_portSpin->value(), user, m_passEdit->text(), m_nameEdit->text());
    }
    fillProfileFromForm(&p);
    p.host = host;
    p.user = user;
    p.password = m_passEdit->text();
    p.keyPassphrase = m_keyPassEdit->text();
    p.savePassword = m_savePass->isChecked();
    p.saveKeyPassphrase = m_saveKeyPass->isChecked();
    if (!p.savePassword) {
        p.password.clear();
    }
    if (!p.saveKeyPassphrase) {
        p.keyPassphrase.clear();
    }

    if (!m_editingId.isEmpty()) {
        bool updated = false;
        for (SessionProfile& existing : m_profiles) {
            if (existing.id == m_editingId) {
                if (p.savePassword && m_passEdit->text().isEmpty() && !existing.password.isEmpty()) {
                    p.password = existing.password;
                }
                if (p.saveKeyPassphrase && m_keyPassEdit->text().isEmpty()
                    && !existing.keyPassphrase.isEmpty()) {
                    p.keyPassphrase = existing.keyPassphrase;
                }
                existing = p;
                updated = true;
                break;
            }
        }
        if (!updated) {
            m_profiles.push_back(p);
        }
        m_hint->setText(QStringLiteral("session updated"));
        appendLog(QStringLiteral("updated profile %1").arg(p.displayTitle()));
    } else {
        m_profiles.push_back(p);
        m_hint->setText(QStringLiteral("profile saved"));
        appendLog(QStringLiteral("saved profile %1").arg(p.displayTitle()));
    }

    saveProfiles(m_profiles);
    m_editingId.clear();
    showHome();
}

void DashboardPage::connectFromForm()
{
    const QString host = m_hostEdit->text().trimmed();
    const QString user = m_userEdit->text().trimmed();
    if (host.isEmpty() || user.isEmpty()) {
        return;
    }
    const bool hasKey = !m_keyPathEdit->text().trimmed().isEmpty()
        || !m_keyringCombo->currentData().toString().isEmpty();
    if (m_passEdit->text().isEmpty() && !hasKey) {
        m_hint->setText(QStringLiteral("provide a password or a private key"));
        return;
    }

    SessionProfile p = makeProfile(host, m_portSpin->value(), user, m_passEdit->text(), m_nameEdit->text());
    fillProfileFromForm(&p);
    p.password = m_passEdit->text();
    p.keyPassphrase = m_keyPassEdit->text();

    if (p.password.isEmpty() && !m_editingId.isEmpty()) {
        for (const SessionProfile& existing : m_profiles) {
            if (existing.id == m_editingId && !existing.password.isEmpty()) {
                p.password = existing.password;
                break;
            }
        }
    }
    if (p.keyPassphrase.isEmpty() && !m_editingId.isEmpty()) {
        for (const SessionProfile& existing : m_profiles) {
            if (existing.id == m_editingId && !existing.keyPassphrase.isEmpty()) {
                p.keyPassphrase = existing.keyPassphrase;
                break;
            }
        }
    }
    emit openProfile(p);
}

void DashboardPage::loadSettingsUi()
{
    QSettings s;
    m_settingsSavePassDefault->setChecked(AppSettings::savePasswordDefault());
    m_settingsDefaultHost->setText(AppSettings::defaultHost());
    m_settingsDefaultUser->setText(AppSettings::defaultUser());

    // Block live-persist handlers while hydrating Appearance controls.
    const QList<QObject*> appearanceBlocks = {
        m_settingsTheme,           m_settingsUiFontFamily, m_settingsUiFontSize,
        m_settingsFontFamily,      m_settingsFontSize,     m_settingsHighlightAddresses,
        m_settingsHighlightKeywords, m_settingsTermBgImageBtn, m_settingsTermBgOpacity, m_settingsTermBgBlur};
    for (QObject* o : appearanceBlocks) {
        if (auto* w = qobject_cast<QWidget*>(o)) {
            w->blockSignals(true);
        }
    }

    const QString theme = AppSettings::theme();
    const int themeIdx = m_settingsTheme->findData(theme);
    m_settingsTheme->setCurrentIndex(themeIdx >= 0 ? themeIdx : 0);

    populateFontCombos();

    auto selectFamily = [](QComboBox* box, const QString& family) {
        int idx = box->findData(family);
        if (idx < 0 && !family.isEmpty()) {
            box->addItem(family, family);
            idx = box->findData(family);
        }
        box->setCurrentIndex(idx >= 0 ? idx : 0);
    };
    selectFamily(m_settingsUiFontFamily, AppSettings::uiFontFamily());
    selectFamily(m_settingsFontFamily, AppSettings::fontFamily());
    m_settingsUiFontSize->setValue(AppSettings::uiFontSize());
    m_settingsFontSize->setValue(AppSettings::fontSize());
    m_settingsHighlightAddresses->setChecked(AppSettings::highlightAddresses());
    m_settingsHighlightKeywords->setChecked(AppSettings::highlightLogKeywords());

    m_termFg = AppSettings::terminalFg();
    m_termBg = AppSettings::terminalBg();
    syncColorSwatch(m_settingsTermFgBtn, m_termFg);
    syncColorSwatch(m_settingsTermBgBtn, m_termBg);

    const QString bgImagePath = AppSettings::terminalBgImage();
    if (!bgImagePath.isEmpty()) {
        m_settingsTermBgImage->setText(QFileInfo(bgImagePath).fileName());
    } else {
        m_settingsTermBgImage->setText(QStringLiteral("None"));
    }
    m_settingsTermBgOpacity->setValue(int(AppSettings::terminalBgOpacity() * 100));
    m_settingsTermBgBlur->setValue(AppSettings::terminalBgBlur());

    if (m_settingsTermPreview) {
        m_settingsTermPreview->setFont(
            clientoshMonospaceFont(m_settingsFontSize->value(), m_settingsFontFamily->currentData().toString()));
        m_settingsTermPreview->setStyleSheet(
            QStringLiteral("QLabel#settingsTermPreview { color: %1; background: %2; padding: 8px; }")
                .arg(m_termFg.name(), m_termBg.name()));
    }

    for (QObject* o : appearanceBlocks) {
        if (auto* w = qobject_cast<QWidget*>(o)) {
            w->blockSignals(false);
        }
    }
    ensureSelectedFonts();

    const bool animations = AppSettings::animationsEnabled();
    m_settingsAnimations->blockSignals(true);
    m_settingsAnimations->setChecked(animations);
    m_settingsAnimations->blockSignals(false);
    Motion::setEnabled(animations);

    m_settingsShowStats->setChecked(AppSettings::showServerStats());
    m_settingsStatsInterval->setValue(AppSettings::statsIntervalSec());
    m_settingsDefaultPort->setValue(AppSettings::defaultPort());

    m_settingsHideDotfiles->setChecked(AppSettings::hideDotfiles());
    const int viewIdx = m_settingsSftpView->findData(AppSettings::sftpDefaultView());
    m_settingsSftpView->setCurrentIndex(viewIdx >= 0 ? viewIdx : 0);
    if (m_settingsSftpVerbose) {
        m_settingsSftpVerbose->blockSignals(true);
        m_settingsSftpVerbose->setChecked(AppSettings::sftpVerboseLogging());
        m_settingsSftpVerbose->blockSignals(false);
    }

    const QList<QKeySequenceEdit*> shortcutEdits = {
        m_shortcutNewSession, m_shortcutSettings, m_shortcutDashboard, m_shortcutClosePanel,
        m_shortcutOpenSftp,   m_shortcutFontLarger, m_shortcutFontSmaller, m_shortcutFontReset};
    for (QKeySequenceEdit* e : shortcutEdits) {
        if (e) {
            e->blockSignals(true);
        }
    }
    m_settingsCtrlScrollZoom->blockSignals(true);
    m_settingsCtrlScrollZoom->setChecked(AppSettings::ctrlScrollFontZoom());

    // Hydrate enable toggles independently of the key fields.
    const QList<QCheckBox*> shortcutEnables = {
        m_enableNewSession, m_enableSettings, m_enableDashboard, m_enableClosePanel,
        m_enableOpenSftp,   m_enableFontLarger, m_enableFontSmaller, m_enableFontReset};
    const QList<const char*> shortcutEnableKeys = {
        AppSettings::kShortcutNewSessionEnabled, AppSettings::kShortcutSettingsEnabled,
        AppSettings::kShortcutDashboardEnabled,  AppSettings::kShortcutClosePanelEnabled,
        AppSettings::kShortcutOpenSftpEnabled,    AppSettings::kShortcutFontLargerEnabled,
        AppSettings::kShortcutFontSmallerEnabled, AppSettings::kShortcutFontResetEnabled};
    for (int i = 0; i < shortcutEnables.size(); ++i) {
        QCheckBox* c = shortcutEnables[i];
        if (c) {
            c->blockSignals(true);
            const bool on = AppSettings::shortcutEnabled(shortcutEnableKeys[i]);
            c->setChecked(on);
            c->blockSignals(false);
        }
    }
    // Reflect enable state on the key fields.
    const QList<QCheckBox*> enableWidgets = shortcutEnables;
    const QList<QKeySequenceEdit*> editWidgets = shortcutEdits;
    for (int i = 0; i < enableWidgets.size(); ++i) {
        if (enableWidgets[i] && editWidgets[i]) {
            editWidgets[i]->setEnabled(enableWidgets[i]->isChecked());
        }
    }

    m_shortcutNewSession->setKeySequence(AppSettings::shortcutNewSession());
    m_shortcutSettings->setKeySequence(AppSettings::shortcutSettings());
    m_shortcutDashboard->setKeySequence(AppSettings::shortcutDashboard());
    m_shortcutClosePanel->setKeySequence(AppSettings::shortcutClosePanel());
    m_shortcutOpenSftp->setKeySequence(AppSettings::shortcutOpenSftp());
    m_shortcutFontLarger->setKeySequence(AppSettings::shortcutFontLarger());
    m_shortcutFontSmaller->setKeySequence(AppSettings::shortcutFontSmaller());
    m_shortcutFontReset->setKeySequence(AppSettings::shortcutFontReset());
    m_settingsCtrlScrollZoom->blockSignals(false);
    for (QKeySequenceEdit* e : shortcutEdits) {
        if (e) {
            e->blockSignals(false);
        }
    }

    if (m_settingsNav && m_settingsNav->currentRow() < 0) {
        m_settingsNav->setCurrentRow(0);
    }
    setSettingsCategory(m_settingsNav ? m_settingsNav->currentRow() : 0);
}

void DashboardPage::saveSettingsUi()
{
    QSettings s;
    s.setValue(QLatin1String(AppSettings::kSavePasswordDefault), m_settingsSavePassDefault->isChecked());
    s.setValue(QLatin1String(AppSettings::kDefaultHost), m_settingsDefaultHost->text().trimmed().isEmpty()
                                                             ? QStringLiteral("127.0.0.1")
                                                             : m_settingsDefaultHost->text().trimmed());
    s.setValue(QLatin1String(AppSettings::kDefaultUser), m_settingsDefaultUser->text().trimmed());
    s.setValue(QLatin1String(AppSettings::kTheme), m_settingsTheme->currentData().toString());
    s.setValue(QLatin1String(AppSettings::kUiFontFamily), m_settingsUiFontFamily->currentData().toString());
    s.setValue(QLatin1String(AppSettings::kUiFontSize), m_settingsUiFontSize->value());
    s.setValue(QLatin1String(AppSettings::kFontFamily), m_settingsFontFamily->currentData().toString());
    AppSettings::setFontSize(m_settingsFontSize->value());
    AppSettings::setColorSetting(AppSettings::kTerminalFg, m_termFg);
    AppSettings::setColorSetting(AppSettings::kTerminalBg, m_termBg);
    s.setValue(QLatin1String(AppSettings::kHighlightAddresses), m_settingsHighlightAddresses->isChecked());
    s.setValue(QLatin1String(AppSettings::kHighlightLogKeywords), m_settingsHighlightKeywords->isChecked());
    s.setValue(QLatin1String(AppSettings::kAnimationsEnabled), m_settingsAnimations->isChecked());
    s.setValue(QLatin1String(AppSettings::kShowServerStats), m_settingsShowStats->isChecked());
    s.setValue(QLatin1String(AppSettings::kStatsIntervalSec), m_settingsStatsInterval->value());
    s.setValue(QLatin1String(AppSettings::kDefaultPort), m_settingsDefaultPort->value());
    s.setValue(QLatin1String(AppSettings::kHideDotfiles), m_settingsHideDotfiles->isChecked());
    s.setValue(QLatin1String(AppSettings::kSftpDefaultView), m_settingsSftpView->currentData().toString());
    if (m_settingsSftpVerbose) {
        AppSettings::setSftpVerboseLogging(m_settingsSftpVerbose->isChecked());
    }

    s.setValue(QLatin1String(AppSettings::kCtrlScrollFontZoom), m_settingsCtrlScrollZoom->isChecked());
    AppSettings::setShortcutSetting(AppSettings::kShortcutNewSession, m_shortcutNewSession->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutSettings, m_shortcutSettings->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutDashboard, m_shortcutDashboard->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutClosePanel, m_shortcutClosePanel->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutOpenSftp, m_shortcutOpenSftp->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutFontLarger, m_shortcutFontLarger->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutFontSmaller, m_shortcutFontSmaller->keySequence());
    AppSettings::setShortcutSetting(AppSettings::kShortcutFontReset, m_shortcutFontReset->keySequence());
    s.sync();

    Motion::setEnabled(m_settingsAnimations->isChecked());
    ensureSelectedFonts();
    emit settingsApplied();

    m_hint->setText(QStringLiteral("settings saved"));
    appendLog(QStringLiteral("settings saved"));
    showHome();
}
