#include <config.h>
#include "nautilus-vfs-utils.h"

GQuark
gnome_vfs_error_quark (void)
{
  return g_quark_from_static_string ("gnome-vfs-error-quark");
}

GFileType
gnome_vfs_file_type_to_g_file_type (GnomeVFSFileType file_type)
{
  switch (file_type)
    {
    case GNOME_VFS_FILE_TYPE_REGULAR:
      return G_FILE_TYPE_REGULAR;
    case GNOME_VFS_FILE_TYPE_DIRECTORY:
      return G_FILE_TYPE_DIRECTORY;
    case GNOME_VFS_FILE_TYPE_FIFO:
    case GNOME_VFS_FILE_TYPE_SOCKET:
    case GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE:
    case GNOME_VFS_FILE_TYPE_BLOCK_DEVICE:
      return G_FILE_TYPE_SPECIAL;
    case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
      return G_FILE_TYPE_SYMBOLIC_LINK;
    default:
    case GNOME_VFS_FILE_TYPE_UNKNOWN:
      return G_FILE_TYPE_UNKNOWN;
    }
}
  
