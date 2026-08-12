#pragma once

#include "core/SessionProfile.h"
#include "core/SftpClient.h"

#include <QDateTime>
#include <QHash>
#include <QStringList>
#include <QWidget>
#include <QVector>

class QLabel;
class QLineEdit;
class QToolButton;
class QTableWidget;
class QTableWidgetItem;
class QProgressBar;
class QThread;
class QPushButton;
class QDropEvent;
class QDragEnterEvent;
class QMenu;
class QFileSystemWatcher;
class QTimer;
class QModelIndex;
class QPoint;
class QAction;

struct EditedRemoteFile {
    QString remotePath; // absolute remote path on the SFTP server
    QString localPath;  // local temp copy being (possibly) edited by the OS app
    QDateTime lastModified;
};

class SftpWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SftpWindow(const QString& sessionId,
                        const SessionProfile& profile,
                        QWidget* parent = nullptr);
    ~SftpWindow() override;

    QString sessionId() const { return m_sessionId; }
    const SessionProfile& profile() const { return m_profile; }
    void applyAppSettings();

signals:
    void windowClosed(const QString& sessionId);
    void debugLog(const QString& message);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void refresh();
    void goUp();
    void downloadSelected();
    void uploadFiles();
    void cancelTransfers();
    void uploadLocalPaths(const QStringList& paths);
    void uploadLocalPathsTo(const QStringList& paths, const QString& remoteDir);
    void createFolder();
    void deleteSelected();
    void renameSelected();
    void showContextMenu(const QPoint& globalPos);
    void openOrEditSelected();
    void editRemoteFile(const QString& remotePath, const QString& fileName);
    void onDownloadedForEdit(bool ok, const QString& message, const QString& remotePath,
                             const QString& localPath);
    void onEditedFileUploaded(bool ok, const QString& message, const QString& remotePath);
    void checkEditedFiles();
    void stopWatchingEditedFile(const QString& remotePath);
    void setBusy(bool busy);
    void populateTable();
    void setHideDotfiles(bool hide);
    void syncDotfilesToggle();
    void pumpQueue();
    QString joinRemote(const QString& name) const;
    QString parentPath(const QString& path) const;
    QTableWidgetItem* itemAtRow(int row) const;
    static QIcon iconForEntry(const SftpEntry& entry);

    QString m_sessionId;
    SessionProfile m_profile;
    QString m_cwd;
    QVector<SftpEntry> m_entries;
    bool m_hideDotfiles = true;

    QThread* m_thread = nullptr;
    SftpClient* m_client = nullptr;

    QLabel* m_title = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QToolButton* m_upBtn = nullptr;
    QToolButton* m_refreshBtn = nullptr;
    QToolButton* m_uploadBtn = nullptr;
    QToolButton* m_downloadBtn = nullptr;
    QToolButton* m_mkdirBtn = nullptr;
    QToolButton* m_deleteBtn = nullptr;
    QToolButton* m_dotfilesBtn = nullptr;
    QTableWidget* m_table = nullptr;
    QProgressBar* m_progress = nullptr;
    QToolButton* m_cancelBtn = nullptr;
    QLabel* m_status = nullptr;

    bool m_busy = false;
    bool m_cancelling = false;

    QStringList m_transferQueue;
    bool m_transferInFlight = false;
    quint64 m_queuedItems = 0;
    quint64 m_doneItems = 0;
    QString m_queueSummary;

    QFileSystemWatcher* m_fileWatcher = nullptr;
    QTimer* m_syncTimer = nullptr;
    QHash<QString, EditedRemoteFile> m_editedFiles;

    // When a transferFinished lands, route it according to what it was for.
    // transferFinished carries only (ok, message) — no path — so we remember
    // the context (edit-download vs auto-upload of a specific remote) here.
    bool m_pendingEditDownload = false;
    QString m_pendingEditRemote;
    QString m_pendingEditLocal;
    QStringList m_pendingEditUploads; // remotes pending an auto-upload completion
};
