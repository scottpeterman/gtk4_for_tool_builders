// Themes from the resource bundle, applied to terminals. Needs GTK; the
// parsing it relies on is in core/theme.
#pragma once

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "core/theme.h"
#include "gtkpyte-terminal.h"

// Every theme under /io/github/gtklab/themes, sorted by label. Files that
// fail to parse are skipped and named in `error`.
std::vector<Theme> load_themes(std::string &error);

void apply_theme(GtkpyteTerminal *terminal, const Theme &theme);
