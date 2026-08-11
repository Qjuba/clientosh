#include "TerminalWindow.h"
#include "SessionChip.h"
#include "TerminalWidget.h"

#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMimeData>
#include <QToolButton>
#include <QVBoxLayout>

TerminalWindow::TerminalWindow(const QString& sessionId, const QString& title,
                               TerminalWidget* terminal, QWidget* parent)
    : QWidget(parent, Qt::Window)
    , m_sessionId(sessionId)
    , m_title(title)
    , m_terminal(terminal)
{
    setObjectName(QStringLiteral("terminalWindow"));
    setWindowTitle(QStringLiteral("clientosh — %1").arg(title));
    setWindowIcon(QIcon(QStringLiteral(":/icons/terminal.svg")));
    setAttribute(Qt::WA_DeleteOnClose);
    setAcceptDrops(true);
    resize(960, 640);
    setMinimumSize(480, 320);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_nav = new QWidget(this);
    m_nav->setObjectName(QStringLiteral("sessionNav"));
    m_nav->setFixedHeight(28);
    m_navLay = new QHBoxLayout(m_nav);
    m_navLay->setContentsMargins(4, 2, 4, 2);
    m_navLay->setSpacing(3);

    m_statusLabel = new QLabel(QStringLiteral(""), m_nav);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));

    auto* attachBtn = new QToolButton(m_nav);
    attachBtn->setIcon(QIcon(QStringLiteral(":/icons/attach.svg")));
    attachBtn->setIconSize(QSize(14, 14));
    attachBtn->setFixedSize(24, 22);
    attachBtn->setToolTip(QStringLiteral("attach back to main window"));
    attachBtn->setAutoRaise(true);
    attachBtn->setFocusPolicy(Qt::NoFocus);
    attachBtn->setObjectName(QStringLiteral("navIconBtn"));
    attachBtn->setCursor(Qt::PointingHandCursor);

    auto* closeBtn = new QToolButton(m_nav);
    closeBtn->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    closeBtn->setIconSize(QSize(14, 14));
    closeBtn->setFixedSize(24, 22);
    closeBtn->setToolTip(QStringLiteral("close session"));
    closeBtn->setAutoRaise(true);
    closeBtn->setFocusPolicy(Qt::NoFocus);
    closeBtn->setObjectName(QStringLiteral("navIconBtn"));
    closeBtn->setCursor(Qt::PointingHandCursor);

    m_navLay->addStretch(1);
    m_navLay->addWidget(m_statusLabel);
    m_navLay->addWidget(attachBtn);
    m_navLay->addWidget(closeBtn);

    rebuildChip();

    m_terminal->setParent(this);
    root->addWidget(m_nav);
    root->addWidget(m_terminal, 1);

    connect(attachBtn, &QToolButton::clicked, this, [this]() { emit attachRequested(m_sessionId); });
    connect(closeBtn, &QToolButton::clicked, this, [this]() { emit closeSessionRequested(m_sessionId); });

    m_terminal->show();
    m_terminal->setFocus(Qt::OtherFocusReason);
}

void TerminalWindow::rebuildChip()
{
    if (m_chip) {
        m_navLay->removeWidget(m_chip);
        m_chip->deleteLater();
        m_chip = nullptr;
    }
    m_chip = new SessionChip(m_sessionId, m_title, true, m_nav);
    m_navLay->insertWidget(0, m_chip);
    connect(m_chip, &SessionChip::activated, this, [this](const QString&) {
        raise();
        activateWindow();
        if (m_terminal) {
            m_terminal->setFocus(Qt::OtherFocusReason);
        }
    });
    connect(m_chip, &SessionChip::closeRequested, this, &TerminalWindow::closeSessionRequested);
    connect(m_chip, &SessionChip::dragFinishedOutside, this, &TerminalWindow::detachHereRequested);
    connect(m_chip, &SessionChip::reorderRequested, this,
            [this](const QString& fromId, const QString&) { emit sessionDropRequested(fromId); });
}

void TerminalWindow::setSessionId(const QString& id)
{
    m_sessionId = id;
}

TerminalWidget* TerminalWindow::takeTerminal()
{
    TerminalWidget* t = m_terminal;
    m_terminal = nullptr;
    if (t) {
        t->setParent(nullptr);
    }
    return t;
}

void TerminalWindow::setTerminal(TerminalWidget* terminal)
{
    auto* root = qobject_cast<QVBoxLayout*>(layout());
    if (m_terminal && m_terminal != terminal) {
        if (root) {
            root->removeWidget(m_terminal);
        }
        m_terminal->setParent(nullptr);
        m_terminal->hide();
    }
    m_terminal = terminal;
    if (m_terminal && root) {
        m_terminal->setParent(this);
        root->addWidget(m_terminal, 1);
        m_terminal->show();
        m_terminal->setFocus(Qt::OtherFocusReason);
    }
}

void TerminalWindow::setStatus(const QString& status)
{
    m_statusLabel->setText(status);
}

void TerminalWindow::setTitleText(const QString& title)
{
    m_title = title;
    setWindowTitle(QStringLiteral("clientosh — %1").arg(title));
    rebuildChip();
}

void TerminalWindow::closeEvent(QCloseEvent* event)
{
    if (m_terminal) {
        emit closeSessionRequested(m_sessionId);
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}

void TerminalWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowActivate) {
        emit focusRequested(m_sessionId);
    }
    QWidget::changeEvent(event);
}

void TerminalWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(QLatin1String(kClientoshSessionMime))) {
        const QString id = QString::fromUtf8(event->mimeData()->data(QLatin1String(kClientoshSessionMime)));
        if (id != m_sessionId) {
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

void TerminalWindow::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()->hasFormat(QLatin1String(kClientoshSessionMime))) {
        event->ignore();
        return;
    }
    const QString id = QString::fromUtf8(event->mimeData()->data(QLatin1String(kClientoshSessionMime)));
    if (id.isEmpty() || id == m_sessionId) {
        event->ignore();
        return;
    }
    emit sessionDropRequested(id);
    event->acceptProposedAction();
}
