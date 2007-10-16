/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-search-directory-file.c: Subclass of NautilusFile to help implement the
   searches
 
   Copyright (C) 2005 Novell, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Anders Carlsson <andersca@imendio.com>
*/

#include <config.h>
#include "nautilus-search-directory-file.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include "nautilus-search-directory.h"
#include <gtk/gtksignal.h>
#include <glib/gi18n.h>
#include <string.h>

static void nautilus_search_directory_file_init       (gpointer   object,
						       gpointer   klass);
static void nautilus_search_directory_file_class_init (gpointer   klass);

EEL_CLASS_BOILERPLATE (NautilusSearchDirectoryFile,
		       nautilus_search_directory_file,
		       NAUTILUS_TYPE_FILE);

struct NautilusSearchDirectoryFileDetails {
	NautilusSearchDirectory *search_directory;
};

static void
search_directory_file_monitor_add (NautilusFile *file,
				   gconstpointer client,
				   NautilusFileAttributes attributes)
{
	/* No need for monitoring, we always emit changed when files
	   are added/removed, and no other metadata changes */
}

static void
search_directory_file_monitor_remove (NautilusFile *file,
				      gconstpointer client)
{
	/* Do nothing here, we don't have any monitors */
}

static void
search_directory_file_call_when_ready (NautilusFile *file,
				       NautilusFileAttributes file_attributes,
				       NautilusFileCallback callback,
				       gpointer callback_data)

{
	/* All data for directory-as-file is always uptodate */
	(* callback) (file, callback_data);
}
 
static void
search_directory_file_cancel_call_when_ready (NautilusFile *file,
					       NautilusFileCallback callback,
					       gpointer callback_data)
{
	/* Do nothing here, we don't have any pending calls */
}


static gboolean
search_directory_file_check_if_ready (NautilusFile *file,
				      NautilusFileAttributes attributes)
{
	return TRUE;
}

static gboolean
search_directory_file_get_item_count (NautilusFile *file, 
				      guint *count,
				      gboolean *count_unreadable)
{
	NautilusSearchDirectory *search_dir;
	GList *file_list;

	if (count) {
		search_dir = NAUTILUS_SEARCH_DIRECTORY (file->details->directory);

		file_list = nautilus_directory_get_file_list (file->details->directory);

		*count = g_list_length (file_list);

		nautilus_file_list_free (file_list);
	}

	return TRUE;
}

static NautilusRequestStatus
search_directory_file_get_deep_counts (NautilusFile *file,
				       guint *directory_count,
				       guint *file_count,
				       guint *unreadable_directory_count,
				       goffset *total_size)
{
	NautilusSearchDirectory *search_dir;
	NautilusFile *dir_file;
	GList *file_list, *l;
	guint dirs, files;
	GFileType type;

	search_dir = NAUTILUS_SEARCH_DIRECTORY (file->details->directory);
	
	file_list = nautilus_directory_get_file_list (file->details->directory);

	dirs = files = 0;
	for (l = file_list; l != NULL; l = l->next) {
		dir_file = NAUTILUS_FILE (l->data);
		type = nautilus_file_get_file_type (dir_file);
		if (type == G_FILE_TYPE_DIRECTORY) {
			dirs++;
		} else {
			files++;
		}
	}

	if (directory_count != NULL) {
		*directory_count = dirs;
	}
	if (file_count != NULL) {
		*file_count = files;
	}
	if (unreadable_directory_count != NULL) {
		*unreadable_directory_count = 0;
	}
	if (total_size != NULL) {
		/* FIXME: Maybe we want to calculate this? */
		*total_size = 0;
	}
	
	nautilus_file_list_free (file_list);
	
	return NAUTILUS_REQUEST_DONE;
}

static char *
search_directory_file_get_where_string (NautilusFile *file)
{
	return g_strdup (_("Search"));
}
    
static void
nautilus_search_directory_file_init (gpointer object, gpointer klass)
{
	NautilusSearchDirectoryFile *search_file;
	NautilusFile *file;

	search_file = NAUTILUS_SEARCH_DIRECTORY_FILE (object);
	file = NAUTILUS_FILE(object);

	file->details->got_file_info = TRUE;
	file->details->mime_type = g_strdup ("x-directory/normal");
	file->details->type = G_FILE_TYPE_DIRECTORY;
	file->details->size = 0;
	file->details->permissions =
		GNOME_VFS_PERM_OTHER_WRITE |
		GNOME_VFS_PERM_GROUP_WRITE |
		GNOME_VFS_PERM_USER_READ |
		GNOME_VFS_PERM_OTHER_READ |
		GNOME_VFS_PERM_GROUP_READ;
	
	file->details->file_info_is_up_to_date = TRUE;

	file->details->display_name = g_strdup (_("Search"));
	file->details->custom_icon = NULL;
	file->details->activation_location = NULL;
	file->details->got_link_info = TRUE;
	file->details->link_info_is_up_to_date = TRUE;

	file->details->directory_count = 0;
	file->details->got_directory_count = TRUE;
	file->details->directory_count_is_up_to_date = TRUE;
}

static void
nautilus_search_directory_file_class_init (gpointer klass)
{
	GObjectClass *object_class;
	NautilusFileClass *file_class;

	object_class = G_OBJECT_CLASS (klass);
	file_class = NAUTILUS_FILE_CLASS (klass);

	file_class->default_file_type = G_FILE_TYPE_DIRECTORY;
	
	file_class->monitor_add = search_directory_file_monitor_add;
	file_class->monitor_remove = search_directory_file_monitor_remove;
	file_class->call_when_ready = search_directory_file_call_when_ready;
	file_class->cancel_call_when_ready = search_directory_file_cancel_call_when_ready;
	file_class->check_if_ready = search_directory_file_check_if_ready;
	file_class->get_item_count = search_directory_file_get_item_count;
	file_class->get_deep_counts = search_directory_file_get_deep_counts;
	file_class->get_where_string = search_directory_file_get_where_string;
}
