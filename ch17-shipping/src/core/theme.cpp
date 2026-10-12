#include "core/theme.h"

#include <glib.h>

#include <cctype>

const std::array<const char *, 16> &theme_palette_keys() {
    static const std::array<const char *, 16> keys = {
        "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white",
        "bright_black", "bright_red", "bright_green", "bright_yellow",
        "bright_blue", "bright_magenta", "bright_cyan", "bright_white"};
    return keys;
}

static bool is_hex_color(const std::string &s) {
    if (s.size() != 7 || s[0] != '#')
        return false;
    for (size_t i = 1; i < 7; i++)
        if (!std::isxdigit(static_cast<unsigned char>(s[i])))
            return false;
    return true;
}

static bool get_color(GKeyFile *kf, const char *key, std::string &out, std::string &error) {
    char *v = g_key_file_get_string(kf, "colors", key, nullptr);
    out = v ? v : "";
    g_free(v);
    if (!is_hex_color(out)) {
        error = std::string("colour '") + key + "' missing or not #rrggbb";
        return false;
    }
    return true;
}

bool parse_theme(const std::string &name, const std::string &text, Theme &out, std::string &error) {
    GKeyFile *kf = g_key_file_new();
    GError *err = nullptr;
    if (!g_key_file_load_from_data(kf, text.c_str(), text.size(), G_KEY_FILE_NONE, &err)) {
        error = err->message;
        g_error_free(err);
        g_key_file_unref(kf);
        return false;
    }
    Theme t;
    t.name = name;
    char *label = g_key_file_get_string(kf, "theme", "label", nullptr);
    t.label = label ? label : name;
    g_free(label);
    char *type = g_key_file_get_string(kf, "theme", "type", nullptr);
    t.dark = !type || g_strcmp0(type, "light") != 0;
    g_free(type);

    bool ok = get_color(kf, "background", t.background, error) &&
              get_color(kf, "foreground", t.foreground, error) &&
              get_color(kf, "cursor", t.cursor, error) &&
              get_color(kf, "selection", t.selection, error);
    for (size_t i = 0; ok && i < 16; i++)
        ok = get_color(kf, theme_palette_keys()[i], t.palette[i], error);
    g_key_file_unref(kf);
    if (ok)
        out = t;
    return ok;
}
