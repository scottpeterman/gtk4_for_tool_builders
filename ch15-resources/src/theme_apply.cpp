#include "theme_apply.h"

#include <algorithm>

static const char *kThemeDir = "/io/github/gtklab/themes";

std::vector<Theme> load_themes(std::string &error) {
    std::vector<Theme> themes;
    GError *err = nullptr;
    char **names = g_resources_enumerate_children(kThemeDir, G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
    if (!names) {
        error = std::string("themes: ") + err->message;
        g_error_free(err);
        return themes;
    }
    for (char **n = names; *n; n++) {
        if (!g_str_has_suffix(*n, ".ini"))
            continue;
        const std::string file = *n;
        const std::string path = std::string(kThemeDir) + "/" + file;
        GBytes *bytes = g_resources_lookup_data(path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr);
        if (!bytes)
            continue;
        gsize len = 0;
        const char *data = static_cast<const char *>(g_bytes_get_data(bytes, &len));
        Theme t;
        std::string perr;
        if (parse_theme(file.substr(0, file.size() - 4), std::string(data, len), t, perr))
            themes.push_back(t);
        else
            error += file + ": " + perr + "\n";
        g_bytes_unref(bytes);
    }
    g_strfreev(names);
    // Case-insensitive, so labels sort the way a reader expects.
    std::sort(themes.begin(), themes.end(), [](const Theme &a, const Theme &b) {
        char *fa = g_utf8_casefold(a.label.c_str(), -1);
        char *fb = g_utf8_casefold(b.label.c_str(), -1);
        const bool less = g_strcmp0(fa, fb) < 0;
        g_free(fa);
        g_free(fb);
        return less;
    });
    return themes;
}

static GdkRGBA rgba(const std::string &hex) {
    GdkRGBA c = {0, 0, 0, 1};
    gdk_rgba_parse(&c, hex.c_str());
    return c;
}

void apply_theme(GtkpyteTerminal *terminal, const Theme &theme) {
    GdkRGBA fg = rgba(theme.foreground);
    GdkRGBA bg = rgba(theme.background);
    GdkRGBA palette[16];
    for (size_t i = 0; i < 16; i++)
        palette[i] = rgba(theme.palette[i]);
    gtkpyte_terminal_set_colors(terminal, &fg, &bg, palette, 16);
    GdkRGBA cursor = rgba(theme.cursor);
    gtkpyte_terminal_set_color_cursor(terminal, &cursor);
    GdkRGBA sel = rgba(theme.selection);
    sel.alpha = 0.6f;  // painted over the text
    gtkpyte_terminal_set_color_selection(terminal, &sel);
}
