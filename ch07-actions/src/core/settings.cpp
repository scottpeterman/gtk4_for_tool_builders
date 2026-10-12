#include "core/settings.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <algorithm>

namespace {

int get_int(GKeyFile *kf, const char *group, const char *key, int fallback, int lo, int hi) {
    GError *err = nullptr;
    int v = g_key_file_get_integer(kf, group, key, &err);
    if (err) {
        g_error_free(err);
        return fallback;
    }
    return std::clamp(v, lo, hi);
}

bool get_bool(GKeyFile *kf, const char *group, const char *key, bool fallback) {
    GError *err = nullptr;
    gboolean v = g_key_file_get_boolean(kf, group, key, &err);
    if (err) {
        g_error_free(err);
        return fallback;
    }
    return v;
}

std::string get_str(GKeyFile *kf, const char *group, const char *key, const std::string &fallback) {
    char *v = g_key_file_get_string(kf, group, key, nullptr);
    std::string out = (v && *v) ? v : fallback;
    g_free(v);
    return out;
}

}  // namespace

std::string config_path(const std::string &name) {
    char *p = g_build_filename(g_get_user_config_dir(), "gtklab", name.c_str(), nullptr);
    std::string out = p;
    g_free(p);
    return out;
}

Settings parse_settings(const std::string &text) {
    Settings s;
    GKeyFile *kf = g_key_file_new();
    if (g_key_file_load_from_data(kf, text.c_str(), text.size(), G_KEY_FILE_NONE, nullptr)) {
        s.width = get_int(kf, "window", "width", s.width, 400, 10000);
        s.height = get_int(kf, "window", "height", s.height, 300, 10000);
        s.sidebar_position = get_int(kf, "window", "sidebar-position", s.sidebar_position, 120, 2000);
        s.sidebar_visible = get_bool(kf, "window", "sidebar-visible", s.sidebar_visible);
        s.app_variant = get_str(kf, "appearance", "app-variant", s.app_variant);
        if (s.app_variant != "light" && s.app_variant != "dark")
            s.app_variant = "light";
        s.terminal_theme = get_str(kf, "appearance", "terminal-theme", s.terminal_theme);
        s.font_size = get_int(kf, "terminal", "font-size", s.font_size, 6, 48);
    }
    g_key_file_unref(kf);
    return s;
}

Settings load_settings(const std::string &path) {
    char *contents = nullptr;
    gsize len = 0;
    if (!g_file_get_contents(path.c_str(), &contents, &len, nullptr))
        return Settings{};
    std::string text(contents, len);
    g_free(contents);
    return parse_settings(text);
}

std::string settings_to_text(const Settings &s) {
    GKeyFile *kf = g_key_file_new();
    g_key_file_set_integer(kf, "window", "width", s.width);
    g_key_file_set_integer(kf, "window", "height", s.height);
    g_key_file_set_integer(kf, "window", "sidebar-position", s.sidebar_position);
    g_key_file_set_boolean(kf, "window", "sidebar-visible", s.sidebar_visible);
    g_key_file_set_string(kf, "appearance", "app-variant", s.app_variant.c_str());
    g_key_file_set_string(kf, "appearance", "terminal-theme", s.terminal_theme.c_str());
    g_key_file_set_integer(kf, "terminal", "font-size", s.font_size);
    char *data = g_key_file_to_data(kf, nullptr, nullptr);
    std::string out = data;
    g_free(data);
    g_key_file_unref(kf);
    return out;
}

bool save_settings(const std::string &path, const Settings &s, std::string &error) {
    char *dir = g_path_get_dirname(path.c_str());
    g_mkdir_with_parents(dir, 0700);
    g_free(dir);

    const std::string text = settings_to_text(s);
    GError *err = nullptr;
    // g_file_set_contents writes a temp file and renames it: no torn file on a crash.
    if (!g_file_set_contents(path.c_str(), text.c_str(), static_cast<gssize>(text.size()), &err)) {
        error = err->message;
        g_error_free(err);
        return false;
    }
    return true;
}
