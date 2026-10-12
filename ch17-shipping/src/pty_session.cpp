#include "pty_session.h"

#include <glib-unix.h>

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <util.h>
#else
#include <pty.h>
#endif

PtySession::~PtySession() {
    // Order matters: remove every main-loop source that holds `this` first,
    // then release the fd and the child.
    if (read_watch_)
        g_source_remove(read_watch_);
    if (child_watch_)
        g_source_remove(child_watch_);
    if (fd_ >= 0)
        close(fd_);
    if (pid_ > 0) {
        kill(pid_, SIGHUP);
        // Still reap it, or it stays a zombie. This watch holds nothing of ours.
        g_child_watch_add(pid_, [](GPid pid, gint, gpointer) { g_spawn_close_pid(pid); }, nullptr);
    }
    g_debug("~PtySession");
}

bool PtySession::start(const std::vector<std::string> &argv, int cols, int rows,
                       const std::string &term) {
    if (argv.empty()) {
        error_ = "no command";
        return false;
    }
    // Build the child's argv before forking: after fork, only async-signal-safe
    // calls are allowed until exec.
    std::vector<char *> cargv;
    for (const std::string &a : argv)
        cargv.push_back(const_cast<char *>(a.c_str()));
    cargv.push_back(nullptr);
    const std::string term_env = "TERM=" + term;

    struct winsize ws = {};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);

    int master = -1;
    pid_t pid = forkpty(&master, nullptr, nullptr, &ws);
    if (pid < 0) {
        error_ = std::string("forkpty: ") + std::strerror(errno);
        return false;
    }
    if (pid == 0) {  // child
        putenv(const_cast<char *>(term_env.c_str()));
        execvp(cargv[0], cargv.data());
        const char msg[] = "exec failed\r\n";
        ssize_t ignored = ::write(STDERR_FILENO, msg, sizeof msg - 1);
        (void)ignored;
        _exit(127);
    }

    fd_ = master;
    pid_ = pid;
    fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | O_NONBLOCK);
    fcntl(fd_, F_SETFD, FD_CLOEXEC);

    // Idle priority, below GTK's redraw (GDK_PRIORITY_REDRAW). GLib only
    // dispatches the highest-priority sources that are ready; at the default
    // priority a pty that is always readable (cat of a big file) would win
    // every iteration and the window would stop repainting until it drained.
    read_watch_ = g_unix_fd_add_full(G_PRIORITY_DEFAULT_IDLE, fd_,
                                     static_cast<GIOCondition>(G_IO_IN | G_IO_HUP | G_IO_ERR),
                                     on_readable, this, nullptr);
    child_watch_ = g_child_watch_add(pid_, on_child_exit, this);
    return true;
}

// Read until the pty is drained or the budget is spent, then give the main
// loop a turn. Without the budget, `cat` of a huge file would never return
// here and the window would stop redrawing and taking input.
gboolean PtySession::on_readable(gint, GIOCondition, gpointer data) {
    auto *self = static_cast<PtySession *>(data);
    char buf[16384];
    gsize total = 0;
    while (total < kReadBudget) {
        const ssize_t n = read(self->fd_, buf, sizeof buf);
        if (n > 0) {
            total += static_cast<gsize>(n);
            if (self->on_output_)
                self->on_output_(buf, static_cast<gsize>(n));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return G_SOURCE_CONTINUE;  // drained; wait for more
        if (n < 0 && errno == EINTR)
            continue;
        // 0, or EIO on Linux: the child side has closed. Stop watching.
        // Returning G_SOURCE_REMOVE destroys the source, so forget its id.
        self->read_watch_ = 0;
        return G_SOURCE_REMOVE;
    }
    g_debug("read budget spent after %zu bytes; yielding", total);
    return G_SOURCE_CONTINUE;
}

void PtySession::on_child_exit(GPid pid, gint status, gpointer data) {
    auto *self = static_cast<PtySession *>(data);
    g_spawn_close_pid(pid);
    self->child_watch_ = 0;  // this source is finished
    self->pid_ = -1;
    if (self->on_exit_)
        self->on_exit_(status);  // may delete self: touch nothing after this
}

void PtySession::write(const char *data, gsize len) {
    if (fd_ < 0)
        return;
    while (len > 0) {
        const ssize_t n = ::write(fd_, data, len);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return;
        data += n;
        len -= static_cast<gsize>(n);
    }
}

void PtySession::resize(int cols, int rows) {
    if (fd_ < 0)
        return;
    struct winsize ws = {};
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_row = static_cast<unsigned short>(rows);
    ioctl(fd_, TIOCSWINSZ, &ws);
}
