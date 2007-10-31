#include <config.h>
#include "nautilus-vfs-utils.h"

GQuark
gnome_vfs_error_quark (void)
{
	return g_quark_from_static_string ("gnome-vfs-error-quark");
}
