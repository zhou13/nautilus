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
	switch (file_type) {
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

GnomeVFSFileType
gnome_vfs_file_type_from_g_file_type (GFileType file_type)
{
	switch (file_type) {
	default:
	case G_FILE_TYPE_UNKNOWN:
	case G_FILE_TYPE_SHORTCUT:
	case G_FILE_TYPE_SPECIAL:
	case G_FILE_TYPE_MOUNTABLE:
		return GNOME_VFS_FILE_TYPE_UNKNOWN;
	case G_FILE_TYPE_REGULAR:
		return GNOME_VFS_FILE_TYPE_REGULAR;
	case G_FILE_TYPE_DIRECTORY:
		return GNOME_VFS_FILE_TYPE_DIRECTORY;
	case G_FILE_TYPE_SYMBOLIC_LINK:
		return GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK;
	}
}


GFileInfo *
gnome_vfs_file_info_to_gio (GnomeVFSFileInfo *vfs_info)
{
	GFileInfo *info;
	
	info = g_file_info_new ();
	
	if (vfs_info->name) {
		g_file_info_set_name (info, vfs_info->name);
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_TYPE) {
		g_file_info_set_file_type (info,
					   gnome_vfs_file_type_to_g_file_type (vfs_info->type));
	}
		
	
	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_PERMISSIONS) {
		g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE,
						  vfs_info->permissions & 07777);
	}
	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_FLAGS) {
		if (vfs_info->flags & GNOME_VFS_FILE_FLAGS_SYMLINK) {
			g_file_info_set_is_symlink (info, TRUE);
		}
			
	}
	
	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_DEVICE) {
		g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_DEVICE,
						  vfs_info->device);
	}
	
	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_INODE) {
		g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_UNIX_INODE,
						  vfs_info->inode);
	}
	
	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_LINK_COUNT) {
		g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_NLINK,
						  vfs_info->link_count);
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE) {
		g_file_info_set_size (info, vfs_info->size);
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_BLOCK_COUNT) {
		/* Ignore */
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_IO_BLOCK_SIZE) {
		/* Ignore */
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_ATIME) {
		g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_ACCESS,
						  vfs_info->atime);
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME) {
		g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED,
						  vfs_info->mtime);
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_CTIME) {
		g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_CHANGED,
						  vfs_info->ctime);
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME &&
	    vfs_info->symlink_name != NULL) {
		g_file_info_set_symlink_target (info, vfs_info->symlink_name);
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE &&
	    vfs_info->mime_type != NULL) {
		g_file_info_set_content_type (info, vfs_info->mime_type);
		
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_ACCESS) {
		g_file_info_set_attribute_boolean (info,
						   G_FILE_ATTRIBUTE_ACCESS_CAN_READ,
						   (vfs_info->permissions & GNOME_VFS_PERM_ACCESS_READABLE) != 0);
		g_file_info_set_attribute_boolean (info,
						   G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
						   (vfs_info->permissions & GNOME_VFS_PERM_ACCESS_WRITABLE) != 0);
		g_file_info_set_attribute_boolean (info,
						   G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE,
						   (vfs_info->permissions & GNOME_VFS_PERM_ACCESS_EXECUTABLE) != 0);
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_IDS) {
		g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID,
						  vfs_info->uid);
		g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID,
						  vfs_info->gid);
	}

	if (vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SELINUX_CONTEXT) {
		g_file_info_set_attribute_string (info, "selinux:context",
						  vfs_info->selinux_context);
	}

	return info;
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
