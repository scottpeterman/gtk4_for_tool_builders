#include "core/session.h"

#include <glib.h>

#include <algorithm>
#include <cctype>

namespace {

std::string get_string(GKeyFile *kf, const char *group, const char *key) {
    char *v = g_key_file_get_string(kf, group, key, nullptr);
    std::string out = v ? v : "";
    g_free(v);
    return out;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool contains_ci(const std::string &hay, const std::string &needle_lower) {
    return lower(hay).find(needle_lower) != std::string::npos;
}

}  // namespace

std::vector<Folder> parse_sessions(const std::string &text, std::string &error) {
    std::vector<Folder> folders;
    error.clear();

    GKeyFile *kf = g_key_file_new();
    GError *err = nullptr;
    if (!g_key_file_load_from_data(kf, text.c_str(), text.size(), G_KEY_FILE_NONE, &err)) {
        error = err->message;
        g_error_free(err);
        g_key_file_unref(kf);
        return folders;
    }

    gsize n = 0;
    char **groups = g_key_file_get_groups(kf, &n);
    for (gsize i = 0; i < n; i++) {
        Session s;
        s.name = groups[i];
        s.folder = get_string(kf, groups[i], "folder");
        if (s.folder.empty())
            s.folder = "Sessions";
        s.host = get_string(kf, groups[i], "host");
        s.user = get_string(kf, groups[i], "user");
        s.local = g_key_file_get_boolean(kf, groups[i], "local", nullptr);

        GError *perr = nullptr;
        int port = g_key_file_get_integer(kf, groups[i], "port", &perr);
        if (perr == nullptr) {
            if (port < 1 || port > 65535) {
                error = "session '" + s.name + "': port out of range";
                folders.clear();
                break;
            }
            s.port = port;
        } else if (perr->code == G_KEY_FILE_ERROR_INVALID_VALUE) {
            error = "session '" + s.name + "': port is not a number";
            g_error_free(perr);
            folders.clear();
            break;
        } else {
            g_error_free(perr);  // key missing: keep the default 22
        }

        if (!s.local && s.host.empty()) {
            error = "session '" + s.name + "': no host";
            folders.clear();
            break;
        }

        auto it = std::find_if(folders.begin(), folders.end(),
                               [&](const Folder &f) { return f.name == s.folder; });
        if (it == folders.end()) {
            folders.push_back(Folder{s.folder, {}});
            it = folders.end() - 1;
        }
        it->sessions.push_back(std::move(s));
    }
    g_strfreev(groups);
    g_key_file_unref(kf);
    return folders;
}

std::vector<Folder> load_sessions(const std::string &path, std::string &error) {
    char *contents = nullptr;
    gsize len = 0;
    GError *err = nullptr;
    if (!g_file_get_contents(path.c_str(), &contents, &len, &err)) {
        error = err->message;
        g_error_free(err);
        return {};
    }
    std::string text(contents, len);
    g_free(contents);
    return parse_sessions(text, error);
}

std::string default_sessions_text() {
    return "# gtklab sessions: one [group] per session\n"
           "[localhost]\n"
           "folder=Local\n"
           "local=true\n";
}

std::vector<std::string> ssh_argv(const Session &s, const std::string &shell) {
    if (s.local)
        return {shell.empty() ? "/bin/sh" : shell};

    std::vector<std::string> argv{"ssh"};
    if (s.port != 22) {
        argv.push_back("-p");
        argv.push_back(std::to_string(s.port));
    }
    argv.push_back(s.user.empty() ? s.host : s.user + "@" + s.host);
    return argv;
}

std::vector<Folder> filter_sessions(const std::vector<Folder> &folders, const std::string &text) {
    if (text.empty())
        return folders;
    const std::string needle = lower(text);
    std::vector<Folder> out;
    for (const Folder &f : folders) {
        Folder kept{f.name, {}};
        for (const Session &s : f.sessions) {
            if (contains_ci(s.name, needle) || contains_ci(s.host, needle) ||
                contains_ci(s.user, needle))
                kept.sessions.push_back(s);
        }
        if (!kept.sessions.empty())
            out.push_back(std::move(kept));
    }
    return out;
}
