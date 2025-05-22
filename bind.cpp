#include "bind.h"

void bind_property_changed(Glib::Object *obj, const Glib::ustring &property, sigc::slot<void()> fun, bool sync_create)
{
    if (sync_create)
        fun();
    obj->connect_property_changed(property, fun);
}

