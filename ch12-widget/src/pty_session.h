// PtySession: a child process on a pseudo-terminal, driven by the GLib main
// loop. Output arrives through on_output; the session never touches a widget.
#pragma once

#include <glib.h>
#include <sys/types.h>

#include <functional>
#include <string>
#include <vector>

class PtySession {
public:
    using OutputHandler = std::function<void(const char *data, gsize len)>;
    using ExitHandler = std::function<void(int wait_status)>;

    PtySession() = default;
    ~PtySession();

    PtySession(const PtySession &) = delete;
    PtySession &operator=(const PtySession &) = delete;

    // Start argv[0] (searched on PATH) on a new pty of cols x rows. `term`
    // becomes $TERM in the child. Returns false and sets error() on failure.
    bool start(const std::vector<std::string> &argv, int cols, int rows,
               const std::string &term = "dumb");

    void set_output_handler(OutputHandler h) { on_output_ = std::move(h); }
    // Runs once, when the child has exited. It may delete this session.
    void set_exit_handler(ExitHandler h) { on_exit_ = std::move(h); }

    // Write bytes to the child. A short write drops the rest (Chapter 14's
    // gtkpyte session queues them instead).
    void write(const char *data, gsize len);
    void resize(int cols, int rows);

    const std::string &error() const { return error_; }
    pid_t pid() const { return pid_; }

    // Bytes read per wakeup before yielding to the main loop.
    static constexpr gsize kReadBudget = 512 * 1024;

private:
    static gboolean on_readable(gint fd, GIOCondition cond, gpointer self);
    static void on_child_exit(GPid pid, gint status, gpointer self);

    int fd_ = -1;
    pid_t pid_ = -1;
    guint read_watch_ = 0;
    guint child_watch_ = 0;
    std::string error_;
    OutputHandler on_output_;
    ExitHandler on_exit_;
};
