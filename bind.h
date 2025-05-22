#ifndef __GTKSHELL_BIND__
#define __GTKSHELL_BIND__

#include <glibmm.h>

// This function calls fun when property "property" of object "obj" changes.
// Depending on flags, it can also call the function when creating the binding
void bind_property_changed(Glib::Object *obj, const Glib::ustring &property, sigc::slot<void()> fun, bool sync_create = true);

#endif // __GTKSHELL_BIND__
