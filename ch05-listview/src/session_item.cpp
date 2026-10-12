#include "session_item.h"

#include <new>

struct _GtklabSessionItem {
    GObject parent_instance;

    // GObject zero-fills the instance and never runs C++ constructors, so
    // the C++ members are built with placement new in init and destroyed
    // by hand in finalize.
    bool is_folder;
    Session session;
    Folder folder;
};

G_DEFINE_FINAL_TYPE(GtklabSessionItem, gtklab_session_item, G_TYPE_OBJECT)

static void gtklab_session_item_finalize(GObject *object) {
    GtklabSessionItem *self = GTKLAB_SESSION_ITEM(object);
    self->session.~Session();
    self->folder.~Folder();
    G_OBJECT_CLASS(gtklab_session_item_parent_class)->finalize(object);
}

static void gtklab_session_item_class_init(GtklabSessionItemClass *klass) {
    G_OBJECT_CLASS(klass)->finalize = gtklab_session_item_finalize;
}

static void gtklab_session_item_init(GtklabSessionItem *self) {
    self->is_folder = false;
    new (&self->session) Session();
    new (&self->folder) Folder();
}

GtklabSessionItem *gtklab_session_item_new_session(const Session &s) {
    auto *item = GTKLAB_SESSION_ITEM(g_object_new(GTKLAB_TYPE_SESSION_ITEM, nullptr));
    item->session = s;
    return item;
}

GtklabSessionItem *gtklab_session_item_new_folder(const Folder &f) {
    auto *item = GTKLAB_SESSION_ITEM(g_object_new(GTKLAB_TYPE_SESSION_ITEM, nullptr));
    item->is_folder = true;
    item->folder = f;
    return item;
}

bool gtklab_session_item_is_folder(GtklabSessionItem *item) {
    return item->is_folder;
}

const Session &gtklab_session_item_session(GtklabSessionItem *item) {
    return item->session;
}

const Folder &gtklab_session_item_folder(GtklabSessionItem *item) {
    return item->folder;
}
