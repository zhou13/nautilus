#ifndef __NAUTILUS_VFS_UTILS_H__
#define __NAUTILUS_VFS_UTILS_H__

#include <glib/gerror.h>
#include <gio/gfileinfo.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

/* This file has helper tools in the conversion from gnome-vfs to gio */

/* To store a GnomeVFSResult in a GError */
GQuark          gnome_vfs_error_quark      (void);

#define GNOME_VFS_ERROR gnome_vfs_error_quark()

GFileType gnome_vfs_file_type_to_g_file_type (GnomeVFSFileType file_type);


#endif /* __NAUTILUS_VFS_UTILS_H__ */
