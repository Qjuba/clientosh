#include "TopNavBar.h"
#include "SessionChip.h"
#include "SessionWorkspace.h"
#include "core/AppSettings.h"
#include "core/SessionManager.h"
#include "ui/Motion.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMetaObject>
#include <QMimeData>
#include <QSizePolicy>
#include <QThread>
#include <QToolButton>

namespace {
QToolButton* makeNavIcon(const QString& iconPath, const QString& tip, QWidget* parent)
{
    auto* btn = new Motion::HoverFillButton(parent);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(14, 14));
    btn->setToolTip(tip);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setObjectName(QStringLiteral("navIconBtn"));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(24, 22);
    btn->setHoverFill(QColor(0x3c, 0x3c, 0x3c));
    return btn;
}

QString formatBytesCompact(qint64 bytes)
{
    if (bytes < 0) {
        return QStringLiteral("—");
    }
    const double gib = double(bytes) / (1024.0 * 1024.0 * 1024.0);
    if (gib >= 10.0) {
        return QStringLiteral("%1G").arg(gib, 0, 'f', 0);
    }
    if (gib >= 1.0) {
        return QStringLiteral("%1G").arg(gib, 0, 'f', 1);
    }
    const double mib = double(bytes) / (1024.0 * 1024.0);
    if (mib >= 10.0) {
        return QStringLiteral("%1M").arg(mib, 0, 'f', 0);
    }
    if (mib >= 1.0) {
        return QStringLiteral("%1M").arg(mib, 0, 'f', 1);
    }
    return QStringLiteral("%1K").arg(bytes / 1024);
}

QString formatPercent(double value)
{
    if (value < 0.0) {
        return QStringLiteral("—");
    }
    return QStringLiteral("%1%").arg(qRound(value));
}
}

TopNavBar::TopNavBar(SessionManager* sessions, SessionWorkspace* workspace, QWidget* parent)
    : QWidget(parent)
    , m_sessions(sessions)
    , m_workspace(workspace)
{
    setObjectName(QStringLiteral("sessionNav"));
    setFixedHeight(28);
    setAcceptDrops(true);

    auto* navLay = new QHBoxLayout(this);
    navLay->setContentsMargins(4, 2, 4, 2);
    navLay->setSpacing(3);

    m_menuBtn = makeNavIcon(QStringLiteral(":/icons/menu.svg"), QStringLiteral("dashboard"), this);
    m_newBtn = makeNavIcon(QStringLiteral(":/icons/plus.svg"), QStringLiteral("new session"), this);
    m_sftpBtn = makeNavIcon(QStringLiteral(":/icons/folder.svg"),
                            QStringLiteral("open standalone sftp · duplicates active sftp"), this);

    m_tabsHost = new QWidget(this);
    m_tabsHost->setAcceptDrops(true);
    m_tabsLay = new QHBoxLayout(m_tabsHost);
    m_tabsLay->setContentsMargins(0, 0, 0, 0);
    m_tabsLay->setSpacing(3);

    m_stats = new QLabel(this);
    m_stats->setObjectName(QStringLiteral("serverStatsLabel"));
    m_stats->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_stats->setVisible(false); // shown only after first valid sample
    m_stats->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    navLay->addWidget(m_menuBtn);
    navLay->addWidget(m_newBtn);
    navLay->addSpacing(4);
    navLay->addWidget(m_tabsHost, 1);
    navLay->addWidget(m_stats, 0);
    navLay->addWidget(m_sftpBtn);

    m_statsThread = new QThread(this);
    m_statsClient = new ServerStatsClient;
    m_statsClient->moveToThread(m_statsThread);
    connect(m_statsThread, &QThread::finished, m_statsClient, &QObject::deleteLater);
    connect(m_statsClient, &ServerStatsClient::statsUpdated, this, &TopNavBar::applyStats);
    connect(m_statsClient, &ServerStatsClient::failed, this, [this](const QString&) {
        // Keep the slot hidden until a valid sample arrives — no placeholder/error chrome.
        hideStatsUntilData();
    });
    m_statsThread->start();

    connect(m_menuBtn, &QToolButton::clicked, this, &TopNavBar::dashboardRequested);
    connect(m_newBtn, &QToolButton::clicked, this, &TopNavBar::newSessionRequested);
    connect(m_sftpBtn, &QToolButton::clicked, this, [this]() {
        emit sftpRequested();
    });

    connect(m_sessions, &SessionManager::sessionOpened, this, [this](const QString&) { refresh(); });
    connect(m_sessions, &SessionManager::sessionClosed, this, [this](const QString&) { refresh(); });
    connect(m_sessions, &SessionManager::sessionActivated, this, [this](const QString&) { refresh(); });
    connect(m_sessions, &SessionManager::sessionStatusChanged, this, [this](const QString&, const QString&) {
        refresh();
    });
    connect(m_sessions, &SessionManager::sessionConnectionChanged, this, [this](const QString&, bool) {
        refresh();
    });

    if (m_workspace) {
        connect(m_workspace, &SessionWorkspace::layoutChanged, this, [this]() { refresh(); });
        connect(m_workspace, &SessionWorkspace::panelSelectRequested, this, [this](const PanelRef&) {
            refresh();
        });
    }

    refresh();
}

TopNavBar::~TopNavBar()
{
    if (m_statsClient) {
        QMetaObject::invokeMethod(m_statsClient, "stop", Qt::BlockingQueuedConnection);
    }
    if (m_statsThread) {
        m_statsThread->quit();
        m_statsThread->wait(3000);
    }
}

void TopNavBar::syncActiveChip()
{
    if (!m_workspace) {
        return;
    }
    const PanelRef active = m_workspace->activePanel();
    for (int i = 0; i < m_tabsLay->count(); ++i) {
        QLayoutItem* item = m_tabsLay->itemAt(i);
        auto* chip = item ? qobject_cast<SessionChip*>(item->widget()) : nullptr;
        if (chip) {
            chip->setActive(chip->panelRef() == active);
        }
    }
}

void TopNavBar::clearStatsDisplay()
{
    m_statsHaveData = false;
    m_stats->clear();
    m_stats->setToolTip(QString());
    m_stats->setVisible(false);
}

void TopNavBar::hideStatsUntilData()
{
    m_statsHaveData = false;
    m_stats->clear();
    m_stats->setToolTip(QString());
    m_stats->setVisible(false);
}

void TopNavBar::applyStats(const ServerStats& stats)
{
    if (m_statsSessionId.isEmpty()) {
        return;
    }
    if (!stats.valid) {
        // e.g. first CPU sample before a delta exists and no mem/disk yet
        if (!m_statsHaveData) {
            hideStatsUntilData();
        }
        return;
    }

    QString cpu = formatPercent(stats.cpuPercent);
    QString ram = QStringLiteral("—");
    if (stats.memTotalBytes > 0 && stats.memUsedBytes >= 0) {
        ram = QStringLiteral("%1/%2")
                  .arg(formatBytesCompact(stats.memUsedBytes), formatBytesCompact(stats.memTotalBytes));
    }

    QString disk = QStringLiteral("—");
    if (stats.diskTotalBytes > 0 && stats.diskUsedBytes >= 0) {
        const double pct = 100.0 * double(stats.diskUsedBytes) / double(stats.diskTotalBytes);
        disk = formatPercent(pct);
    }

    m_stats->setText(QStringLiteral("cpu %1  ·  ram %2  ·  disk %3").arg(cpu, ram, disk));

    QString tip;
    if (stats.cpuPercent >= 0.0) {
        tip += QStringLiteral("CPU %1%\n").arg(stats.cpuPercent, 0, 'f', 1);
    }
    if (stats.memTotalBytes > 0) {
        tip += QStringLiteral("RAM %1 / %2\n")
                   .arg(formatBytesCompact(stats.memUsedBytes), formatBytesCompact(stats.memTotalBytes));
    }
    if (stats.diskTotalBytes > 0) {
        tip += QStringLiteral("Disk %1 / %2")
                   .arg(formatBytesCompact(stats.diskUsedBytes), formatBytesCompact(stats.diskTotalBytes));
    }
    m_stats->setToolTip(tip.trimmed());
    m_statsHaveData = true;
    m_stats->setVisible(true);
}

void TopNavBar::syncStatsProbe()
{
    const QString id = m_sessions->activeId();
    const auto* live = m_sessions->session(id);
    const bool wantStats = AppSettings::showServerStats() && live && live->connected;

    if (!wantStats) {
        if (!m_statsSessionId.isEmpty()) {
            m_statsSessionId.clear();
            QMetaObject::invokeMethod(m_statsClient, "stop", Qt::QueuedConnection);
        }
        hideStatsUntilData();
        return;
    }

    if (m_statsSessionId == id) {
        // Same session: keep current visibility (hidden until first valid sample).
        return;
    }

    m_statsSessionId = id;
    hideStatsUntilData();
    QMetaObject::invokeMethod(m_statsClient, "start", Qt::QueuedConnection,
                              Q_ARG(SessionProfile, live->profile));
}

void TopNavBar::applySettings()
{
    // Restart probe so interval / visibility changes take effect immediately.
    const QString previous = m_statsSessionId;
    m_statsSessionId.clear();
    if (!previous.isEmpty()) {
        QMetaObject::invokeMethod(m_statsClient, "stop", Qt::QueuedConnection);
    }
    syncStatsProbe();
}

void TopNavBar::refresh()
{
    rebuildTabs();

    // SFTP is standalone (its own SSH session) so the button is always enabled when
    // any workspace exists; duplicating the active SFTP never needs a connected SSH tab.
    if (m_workspace && m_workspace->hasAttachedSessions()) {
        m_sftpBtn->setEnabled(true);
    } else if (auto* live = m_sessions->session(m_sessions->activeId())) {
        m_sftpBtn->setEnabled(live->connected);
    } else {
        // Still allow opening an SFTP from the dashboard hosts — MainWindow picks a profile.
        m_sftpBtn->setEnabled(m_workspace && !m_workspace->openPanels().isEmpty());
    }

    syncStatsProbe();
}

void TopNavBar::rebuildTabs()
{
    while (QLayoutItem* item = m_tabsLay->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (!m_workspace) {
        m_tabsLay->addStretch(1);
        return;
    }

    const PanelRef active = m_workspace->activePanel();
    for (const PanelRef& ref : m_workspace->openPanels()) {
        QString title;
        if (ref.kind == PanelKind::Sftp) {
            title = m_workspace->paneTitle(ref);
            if (title.isEmpty()) {
                title = QStringLiteral("sftp");
            }
        } else {
            const auto* live = m_sessions->session(ref.sessionId);
            if (!live) {
                continue;
            }
            title = live->profile.displayTitle();
        }

        auto* chip = new SessionChip(ref, title, ref == active, m_tabsHost);
        connect(chip, &SessionChip::activated, this, &TopNavBar::panelSelectRequested);
        connect(chip, &SessionChip::closeRequested, this, &TopNavBar::panelCloseRequested);
        connect(chip, &SessionChip::hoverActivated, this, &TopNavBar::panelPreviewRequested);
        connect(chip, &SessionChip::dragFinished, m_workspace, &SessionWorkspace::flushPendingDock);
        connect(chip, &SessionChip::reorderRequested, this,
                [this](const PanelRef& from, const PanelRef& before) {
                    if (from.kind == PanelKind::Terminal && before.kind == PanelKind::Terminal) {
                        m_workspace->reorderAttached(from.sessionId, before.sessionId);
                    }
                    rebuildTabs();
                });

        m_tabsLay->addWidget(chip);
    }
    m_tabsLay->addStretch(1);
}

void TopNavBar::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(QLatin1String(kClientoshPanelMime))
        || event->mimeData()->hasFormat(QLatin1String(kClientoshSessionMime))) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void TopNavBar::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasFormat(QLatin1String(kClientoshPanelMime))
        || event->mimeData()->hasFormat(QLatin1String(kClientoshSessionMime))) {
        event->acceptProposedAction();
    }
}

void TopNavBar::dropEvent(QDropEvent* event)
{
    PanelRef ref;
    if (event->mimeData()->hasFormat(QLatin1String(kClientoshPanelMime))) {
        ref = PanelRef::fromMime(event->mimeData()->data(QLatin1String(kClientoshPanelMime)));
    } else if (event->mimeData()->hasFormat(QLatin1String(kClientoshSessionMime))) {
        ref = PanelRef::terminal(
            QString::fromUtf8(event->mimeData()->data(QLatin1String(kClientoshSessionMime))));
    }
    if (!ref.isValid()) {
        event->ignore();
        return;
    }
    emit panelSelectRequested(ref);
    event->acceptProposedAction();
}
