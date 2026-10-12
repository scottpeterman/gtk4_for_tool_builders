// GtklabSessionItem: a GObject wrapper so the core's plain C++ Session and
// Folder can live in a GListModel. Your first GObject subclass.
#pragma once

#include <glib-object.h>

#include "core/session.h"

G_BEGIN_DECLS

#define GTKLAB_TYPE_SESSION_ITEM (gtklab_session_item_get_type())
G_DECLARE_FINAL_TYPE(GtklabSessionItem, gtklab_session_item, GTKLAB, SESSION_ITEM, GObject)

G_END_DECLS

// Constructors copy their argument; the item owns the copy.
GtklabSessionItem *gtklab_session_item_new_session(const Session &s);
GtklabSessionItem *gtklab_session_item_new_folder(const Folder &f);

bool gtklab_session_item_is_folder(GtklabSessionItem *item);
const Session &gtklab_session_item_session(GtklabSessionItem *item);
const Folder &gtklab_session_item_folder(GtklabSessionItem *item);
