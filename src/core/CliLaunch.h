#pragma once

#include "SessionProfile.h"

#include <QString>

class QCommandLineParser;

namespace CliLaunch {

struct Request {
    bool launch = false;
    QString error;
    SessionProfile profile;
    /** When true and mode is SSH, open a standalone SFTP pane as well. */
    bool openSftpWithSsh = false;
};

void configureParser(QCommandLineParser& parser);
void addStandardOptions(QCommandLineParser& parser);
Request parse(const QCommandLineParser& parser);

/**
 * Parse argv without starting the GUI (help, version, connect errors).
 * Returns an exit code (>= 0) to terminate, or -1 to continue into the GUI.
 * When returning -1, @p outRequest and @p outLaunch are filled when a connect
 * command was parsed successfully.
 */
int runHeadlessPhase(int argc, char* argv[], Request* outRequest, bool* outLaunch, bool* verboseOut);

} // namespace CliLaunch
