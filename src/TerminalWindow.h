#pragma once

#include <QWidget>

class TerminalWidget;
class SessionChip;
class QLabel;
class QToolButton;

class TerminalWindow : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalWindow(const QString& sessionId, const QString& title,
                           TerminalWidget* terminal, QWidget* parent = nullptr);

    QString sessionId() const { return m_sessionId; }
    void setSessionId(const QString& id);
    TerminalWidget* terminal() const { return m_terminal; }
    TerminalWidget* takeTerminal();
    void setTerminal(TerminalWidget* terminal);
    void setStatus(const QString& status);
    void setTitleText(const QString& title);

signals:
    void attachRequested(const QString& sessionId);
    void closeSessionRequested(const QString& sessionId);
    void focusRequested(const QString& sessionId);
    void sessionDropRequested(const QString& sessionId); // drop another session onto this window
    void detachHereRequested(const QString& sessionId);  // drag finished outside from this chip

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void rebuildChip();

    QString m_sessionId;
    QString m_title;
    TerminalWidget* m_terminal = nullptr;
    QWidget* m_nav = nullptr;
    QLabel* m_statusLabel = nullptr;
    SessionChip* m_chip = nullptr;
    class QHBoxLayout* m_navLay = nullptr;
};
