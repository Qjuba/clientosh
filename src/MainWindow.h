#pragma once

#include "PanelTypes.h"
#include "core/SessionProfile.h"

#include <QMainWindow>
#include <QHash>
#include <QSet>

class QStackedWidget;
class QShortcut;
class SessionManager;
class DashboardPage;
class SessionWorkspace;
class TerminalWidget;
class TopNavBar;
class SftpWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void applyTheme();
    void setupShortcuts();
    void rebindShortcuts();
    void adjustAllTerminalFonts(int deltaOrAbsolute, bool absolute);
    void showDashboard();
    void showWorkspace();
    void openProfileSession(const SessionProfile& profile);
    void openProfileThenSftp(const SessionProfile& profile);
    void openProfileSftpOnly(const SessionProfile& profile);
    /** Mount terminal, layout for real size, then start SSH with matching PTY. */
    QString beginTerminalSession(const SessionProfile& profile, bool openSftpWhenConnected);
    void openOrFocusSession(const QString& id);
    void openOrFocusPanel(const PanelRef& ref);
    void closePanel(const PanelRef& ref);
    void closeLiveSession(const QString& id);
    void openSftp(const QString& id);
    void openStandaloneSftp(const SessionProfile& profile);
    void openSftpFromSession(const QString& sessionId);
    void createSftpPane(const QString& panelId, const SessionProfile& profile);
    void wireSessionTerminal(const QString& id, TerminalWidget* term);
    TerminalWidget* findTerminal(const QString& id) const;

    SessionManager* m_sessions = nullptr;
    TopNavBar* m_topNav = nullptr;
    QStackedWidget* m_rootStack = nullptr;
    DashboardPage* m_dashboard = nullptr;
    SessionWorkspace* m_workspace = nullptr;
    QHash<QString, SftpWindow*> m_sftpPanes;
    QString m_pendingSftpSessionId;
    QSet<QString> m_sftpOnlySessionIds;

    QShortcut* m_scNewSession = nullptr;
    QShortcut* m_scSettings = nullptr;
    QShortcut* m_scDashboard = nullptr;
    QShortcut* m_scClosePanel = nullptr;
    QShortcut* m_scOpenSftp = nullptr;
    QShortcut* m_scFontLarger = nullptr;
    QShortcut* m_scFontSmaller = nullptr;
    QShortcut* m_scFontReset = nullptr;
};
