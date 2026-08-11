#include "SftpWindow.h"
#include "core/AppSettings.h"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QColor>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QToolButton* makeSftpBtn(const QString& icon, const QString& tip, QWidget* parent)
{
    auto* btn = new QToolButton(parent);
    btn->setIcon(QIcon(icon));
    btn->setIconSize(QSize(14, 14));
    btn->setToolTip(tip);
    btn->setAutoRaise(true);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setObjectName(QStringLiteral("navIconBtn"));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(26, 24);
    return btn;
}

QString formatSize(quint64 bytes)
{
    if (bytes < 1024) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    if (bytes < 1024ull * 1024) {
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    }
    if (bytes < 1024ull * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    }
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

bool isDotfile(const QString& name)
{
    return name.startsWith(QLatin1Char('.')) && name != QLatin1String(".") && name != QLatin1String("..");
}
}

QIcon SftpWindow::iconForEntry(const SftpEntry& entry)
{
    if (entry.isLink) {
        return QIcon(QStringLiteral(":/icons/filetypes/file-link.svg"));
    }
    if (entry.isDir) {
        return QIcon(QStringLiteral(":/icons/filetypes/folder.svg"));
    }

    const QString ext = QFileInfo(entry.name).suffix().toLower();
    static const QStringList imageExt = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("gif"),
        QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("svg"), QStringLiteral("ico"),
        QStringLiteral("tif"), QStringLiteral("tiff")};
    static const QStringList archiveExt = {
        QStringLiteral("zip"), QStringLiteral("tar"), QStringLiteral("gz"), QStringLiteral("tgz"),
        QStringLiteral("bz2"), QStringLiteral("xz"), QStringLiteral("7z"), QStringLiteral("rar"),
        QStringLiteral("deb"), QStringLiteral("rpm")};
    static const QStringList audioExt = {
        QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("ogg"),
        QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("wma")};
    static const QStringList videoExt = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"), QStringLiteral("mov"),
        QStringLiteral("webm"), QStringLiteral("wmv"), QStringLiteral("m4v")};
    static const QStringList codeExt = {
        QStringLiteral("c"), QStringLiteral("cc"), QStringLiteral("cpp"), QStringLiteral("cxx"),
        QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("hh"), QStringLiteral("cs"),
        QStringLiteral("java"), QStringLiteral("js"), QStringLiteral("jsx"), QStringLiteral("ts"),
        QStringLiteral("tsx"), QStringLiteral("py"), QStringLiteral("rb"), QStringLiteral("go"),
        QStringLiteral("rs"), QStringLiteral("php"), QStringLiteral("sh"), QStringLiteral("bash"),
        QStringLiteral("zsh"), QStringLiteral("ps1"), QStringLiteral("sql"), QStringLiteral("json"),
        QStringLiteral("yml"), QStringLiteral("yaml"), QStringLiteral("toml"), QStringLiteral("xml"),
        QStringLiteral("html"), QStringLiteral("htm"), QStringLiteral("css"), QStringLiteral("scss"),
        QStringLiteral("vue"), QStringLiteral("swift"), QStringLiteral("kt"), QStringLiteral("kts"),
        QStringLiteral("cmake"), QStringLiteral("makefile")};
    static const QStringList textExt = {
        QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("markdown"), QStringLiteral("log"),
        QStringLiteral("csv"), QStringLiteral("tsv"), QStringLiteral("ini"), QStringLiteral("conf"),
        QStringLiteral("cfg"), QStringLiteral("env"), QStringLiteral("rst")};

    if (imageExt.contains(ext)) {
        return QIcon(QStringLiteral(":/icons/filetypes/file-image.svg"));
    }
    if (archiveExt.contains(ext)) {
        return QIcon(QStringLiteral(":/icons/filetypes/file-archive.svg"));
    }
    if (audioExt.contains(ext)) {
        return QIcon(QStringLiteral(":/icons/filetypes/file-audio.svg"));
    }
    if (videoExt.contains(ext)) {
        return QIcon(QStringLiteral(":/icons/filetypes/file-video.svg"));
    }
    if (codeExt.contains(ext)) {
        return QIcon(QStringLiteral(":/icons/filetypes/file-code.svg"));
    }
    if (textExt.contains(ext)) {
        return QIcon(QStringLiteral(":/icons/filetypes/file-text.svg"));
    }
    return QIcon(QStringLiteral(":/icons/filetypes/file.svg"));
}

SftpWindow::SftpWindow(const QString& sessionId, const SessionProfile& profile, QWidget* parent)
    : QWidget(parent)
    , m_sessionId(sessionId)
    , m_profile(profile)
{
    setObjectName(QStringLiteral("sftpWindow"));
    m_hideDotfiles = AppSettings::hideDotfiles();
    // When embedded in a split pane, stay a child widget (no OS window).
    if (!parent) {
        setWindowTitle(QStringLiteral("sftp · %1").arg(profile.displayTitle()));
        setWindowIcon(QIcon(QStringLiteral(":/icons/folder.svg")));
        resize(780, 520);
        setMinimumSize(520, 360);
        setAttribute(Qt::WA_DeleteOnClose);
    } else {
        setMinimumSize(240, 160);
    }

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* nav = new QWidget(this);
    nav->setObjectName(QStringLiteral("sessionNav"));
    nav->setFixedHeight(28);
    auto* navLay = new QHBoxLayout(nav);
    navLay->setContentsMargins(6, 2, 6, 2);
    navLay->setSpacing(4);

    m_title = new QLabel(QStringLiteral("sftp · %1").arg(profile.displayTitle()), nav);
    m_title->setObjectName(QStringLiteral("statusLabel"));

    m_upBtn = makeSftpBtn(QStringLiteral(":/icons/up.svg"), QStringLiteral("parent directory"), nav);
    m_refreshBtn = makeSftpBtn(QStringLiteral(":/icons/refresh.svg"), QStringLiteral("refresh"), nav);
    m_uploadBtn = makeSftpBtn(QStringLiteral(":/icons/upload.svg"), QStringLiteral("upload files"), nav);
    m_downloadBtn = makeSftpBtn(QStringLiteral(":/icons/download.svg"), QStringLiteral("download selected"), nav);
    m_mkdirBtn = makeSftpBtn(QStringLiteral(":/icons/folder.svg"), QStringLiteral("new folder"), nav);
    m_deleteBtn = makeSftpBtn(QStringLiteral(":/icons/close.svg"), QStringLiteral("delete selected"), nav);
    m_dotfilesBtn = makeSftpBtn(QStringLiteral(":/icons/filetypes/eye-off.svg"),
                                QStringLiteral("toggle hidden (dot) files"), nav);
    m_dotfilesBtn->setCheckable(true);

    navLay->addWidget(m_title, 1);
    navLay->addWidget(m_upBtn);
    navLay->addWidget(m_refreshBtn);
    navLay->addWidget(m_uploadBtn);
    navLay->addWidget(m_downloadBtn);
    navLay->addWidget(m_mkdirBtn);
    navLay->addWidget(m_deleteBtn);
    navLay->addWidget(m_dotfilesBtn);

    auto* pathBar = new QWidget(this);
    pathBar->setObjectName(QStringLiteral("sftpPathBar"));
    auto* pathLay = new QHBoxLayout(pathBar);
    pathLay->setContentsMargins(8, 6, 8, 6);
    pathLay->setSpacing(6);
    auto* pathLab = new QLabel(QStringLiteral("path"), pathBar);
    pathLab->setObjectName(QStringLiteral("fieldLabel"));
    m_pathEdit = new QLineEdit(pathBar);
    m_pathEdit->setObjectName(QStringLiteral("dashSearch"));
    pathLay->addWidget(pathLab);
    pathLay->addWidget(m_pathEdit, 1);

    m_table = new QTableWidget(0, 3, this);
    m_table->setObjectName(QStringLiteral("dashTable"));
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral("name"), QStringLiteral("size"), QStringLiteral("type")});
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(26);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setFocusPolicy(Qt::StrongFocus);
    m_table->setAlternatingRowColors(false);
    m_table->setIconSize(QSize(16, 16));

    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("sftpProgress"));
    m_progress->setTextVisible(true);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFixedHeight(14);
    m_progress->hide();

    m_status = new QLabel(QStringLiteral("connecting…"), this);
    m_status->setObjectName(QStringLiteral("dashHint"));
    m_status->setContentsMargins(8, 4, 8, 6);

    root->addWidget(nav);
    root->addWidget(pathBar);
    root->addWidget(m_table, 1);
    root->addWidget(m_progress);
    root->addWidget(m_status);

    applyAppSettings();
    syncDotfilesToggle();

    // Accept file drops from the OS anywhere over the SFTP view.
    setAcceptDrops(true);
    m_table->setAcceptDrops(true);
    m_table->viewport()->setAcceptDrops(true);

    m_thread = new QThread(this);
    m_client = new SftpClient;
    m_client->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_client, &QObject::deleteLater);

    connect(m_client, &SftpClient::connected, this, [this](const QString& home) {
        m_cwd = home;
        m_pathEdit->setText(home);
        setBusy(false);
        refresh();
    });
    connect(m_client, &SftpClient::disconnected, this, [this]() {
        m_status->setText(QStringLiteral("disconnected"));
        setBusy(false);
    });
    connect(m_client, &SftpClient::directoryListed, this,
            [this](const QString& path, const QVector<SftpEntry>& entries) {
                m_cwd = path;
                m_pathEdit->setText(path);
                m_entries = entries;
                populateTable();
                setBusy(false);
            });
    connect(m_client, &SftpClient::statusChanged, this, [this](const QString& s) {
        m_status->setText(s);
    });
    connect(m_client, &SftpClient::errorOccurred, this, [this](const QString& e) {
        m_status->setText(e);
        setBusy(false);
        QMessageBox::warning(this, QStringLiteral("sftp"), e);
    });
    connect(m_client, &SftpClient::transferProgress, this,
            [this](const QString& label, qint64 done, qint64 total) {
                m_progress->show();
                if (total > 0) {
                    m_progress->setRange(0, 100);
                    m_progress->setValue(static_cast<int>((done * 100) / total));
                    m_progress->setFormat(QStringLiteral("%1 · %p%").arg(label));
                } else {
                    m_progress->setRange(0, 0);
                    m_progress->setFormat(label);
                }
            });
    connect(m_client, &SftpClient::transferFinished, this, [this](bool ok, const QString& msg) {
        m_transferInFlight = false;
        if (ok) {
            ++m_doneItems;
        }
        if (m_transferQueue.isEmpty()) {
            m_progress->hide();
            m_progress->setRange(0, 100);
            m_progress->setValue(0);
            setBusy(false);
            if (!ok) {
                m_status->setText(msg);
                QMessageBox::warning(this, QStringLiteral("sftp"), msg);
            } else if (m_queuedItems > 1) {
                m_status->setText(QStringLiteral("%1 of %2 transferred")
                                      .arg(m_doneItems)
                                      .arg(m_queuedItems));
            } else {
                m_status->setText(msg);
            }
            refresh();
            m_doneItems = 0;
            m_queuedItems = 0;
            m_queueSummary.clear();
        } else {
            if (!ok) {
                m_status->setText(msg);
                QMessageBox::warning(this, QStringLiteral("sftp"), msg);
            }
            pumpQueue();
        }
    });
    // Route transfer completion for the remote-file-edit workflow.
    connect(m_client, &SftpClient::transferFinished, this,
            [this](bool ok, const QString& msg) {
                if (m_pendingEditDownload) {
                    const QString remote = m_pendingEditRemote;
                    const QString local = m_pendingEditLocal;
                    m_pendingEditDownload = false;
                    m_pendingEditRemote.clear();
                    m_pendingEditLocal.clear();
                    onDownloadedForEdit(ok, msg, remote, local);
                } else if (m_pendingEditUpload) {
                    const QString remote = m_pendingUploadRemote;
                    m_pendingEditUpload = false;
                    m_pendingUploadRemote.clear();
                    onEditedFileUploaded(ok, msg, remote);
                }
            });
    connect(m_client, &SftpClient::operationFinished, this, [this](bool ok, const QString& msg) {
        m_transferInFlight = false;
        setBusy(false);
        m_status->setText(msg);
        if (ok) {
            refresh();
        } else {
            QMessageBox::warning(this, QStringLiteral("sftp"), msg);
        }
    });
    connect(m_client, &SftpClient::debugLog, this, [this](const QString& line) {
        // Route to Diagnostics > Logs (DashboardPage) and to the SFTP status line.
        emit debugLog(line);
        m_status->setText(line);
    });

    connect(m_upBtn, &QToolButton::clicked, this, &SftpWindow::goUp);
    connect(m_refreshBtn, &QToolButton::clicked, this, &SftpWindow::refresh);
    connect(m_uploadBtn, &QToolButton::clicked, this, &SftpWindow::uploadFiles);
    connect(m_downloadBtn, &QToolButton::clicked, this, &SftpWindow::downloadSelected);
    connect(m_mkdirBtn, &QToolButton::clicked, this, &SftpWindow::createFolder);
    connect(m_deleteBtn, &QToolButton::clicked, this, &SftpWindow::deleteSelected);
    connect(m_dotfilesBtn, &QToolButton::toggled, this, [this](bool checked) {
        // checked == showing hidden files
        setHideDotfiles(!checked);
    });
    connect(m_pathEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_busy) {
            return;
        }
        setBusy(true);
        QMetaObject::invokeMethod(m_client, "listDirectory", Qt::QueuedConnection,
                                  Q_ARG(QString, m_pathEdit->text().trimmed()));
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int, int) { openOrEditSelected(); });
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) { showContextMenu(m_table->viewport()->mapToGlobal(pos)); });

    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString& localPath) {
                // Debounce: multiple writes from editors can fire this repeatedly;
                // the periodic timer does the actual mtime comparison and upload.
                Q_UNUSED(localPath)
                checkEditedFiles();
            });
    m_syncTimer = new QTimer(this);
    m_syncTimer->setInterval(15 * 1000);
    m_syncTimer->start();
    connect(m_syncTimer, &QTimer::timeout, this, [this]() { checkEditedFiles(); });

    m_thread->start();
    setBusy(true);
    QMetaObject::invokeMethod(m_client, "connectHost", Qt::QueuedConnection,
                              Q_ARG(SessionProfile, m_profile));
}

SftpWindow::~SftpWindow()
{
    if (m_client) {
        QMetaObject::invokeMethod(m_client, "disconnectHost", Qt::QueuedConnection);
    }
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(3000);
    }
}

void SftpWindow::closeEvent(QCloseEvent* event)
{
    emit windowClosed(m_sessionId);
    QWidget::closeEvent(event);
}

void SftpWindow::setBusy(bool busy)
{
    m_busy = busy;
    m_upBtn->setEnabled(!busy);
    m_refreshBtn->setEnabled(!busy);
    m_uploadBtn->setEnabled(!busy);
    m_downloadBtn->setEnabled(!busy);
    m_mkdirBtn->setEnabled(!busy);
    m_deleteBtn->setEnabled(!busy);
    m_dotfilesBtn->setEnabled(!busy);
    m_pathEdit->setEnabled(!busy);
    m_table->setEnabled(!busy);
}

void SftpWindow::syncDotfilesToggle()
{
    // Button checked = showing hidden files.
    const bool showing = !m_hideDotfiles;
    m_dotfilesBtn->blockSignals(true);
    m_dotfilesBtn->setChecked(showing);
    m_dotfilesBtn->setIcon(QIcon(showing ? QStringLiteral(":/icons/filetypes/eye.svg")
                                         : QStringLiteral(":/icons/filetypes/eye-off.svg")));
    m_dotfilesBtn->setToolTip(showing ? QStringLiteral("hide dotfiles")
                                      : QStringLiteral("show dotfiles"));
    m_dotfilesBtn->blockSignals(false);
}

void SftpWindow::applyAppSettings()
{
    const bool hide = AppSettings::hideDotfiles();
    const bool compact = AppSettings::sftpCompactView();
    m_table->setColumnHidden(2, compact);
    m_table->verticalHeader()->setDefaultSectionSize(compact ? 22 : 26);
    if (m_hideDotfiles != hide) {
        m_hideDotfiles = hide;
        syncDotfilesToggle();
        if (!m_entries.isEmpty()) {
            populateTable();
        }
    } else {
        syncDotfilesToggle();
    }
    if (m_client) {
        const bool verbose = AppSettings::sftpVerboseLogging();
        QMetaObject::invokeMethod(m_client, "setVerboseEnabled", Qt::QueuedConnection,
                                  Q_ARG(bool, verbose));
    }
}

void SftpWindow::setHideDotfiles(bool hide)
{
    if (m_hideDotfiles == hide) {
        syncDotfilesToggle();
        return;
    }
    m_hideDotfiles = hide;
    QSettings s;
    s.setValue(QLatin1String(AppSettings::kHideDotfiles), m_hideDotfiles);
    syncDotfilesToggle();
    populateTable();
}

void SftpWindow::populateTable()
{
    m_table->setRowCount(0);
    int hidden = 0;

    // Parent directory entry (../) unless already at filesystem root.
    const bool atRoot = m_cwd.isEmpty() || m_cwd == QLatin1String("/");
    if (!atRoot) {
        m_table->insertRow(0);
        auto* nameItem = new QTableWidgetItem(QIcon(QStringLiteral(":/icons/filetypes/folder.svg")),
                                              QStringLiteral(".."));
        nameItem->setData(Qt::UserRole, true);       // isDir
        nameItem->setData(Qt::UserRole + 1, false);  // isLink
        nameItem->setData(Qt::UserRole + 2, true);   // isParent
        nameItem->setForeground(QColor(0x9a, 0x9a, 0x9a));
        auto* sizeItem = new QTableWidgetItem(QStringLiteral("—"));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* typeItem = new QTableWidgetItem(QStringLiteral("parent directory"));
        typeItem->setForeground(QColor(0x8a, 0x8a, 0x8a));
        m_table->setItem(0, 0, nameItem);
        m_table->setItem(0, 1, sizeItem);
        m_table->setItem(0, 2, typeItem);
    }

    for (const SftpEntry& e : m_entries) {
        if (e.name == QLatin1String(".") || e.name == QLatin1String("..")) {
            continue;
        }
        if (m_hideDotfiles && isDotfile(e.name)) {
            ++hidden;
            continue;
        }
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        auto* nameItem = new QTableWidgetItem(iconForEntry(e), e.name);
        nameItem->setData(Qt::UserRole, e.isDir);
        nameItem->setData(Qt::UserRole + 1, e.isLink);
        nameItem->setData(Qt::UserRole + 2, false);
        if (e.isDir) {
            nameItem->setForeground(QColor(0x9a, 0x9a, 0x9a));
        }
        auto* sizeItem = new QTableWidgetItem(e.isDir ? QStringLiteral("—") : formatSize(e.size));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto* typeItem = new QTableWidgetItem(e.longName);
        typeItem->setForeground(QColor(0x8a, 0x8a, 0x8a));
        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, sizeItem);
        m_table->setItem(row, 2, typeItem);
    }

    const int listed = m_table->rowCount() - (atRoot ? 0 : 1);
    if (hidden > 0 && m_hideDotfiles) {
        m_status->setText(QStringLiteral("%1 items · %2 hidden dotfiles").arg(listed).arg(hidden));
    } else {
        m_status->setText(QStringLiteral("%1 items").arg(listed));
    }
}

QString SftpWindow::joinRemote(const QString& name) const
{
    if (m_cwd == QLatin1String("/")) {
        return QStringLiteral("/") + name;
    }
    return m_cwd + QLatin1Char('/') + name;
}

QString SftpWindow::parentPath(const QString& path) const
{
    QString p = path;
    if (p.size() > 1 && p.endsWith(QLatin1Char('/'))) {
        p.chop(1);
    }
    const int slash = p.lastIndexOf(QLatin1Char('/'));
    if (slash <= 0) {
        return QStringLiteral("/");
    }
    return p.left(slash);
}

void SftpWindow::refresh()
{
    if (m_busy && m_cwd.isEmpty()) {
        return;
    }
    setBusy(true);
    QMetaObject::invokeMethod(m_client, "listDirectory", Qt::QueuedConnection,
                              Q_ARG(QString, m_cwd.isEmpty() ? m_pathEdit->text() : m_cwd));
}

void SftpWindow::goUp()
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    QMetaObject::invokeMethod(m_client, "listDirectory", Qt::QueuedConnection,
                              Q_ARG(QString, parentPath(m_cwd)));
}

void SftpWindow::openSelected()
{
    if (m_busy) {
        return;
    }
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    const int row = rows.first().row();
    auto* item = m_table->item(row, 0);
    if (!item) {
        return;
    }
    if (item->data(Qt::UserRole + 2).toBool()) {
        goUp();
        return;
    }
    const bool isDir = item->data(Qt::UserRole).toBool();
    if (isDir) {
        setBusy(true);
        QMetaObject::invokeMethod(m_client, "listDirectory", Qt::QueuedConnection,
                                  Q_ARG(QString, joinRemote(item->text())));
    } else {
        editRemoteFile(joinRemote(item->text()), item->text());
    }
}

void SftpWindow::openOrEditSelected()
{
    if (m_busy) {
        return;
    }
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    const int row = rows.first().row();
    auto* item = itemAtRow(row);
    if (!item) {
        return;
    }
    if (item->data(Qt::UserRole + 2).toBool()) { // parent ".."
        goUp();
        return;
    }
    const bool isDir = item->data(Qt::UserRole).toBool();
    if (isDir) {
        setBusy(true);
        QMetaObject::invokeMethod(m_client, "listDirectory", Qt::QueuedConnection,
                                  Q_ARG(QString, joinRemote(item->text())));
    } else {
        editRemoteFile(joinRemote(item->text()), item->text());
    }
}

void SftpWindow::downloadSelected()
{
    if (m_busy) {
        return;
    }
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        m_status->setText(QStringLiteral("select a file to download"));
        return;
    }

    QStringList files;
    for (const QModelIndex& idx : rows) {
        auto* item = m_table->item(idx.row(), 0);
        if (!item) {
            continue;
        }
        if (item->data(Qt::UserRole + 2).toBool()) {
            continue; // skip parent ".."
        }
        if (item->data(Qt::UserRole).toBool()) {
            continue; // skip dirs for now
        }
        files.push_back(item->text());
    }
    if (files.isEmpty()) {
        m_status->setText(QStringLiteral("select one or more files (not folders)"));
        return;
    }

    const QString destDir = QFileDialog::getExistingDirectory(this, QStringLiteral("Download to"));
    if (destDir.isEmpty()) {
        return;
    }

    m_transferQueue.clear();
    m_doneItems = 0;
    m_queuedItems = quint64(files.size());
    for (const QString& f : files) {
        m_transferQueue.push_back(QStringLiteral("dl:%1:%2")
                                      .arg(joinRemote(f), destDir + QLatin1Char('/') + f));
    }
    setBusy(true);
    m_progress->show();
    pumpQueue();
}

void SftpWindow::uploadFiles()
{
    if (m_busy) {
        return;
    }
    QStringList paths = QFileDialog::getOpenFileNames(this, QStringLiteral("Upload files"));
    if (paths.isEmpty()) {
        // Allow selecting a whole folder too.
        const QString folder = QFileDialog::getExistingDirectory(
            this, QStringLiteral("Upload folder"));
        if (folder.isEmpty()) {
            return;
        }
        paths = {folder};
    }
    uploadLocalPaths(paths);
}

void SftpWindow::uploadLocalPaths(const QStringList& paths)
{
    uploadLocalPathsTo(paths, m_cwd);
}

void SftpWindow::uploadLocalPathsTo(const QStringList& paths, const QString& remoteDir)
{
    if (m_busy || paths.isEmpty()) {
        return;
    }

    const QString targetDir = remoteDir.isEmpty() ? m_cwd : remoteDir;
    m_transferQueue.clear();
    m_doneItems = 0;
    m_queuedItems = 0;
    // Use RS (0x1E) as delimiter — never valid in Windows/Unix filenames.
    const QChar sep(0x1E);
    for (const QString& p : paths) {
        if (!QFileInfo::exists(p)) {
            continue;
        }
        ++m_queuedItems;
        m_transferQueue.push_back(QStringLiteral("uu:%1%2%3").arg(p, QString(sep), targetDir));
    }
    if (m_transferQueue.isEmpty()) {
        return;
    }

    setBusy(true);
    m_progress->show();
    pumpQueue();
}

void SftpWindow::pumpQueue()
{
    if (m_transferQueue.isEmpty()) {
        return;
    }
    const QString item = m_transferQueue.takeFirst();

    if (item.startsWith(QLatin1String("dl:"))) {
        const int sep = item.indexOf(QLatin1Char(':'), 3);
        const QString remote = item.mid(3, sep - 3);
        const QString local = item.mid(sep + 1);
        m_transferInFlight = true;
        QMetaObject::invokeMethod(m_client, "downloadFile", Qt::QueuedConnection,
                                  Q_ARG(QString, remote), Q_ARG(QString, local));
        return;
    }

    const QChar rs(0x1E);
    QString payload = item.mid(3);
    QString path;
    QString dir = m_cwd;
    const int rsPos = payload.indexOf(rs);
    if (rsPos != -1) {
        path = payload.left(rsPos);
        dir = payload.mid(rsPos + 1);
        if (dir.isEmpty()) {
            dir = m_cwd;
        }
    } else {
        // Legacy queues ("uu:<local>|<dir>" or "uu:<local>") — try RS first,
        // then fall back to '|' heuristic.
        if (payload.contains(QLatin1Char('|'))) {
            int best = -1;
            int scan = -1;
            while ((scan = payload.indexOf(QLatin1Char('|'), scan + 1)) != -1) {
                if (QFileInfo::exists(payload.left(scan))) {
                    best = scan;
                }
            }
            if (best == -1) {
                best = payload.lastIndexOf(QLatin1Char('|'));
            }
            if (best != -1) {
                path = payload.left(best);
                dir = payload.mid(best + 1);
                if (dir.isEmpty()) {
                    dir = m_cwd;
                }
            } else {
                path = payload;
            }
        } else {
            path = payload;
        }
    }
    m_transferInFlight = true;
    QMetaObject::invokeMethod(m_client, "uploadPath", Qt::QueuedConnection,
                              Q_ARG(QString, path), Q_ARG(QString, dir));
}

void SftpWindow::createFolder()
{
    if (m_busy) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("New folder"),
                                               QStringLiteral("Folder name:"), QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    setBusy(true);
    QMetaObject::invokeMethod(m_client, "makeDirectory", Qt::QueuedConnection,
                              Q_ARG(QString, joinRemote(name.trimmed())));
}

void SftpWindow::deleteSelected()
{
    if (m_busy) {
        return;
    }
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    auto* item = m_table->item(rows.first().row(), 0);
    if (!item) {
        return;
    }
    if (item->data(Qt::UserRole + 2).toBool()) {
        m_status->setText(QStringLiteral("cannot delete parent directory"));
        return;
    }
    const bool isDir = item->data(Qt::UserRole).toBool();
    const QString name = item->text();
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Delete"),
        QStringLiteral("Delete %1 “%2”?").arg(isDir ? QStringLiteral("folder") : QStringLiteral("file"), name));
    if (answer != QMessageBox::Yes) {
        return;
    }
    setBusy(true);
    QMetaObject::invokeMethod(m_client, "removePath", Qt::QueuedConnection,
                              Q_ARG(QString, joinRemote(name)), Q_ARG(bool, isDir));
}

QTableWidgetItem* SftpWindow::itemAtRow(int row) const
{
    if (row < 0 || row >= m_table->rowCount()) {
        return nullptr;
    }
    return m_table->item(row, 0);
}

void SftpWindow::showContextMenu(const QPoint& globalPos)
{
    const int row = m_table->rowAt(m_table->viewport()->mapFromGlobal(globalPos).y());
    if (row < 0) {
        return;
    }
    m_table->selectRow(row);

    auto* item = itemAtRow(row);
    if (!item) {
        return;
    }
    const bool isParent = item->data(Qt::UserRole + 2).toBool();
    const bool isDir = item->data(Qt::UserRole).toBool();

    auto* menu = new QMenu(this);
    if (!isParent) {
        if (isDir) {
            auto* open = menu->addAction(QStringLiteral("Open"));
            connect(open, &QAction::triggered, this, [this, row]() {
                auto* it = itemAtRow(row);
                if (!it || m_busy) {
                    return;
                }
                setBusy(true);
                QMetaObject::invokeMethod(m_client, "listDirectory", Qt::QueuedConnection,
                                          Q_ARG(QString, joinRemote(it->text())));
            });
        } else {
            auto* view = menu->addAction(QStringLiteral("Open / View"));
            connect(view, &QAction::triggered, this, [this, row]() {
                auto* it = itemAtRow(row);
                if (it) {
                    editRemoteFile(joinRemote(it->text()), it->text());
                }
            });
        }
        auto* download = menu->addAction(QStringLiteral("Download"));
        connect(download, &QAction::triggered, this, &SftpWindow::downloadSelected);

        menu->addSeparator();

        auto* rename = menu->addAction(QStringLiteral("Rename"));
        connect(rename, &QAction::triggered, this, &SftpWindow::renameSelected);

        auto* del = menu->addAction(QStringLiteral("Delete"));
        connect(del, &QAction::triggered, this, &SftpWindow::deleteSelected);
    }
    const bool hasAction = !menu->isEmpty();
    if (hasAction) {
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(globalPos);
    } else {
        delete menu;
    }
}

void SftpWindow::renameSelected()
{
    if (m_busy) {
        return;
    }
    const auto rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        return;
    }
    auto* item = itemAtRow(rows.first().row());
    if (!item || item->data(Qt::UserRole + 2).toBool()) {
        m_status->setText(QStringLiteral("cannot rename parent directory"));
        return;
    }
    const QString oldName = item->text();
    bool ok = false;
    const QString newName =
        QInputDialog::getText(this, QStringLiteral("Rename"), QStringLiteral("New name:"),
                              QLineEdit::Normal, oldName, &ok);
    if (!ok || newName.trimmed().isEmpty() || newName.trimmed() == oldName) {
        return;
    }
    const bool edited = m_editedFiles.contains(joinRemote(oldName));
    setBusy(true);
    QMetaObject::invokeMethod(m_client, "renamePath", Qt::QueuedConnection,
                              Q_ARG(QString, joinRemote(oldName)),
                              Q_ARG(QString, joinRemote(newName.trimmed())));
    // If the renamed file was being auto-edited, keep tracking it under the new path.
    if (edited) {
        const EditedRemoteFile prev = m_editedFiles.take(joinRemote(oldName));
        EditedRemoteFile updated = prev;
        updated.remotePath = joinRemote(newName.trimmed());
        m_editedFiles.insert(updated.remotePath, updated);
    }
}

void SftpWindow::editRemoteFile(const QString& remotePath, const QString& fileName)
{
    if (m_busy) {
        return;
    }
    if (m_editedFiles.contains(remotePath)) {
        // Already open for editing — re-open the local copy.
        const EditedRemoteFile e = m_editedFiles.value(remotePath);
        QDesktopServices::openUrl(QUrl::fromLocalFile(e.localPath));
        m_status->setText(QStringLiteral("re-opened %1 (auto-sync active)").arg(remotePath));
        return;
    }

    const QString tempBase =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString localPath =
        tempBase + QLatin1Char('/') + QStringLiteral("clientosh-edit-")
        + QString::number(QCoreApplication::applicationPid()) + QLatin1Char('-')
        + fileName;

    QFile::remove(localPath); // ensure a clean download target
    setBusy(true);
    m_status->setText(QStringLiteral("downloading %1 for editing…").arg(remotePath));
    m_pendingEditDownload = true;
    m_pendingEditRemote = remotePath;
    m_pendingEditLocal = localPath;
    QMetaObject::invokeMethod(m_client, "downloadFile", Qt::QueuedConnection,
                              Q_ARG(QString, remotePath), Q_ARG(QString, localPath));
}

void SftpWindow::onDownloadedForEdit(bool ok, const QString& message, const QString& remotePath,
                                     const QString& localPath)
{
    if (!ok) {
        setBusy(false);
        m_status->setText(message);
        QMessageBox::warning(this, QStringLiteral("sftp"), message);
        return;
    }
    QFileInfo fi(localPath);
    if (!fi.exists() || fi.isDir()) {
        setBusy(false);
        m_status->setText(QStringLiteral("edit download produced no file: %1").arg(localPath));
        return;
    }

    EditedRemoteFile e;
    e.remotePath = remotePath;
    e.localPath = localPath;
    e.lastModified = fi.lastModified();
    m_editedFiles.insert(remotePath, e);

    if (!m_fileWatcher->files().contains(localPath)) {
        m_fileWatcher->addPath(localPath);
    }
    setBusy(false);
    QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
    m_status->setText(QStringLiteral("now editing %1 (changes auto-upload)").arg(remotePath));
    emit debugLog(QStringLiteral("edit: opened '%1' via '%2'").arg(remotePath, localPath));
}

void SftpWindow::checkEditedFiles()
{
    if (m_editedFiles.isEmpty()) {
        return;
    }
    for (auto it = m_editedFiles.begin(); it != m_editedFiles.end();) {
        const QString remotePath = it.key();
        EditedRemoteFile& e = it.value();
        QFileInfo fi(e.localPath);
        if (!fi.exists()) {
            // Local copy gone (app closed / user deleted) — stop tracking.
            it = m_editedFiles.erase(it);
            continue;
        }
        const QDateTime mtime = fi.lastModified();
        if (mtime == e.lastModified) {
            ++it;
            continue;
        }
        e.lastModified = mtime;
        const QString localPath = e.localPath;
        QMetaObject::invokeMethod(m_client, "uploadFile", Qt::QueuedConnection,
                                  Q_ARG(QString, localPath), Q_ARG(QString, remotePath));
        emit debugLog(QStringLiteral("edit: auto-uploading changed '%1'").arg(remotePath));
        m_status->setText(QStringLiteral("auto-uploading %1…").arg(remotePath));
        ++it;
    }
}

void SftpWindow::onEditedFileUploaded(bool ok, const QString& message, const QString& remotePath)
{
    if (ok) {
        emit debugLog(QStringLiteral("edit: auto-upload complete '%1'").arg(remotePath));
        m_status->setText(QStringLiteral("auto-uploaded %1").arg(remotePath));
    } else {
        emit debugLog(QStringLiteral("edit: auto-upload FAILED '%1': %2").arg(remotePath, message));
        m_status->setText(QStringLiteral("auto-upload failed %1: %2").arg(remotePath, message));
    }
}

void SftpWindow::stopWatchingEditedFile(const QString& remotePath)
{
    const auto it = m_editedFiles.constFind(remotePath);
    if (it != m_editedFiles.constEnd()) {
        if (m_fileWatcher && m_fileWatcher->files().contains(it->localPath)) {
            m_fileWatcher->removePath(it->localPath);
        }
        m_editedFiles.erase(m_editedFiles.find(remotePath));
    }
}

void SftpWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragEnterEvent(event);
}

void SftpWindow::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragMoveEvent(event);
}

void SftpWindow::dropEvent(QDropEvent* event)
{
    if (m_busy) {
        event->ignore();
        return;
    }
    if (!event->mimeData()->hasUrls()) {
        QWidget::dropEvent(event);
        return;
    }

    QString dropRemoteDir = m_cwd;
    const QPoint pos = event->position().toPoint();
    QWidget* vp = m_table->viewport();
    const QPoint vpPos = vp->mapFrom(this, pos);
    if (vp->rect().contains(vpPos)) {
        if (QTableWidgetItem* it = m_table->itemAt(vpPos)) {
            if (it->data(Qt::UserRole).toBool() && !it->data(Qt::UserRole + 2).toBool()) {
                dropRemoteDir = joinRemote(it->text());
            }
        }
    }

    QStringList locals;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            locals.push_back(url.toLocalFile());
        }
    }
    if (locals.isEmpty()) {
        return;
    }
    uploadLocalPathsTo(locals, dropRemoteDir);
    event->acceptProposedAction();
}
