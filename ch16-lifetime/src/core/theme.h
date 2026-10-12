// Terminal colour themes: parsing only, no GTK.
#pragma once

#include <array>
#include <string>

struct Theme {
    std::string name;   // stable id, from the file name
    std::string label;  // shown in menus
    bool dark = true;
    std::string background, foreground, cursor, selection;  // "#rrggbb"
    std::array<std::string, 16> palette;  // ANSI 0-15
};

// Parse a theme file (GKeyFile format):
//
//   [theme]
//   label=Nord
//   type=dark
//   [colors]
//   background=#2e3440
//   ...            (foreground, cursor, selection, and the 16 below)
//   black=...  red=...  green=...  yellow=...  blue=...  magenta=...  cyan=...  white=...
//   bright_black=...  ...  bright_white=...
//
// Every colour must be #rrggbb. Returns false and sets `error` otherwise.
bool parse_theme(const std::string &name, const std::string &text, Theme &out, std::string &error);

// The 16 key names in palette order.
const std::array<const char *, 16> &theme_palette_keys();
