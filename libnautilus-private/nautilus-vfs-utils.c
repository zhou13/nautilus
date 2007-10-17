#include <config.h>
#include "nautilus-vfs-utils.h"

GQuark
gnome_vfs_error_quark (void)
{
	return g_quark_from_static_string ("gnome-vfs-error-quark");
}

GError *
gnome_vfs_result_to_error (GnomeVFSResult result)
{
	if (result == GNOME_VFS_OK) {
		return NULL;
	}

	return g_error_new_literal (GNOME_VFS_ERROR, result,
				    gnome_vfs_result_to_string (result));
}
