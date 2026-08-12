#pragma once

#include "PanelTypes.h"
#include "core/ServerStatsClient.h"
#include "core/SessionProfile.h"

#include <QWidget>
#include <QStringList>

class SessionManager;
class SessionWorkspace;
class QHBoxLayout;
class QLabel;
class QToolButton;
class QThread;

class TopNavBar : public QWidget
{
    Q_OBJECT

public:
    explicit TopNavBar(SessionManager* sessions, SessionWorkspace* workspace, QWidget* parent = nullptr);
    ~TopNavBar() override;

    void refresh();
    void syncActiveChip();
    void applySettings();

signals:
    void dashboardRequested();
    void newSessionRequested();
    void panelSelectRequested(const PanelRef& ref);
    void panelCloseRequested(const PanelRef& ref);
    void panelPreviewRequested(const PanelRef& ref);
    void sftpRequested();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void rebuildTabs();
    void syncStatsProbe();
    void applyStats(const ServerStats& stats);
    void clearStatsDisplay();
    void hideStatsUntilData();

    SessionManager* m_sessions = nullptr;
    SessionWorkspace* m_workspace = nullptr;
    QHBoxLayout* m_tabsLay = nullptr;
    QWidget* m_tabsHost = nullptr;
    QToolButton* m_menuBtn = nullptr;
    QToolButton* m_newBtn = nullptr;
    QToolButton* m_sftpBtn = nullptr;
    QLabel* m_stats = nullptr;
    bool m_statsHaveData = false;

    QThread* m_statsThread = nullptr;
    ServerStatsClient* m_statsClient = nullptr;
    QString m_statsSessionId;
};
