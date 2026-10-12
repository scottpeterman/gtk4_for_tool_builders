// SessionLog: raw pty output appended to a file, one per tab while logging
// is on. The file closes (with a footer) when the SessionLog is destroyed.
#pragma once

#include <glib.h>
#include <glib/gstdio.h>

#include <cstdio>
#include <string>

class SessionLog {
public:
    // $XDG_DATA_HOME/gtklab/logs/<session>-<YYYYmmdd-HHMMSS>.log
    explicit SessionLog(const std::string &session_name) {
        char *dir = g_build_filename(g_get_user_data_dir(), "gtklab", "logs", nullptr);
        g_mkdir_with_parents(dir, 0700);
        GDateTime *now = g_date_time_new_now_local();
        char *stamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
        const std::string name = session_name + "-" + stamp + ".log";
        char *path = g_build_filename(dir, name.c_str(), nullptr);
        path_ = path;
        file_ = g_fopen(path, "ab");
        if (file_) {
            char *header = g_date_time_format(now, "--- log started %F %T ---\n");
            std::fputs(header, file_);
            g_free(header);
        }
        g_free(path);
        g_free(stamp);
        g_date_time_unref(now);
        g_free(dir);
    }

    ~SessionLog() {
        if (!file_)
            return;
        GDateTime *now = g_date_time_new_now_local();
        char *footer = g_date_time_format(now, "\n--- log closed %F %T ---\n");
        std::fputs(footer, file_);
        g_free(footer);
        g_date_time_unref(now);
        std::fclose(file_);
        g_debug("~SessionLog %s", path_.c_str());
    }

    SessionLog(const SessionLog &) = delete;
    SessionLog &operator=(const SessionLog &) = delete;

    bool ok() const { return file_ != nullptr; }
    const std::string &path() const { return path_; }

    // Raw bytes, escape sequences and all. tmssh cleans these into plain
    // text (its LogCleaner); gtklab keeps them as received.
    void write(const char *data, gsize len) {
        if (file_)
            std::fwrite(data, 1, len, file_);
    }

private:
    std::FILE *file_ = nullptr;
    std::string path_;
};
