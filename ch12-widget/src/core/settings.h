// App settings: plain C++ plus GLib's GKeyFile. Never includes GTK.
#pragma once

#include <string>

struct Settings {
    int width = 900;
    int height = 600;
    int sidebar_position = 260;
    bool sidebar_visible = true;
    std::string app_variant = "light";     // "light" | "dark"
    std::string terminal_theme = "dark";   // a theme name (Chapter 15)
    int font_size = 11;                    // points (Chapter 14)
};

// $XDG_CONFIG_HOME/gtklab/<name> (normally ~/.config/gtklab/<name>).
std::string config_path(const std::string &name);

// Missing file or missing keys fall back to the defaults above. Values out of
// range are clamped, so a hand-edited file cannot produce a 10x10 window.
Settings parse_settings(const std::string &text);
Settings load_settings(const std::string &path);

std::string settings_to_text(const Settings &s);
bool save_settings(const std::string &path, const Settings &s, std::string &error);
