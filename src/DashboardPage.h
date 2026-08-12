#pragma once

#include "core/SessionProfile.h"

#include <QColor>
#include <QWidget>
#include <QVector>

class SessionManager;
class QStackedWidget;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QToolButton;
class QTableWidget;
class QListWidget;
class QPlainTextEdit;
class QButtonGroup;
class QComboBox;
class QAbstractButton;
class QKeySequenceEdit;

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(SessionManager* sessions, QWidget* parent = nullptr);

    void refresh();
    void showNewSessionForm();
    void showSettings();
    void showHome();
    void appendLog(const QString& line);
    void syncTerminalFontSizeUi(int points);

signals:
    void openProfile(const SessionProfile& profile);
    void openLiveSession(const QString& id);
    void openSftpForProfile(const SessionProfile& profile);
    void openSftpForSession(const QString& sessionId);
    void closeLiveSession(const QString& sessionId);
    void settingsApplied();

private:
    enum class NavPage {
        Hosts = 0,
        Active, // deprecated/unused: live sessions are surfaced via the "N live" badge
        Keychain,
        Logs,
        Settings,
        Form
    };

    enum class SettingsCategory {
        General = 0,
        Appearance,
        Performance,
        Ssh,
        Sftp,
        Shortcuts
    };

    void rebuildSavedList();
    void rebuildActiveList();
    void rebuildKeychainList();
    void applySavedFilter();
    void clearForm();
    void loadProfileIntoForm(const SessionProfile& profile);
    void showEditSessionForm(const QString& profileId);
    void openSavedProfile(const QString& profileId);
    void sftpSavedProfile(const QString& profileId);
    void deleteSavedProfile(const QString& profileId);
    void saveCurrentFormAsProfile();
    void connectFromForm();
    void loadSettingsUi();
    void saveSettingsUi();
    void setSettingsCategory(int index);
    void populateFontCombos();
    void ensureSelectedFonts();
    void refreshFontPreviews();
    void persistAppearanceLive();
    void syncColorSwatch(QAbstractButton* btn, const QColor& color);
    void pickTerminalColor(bool foreground);
    void persistShortcutsLive();
    void resetShortcutsToDefaults();
    void setNavPage(NavPage page);
    void browsePrivateKey();
    void reloadKeyringCombo();
    void rebuildStoredKeysFromForm();
    void importKeyIntoKeyring();
    void removeSelectedKeyringKey();
    void onKeyringSelectionChanged(int index);
    void fillProfileFromForm(SessionProfile* profile) const;
    int profileIndexById(const QString& id) const;
    QToolButton* makeNavButton(const QString& iconPath, const QString& text, QWidget* parent);
    QToolButton* makeRowAction(const QString& iconPath, const QString& tip, QWidget* parent);
    void updateTopBar();
    QWidget* buildSettingsSection(const QString& title, QWidget* parent);
    void addSettingsField(QWidget* section, const QString& label, QWidget* field);

    SessionManager* m_sessions = nullptr;

    QWidget* m_sidebar = nullptr;
    QButtonGroup* m_navGroup = nullptr;
    QToolButton* m_navHosts = nullptr;
    QToolButton* m_navKeys = nullptr;
    QToolButton* m_navLogs = nullptr;
    QToolButton* m_navSettings = nullptr;
    QLabel* m_activeBadge = nullptr;

    QLabel* m_pageTitle = nullptr;
    QLabel* m_pageSub = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_newSessionBtn = nullptr;
    QWidget* m_topBar = nullptr;

    QStackedWidget* m_stack = nullptr;
    QWidget* m_hostsPage = nullptr;
    QWidget* m_keysPage = nullptr;
    QWidget* m_logsPage = nullptr;
    QWidget* m_settingsPage = nullptr;
    QWidget* m_formPage = nullptr;

    QTableWidget* m_savedTable = nullptr;
    QLabel* m_savedEmpty = nullptr;
    QTableWidget* m_keysTable = nullptr;
    QLabel* m_keysEmpty = nullptr;
    QPlainTextEdit* m_logsView = nullptr;

    QLabel* m_formTitle = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_connectionModeCombo = nullptr;
    QLineEdit* m_hostEdit = nullptr;
    QSpinBox* m_portSpin = nullptr;
    QLineEdit* m_userEdit = nullptr;
    QLineEdit* m_passEdit = nullptr;
    QCheckBox* m_savePass = nullptr;
    QLineEdit* m_keyPathEdit = nullptr;
    QComboBox* m_keyringCombo = nullptr;
    QPushButton* m_importKeyBtn = nullptr;
    QPushButton* m_removeKeyBtn = nullptr;
    QLineEdit* m_keyPassEdit = nullptr;
    QCheckBox* m_saveKeyPass = nullptr;
    QPushButton* m_browseKeyBtn = nullptr;
    QPushButton* m_saveProfileBtn = nullptr;

    QListWidget* m_settingsNav = nullptr;
    QStackedWidget* m_settingsStack = nullptr;

    QCheckBox* m_settingsSavePassDefault = nullptr;
    QLineEdit* m_settingsDefaultHost = nullptr;
    QLineEdit* m_settingsDefaultUser = nullptr;

    QComboBox* m_settingsTheme = nullptr;
    QComboBox* m_settingsUiFontFamily = nullptr;
    QSpinBox* m_settingsUiFontSize = nullptr;
    QComboBox* m_settingsFontFamily = nullptr;
    QSpinBox* m_settingsFontSize = nullptr;
    QLabel* m_settingsFontStatus = nullptr;
    QToolButton* m_settingsTermFgBtn = nullptr;
    QToolButton* m_settingsTermBgBtn = nullptr;
    QToolButton* m_settingsTermBgImageBtn = nullptr;
    QLabel* m_settingsTermBgImage = nullptr;
    QSpinBox* m_settingsTermBgOpacity = nullptr;
    QSpinBox* m_settingsTermBgBlur = nullptr;
    QLabel* m_settingsTermPreview = nullptr;
    QColor m_termFg;
    QColor m_termBg;

    QCheckBox* m_settingsAnimations = nullptr;
    QCheckBox* m_settingsShowStats = nullptr;
    QSpinBox* m_settingsStatsInterval = nullptr;

    QSpinBox* m_settingsDefaultPort = nullptr;

    QCheckBox* m_settingsHideDotfiles = nullptr;
    QComboBox* m_settingsSftpView = nullptr;
    QCheckBox* m_settingsSftpVerbose = nullptr;
    QCheckBox* m_settingsHighlightAddresses = nullptr;
    QCheckBox* m_settingsHighlightKeywords = nullptr;

    QCheckBox* m_settingsCtrlScrollZoom = nullptr;
    QKeySequenceEdit* m_shortcutNewSession = nullptr;
    QKeySequenceEdit* m_shortcutSettings = nullptr;
    QKeySequenceEdit* m_shortcutDashboard = nullptr;
    QKeySequenceEdit* m_shortcutClosePanel = nullptr;
    QKeySequenceEdit* m_shortcutOpenSftp = nullptr;
    QKeySequenceEdit* m_shortcutFontLarger = nullptr;
    QKeySequenceEdit* m_shortcutFontSmaller = nullptr;
    QKeySequenceEdit* m_shortcutFontReset = nullptr;
    QCheckBox* m_enableNewSession = nullptr;
    QCheckBox* m_enableSettings = nullptr;
    QCheckBox* m_enableDashboard = nullptr;
    QCheckBox* m_enableClosePanel = nullptr;
    QCheckBox* m_enableOpenSftp = nullptr;
    QCheckBox* m_enableFontLarger = nullptr;
    QCheckBox* m_enableFontSmaller = nullptr;
    QCheckBox* m_enableFontReset = nullptr;

    QLabel* m_hint = nullptr;

    QVector<SessionProfile> m_profiles;
    QString m_editingId;
    NavPage m_currentNav = NavPage::Hosts;
};
