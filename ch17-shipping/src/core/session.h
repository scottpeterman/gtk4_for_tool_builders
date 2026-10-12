// Session model: plain C++ plus GLib's GKeyFile. Never includes GTK.
#pragma once

#include <string>
#include <vector>

struct Session {
    std::string name;
    std::string folder;
    std::string host;
    std::string user;
    int port = 22;
    bool local = false;
};

struct Folder {
    std::string name;
    std::vector<Session> sessions;
};

// Parse a sessions file held in memory. One [group] per session:
//
//   [core1]
//   folder=Core
//   host=10.0.0.1
//   user=admin
//   port=2222
//
//   [localhost]
//   folder=Local
//   local=true
//
// Folders and sessions keep file order. On a parse error, returns an empty
// list and sets `error`.
std::vector<Folder> parse_sessions(const std::string &text, std::string &error);

// Read and parse a sessions file from disk.
std::vector<Folder> load_sessions(const std::string &path, std::string &error);

// The text of a first-run sessions file: one folder with a localhost entry.
std::string default_sessions_text();

// argv for the session: ssh for remote sessions, the user's shell for local.
// `shell` is passed in (normally $SHELL) so tests do not depend on the host.
std::vector<std::string> ssh_argv(const Session &s, const std::string &shell = "/bin/sh");

// Folders filtered to sessions whose name, host or user contains `text`
// (case-insensitive). Empty text returns the input. Folders with no
// matching sessions are dropped.
std::vector<Folder> filter_sessions(const std::vector<Folder> &folders, const std::string &text);
