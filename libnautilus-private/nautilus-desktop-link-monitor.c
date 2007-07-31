/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-desktop-link-monitor.c: singleton thatn manages the links
    
   Copyright (C) 2003 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nautilus-desktop-link-monitor.h"
#include "nautilus-desktop-link.h"
#include "nautilus-desktop-icon-file.h"
#include "nautilus-directory.h"
#include "nautilus-desktop-directory.h"
#include "nautilus-global-preferences.h"

#include <eel/eel-debug.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <libgnome/gnome-desktop-item.h>
#include <libnautilus-private/nautilus-trash-monitor.h>
#include <string.h>

struct NautilusDesktopLinkMonitorDetails {
	NautilusDirectory *desktop_dir;
    NautilusDirectory *global_dir;
	
	NautilusDesktopLink *home_link;
	NautilusDesktopLink *computer_link;
	NautilusDesktopLink *trash_link;
	NautilusDesktopLink *network_link;

	gulong mount_id;
	gulong unmount_id;
	
	GList *volume_links;
    GList *global_links;

    void *global_dir_client;
};


static void nautilus_desktop_link_monitor_init       (gpointer              object,
						      gpointer              klass);
static void nautilus_desktop_link_monitor_class_init (gpointer              klass);

EEL_CLASS_BOILERPLATE (NautilusDesktopLinkMonitor,
		       nautilus_desktop_link_monitor,
		       G_TYPE_OBJECT)

static NautilusDesktopLinkMonitor *the_link_monitor = NULL;

static void
destroy_desktop_link_monitor (void)
{
	if (the_link_monitor != NULL) {
		g_object_unref (the_link_monitor);
	}
}

NautilusDesktopLinkMonitor *
nautilus_desktop_link_monitor_get (void)
{
	if (the_link_monitor == NULL) {
		g_object_new (NAUTILUS_TYPE_DESKTOP_LINK_MONITOR, NULL);
		eel_debug_call_at_shutdown (destroy_desktop_link_monitor);
	}
	return the_link_monitor;
}

static gboolean
eject_for_type (GnomeVFSDeviceType type)
{
	switch (type) {
	case GNOME_VFS_DEVICE_TYPE_CDROM:
	case GNOME_VFS_DEVICE_TYPE_ZIP:
	case GNOME_VFS_DEVICE_TYPE_JAZ:
		return TRUE;
	default:
		return FALSE;
	}
}

static void
volume_delete_dialog (GtkWidget *parent_view,
                      NautilusDesktopLink *link)
{
	GnomeVFSVolume *volume;
	char *dialog_str;
	char *display_name;

	volume = nautilus_desktop_link_get_volume (link);

	if (volume != NULL) {
		display_name = nautilus_desktop_link_get_display_name (link);
		dialog_str = g_strdup_printf (_("You cannot move the volume \"%s\" to the trash."),
					      display_name);
		g_free (display_name);

		if (eject_for_type (gnome_vfs_volume_get_device_type (volume))) {
			eel_run_simple_dialog
				(parent_view, 
				 FALSE,
				 GTK_MESSAGE_ERROR,
				 dialog_str,
				 _("If you want to eject the volume, please use \"Eject\" in the "
				   "popup menu of the volume."),
				 GTK_STOCK_OK, NULL);
		} else {
			eel_run_simple_dialog
				(parent_view, 
				 FALSE,
				 GTK_MESSAGE_ERROR,
				 dialog_str,
				 _("If you want to unmount the volume, please use \"Unmount Volume\" in the "
				   "popup menu of the volume."),
				 GTK_STOCK_OK, NULL);
		}

		gnome_vfs_volume_unref (volume);
		g_free (dialog_str);
	}
}

void
nautilus_desktop_link_monitor_delete_link (NautilusDesktopLinkMonitor *monitor,
					   NautilusDesktopLink *link,
					   GtkWidget *parent_view)
{
	switch (nautilus_desktop_link_get_link_type (link)) {
	case NAUTILUS_DESKTOP_LINK_HOME:
	case NAUTILUS_DESKTOP_LINK_COMPUTER:
	case NAUTILUS_DESKTOP_LINK_TRASH:
	case NAUTILUS_DESKTOP_LINK_NETWORK:
    case NAUTILUS_DESKTOP_LINK_GLOBAL:
		/* just ignore. We don't allow you to delete these */
		break;
	default:
		volume_delete_dialog (parent_view, link);
		break;
	}
}

static gboolean
volume_file_name_used (NautilusDesktopLinkMonitor *monitor,
		       const char *name)
{
	GList *l;
	char *other_name;
	gboolean same;

	for (l = monitor->details->volume_links; l != NULL; l = l->next) {
		other_name = nautilus_desktop_link_get_file_name (l->data);
		same = strcmp (name, other_name) == 0;
		g_free (other_name);

		if (same) {
			return TRUE;
		}
	}

	return FALSE;
}

char *
nautilus_desktop_link_monitor_make_filename_unique (NautilusDesktopLinkMonitor *monitor,
						    const char *filename)
{
	char *unique_name;
	int i;
	
	i = 2;
	unique_name = g_strdup (filename);
	while (volume_file_name_used (monitor, unique_name)) {
		g_free (unique_name);
		unique_name = g_strdup_printf ("%s.%d", filename, i++);
	}
	return unique_name;
}

static void
create_volume_link (NautilusDesktopLinkMonitor *monitor,
		    GnomeVFSVolume *volume)
{
	NautilusDesktopLink *link;

	link = NULL;

	if (!gnome_vfs_volume_is_user_visible (volume)) {
		return;
	}

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_VOLUMES_VISIBLE)) {
		link = nautilus_desktop_link_new_from_volume (volume);
		monitor->details->volume_links = g_list_prepend (monitor->details->volume_links, link);
	}
}



static void
volume_mounted_callback (GnomeVFSVolumeMonitor *volume_monitor,
			 GnomeVFSVolume *volume, 
			 NautilusDesktopLinkMonitor *monitor)
{
	create_volume_link (monitor, volume);
}


static void
volume_unmounted_callback (GnomeVFSVolumeMonitor *volume_monitor,
			   GnomeVFSVolume *volume, 
			   NautilusDesktopLinkMonitor *monitor)
{
	GList *l;
	NautilusDesktopLink *link;
	GnomeVFSVolume *other_volume;

	link = NULL;
	for (l = monitor->details->volume_links; l != NULL; l = l->next) {
		other_volume = nautilus_desktop_link_get_volume (l->data);
		if (volume == other_volume) {
			gnome_vfs_volume_unref (other_volume);
			link = l->data;
			break;
		}
		gnome_vfs_volume_unref (other_volume);
	}

	if (link) {
		monitor->details->volume_links = g_list_remove (monitor->details->volume_links, link);
		g_object_unref (link);
	}
}

static void
update_link_visibility (NautilusDesktopLinkMonitor *monitor,
			NautilusDesktopLink       **link_ref,
			NautilusDesktopLinkType     link_type,
			const char                 *preference_key)
{
	if (eel_preferences_get_boolean (preference_key)) {
		if (*link_ref == NULL) {
			*link_ref = nautilus_desktop_link_new (link_type);
		}
	} else {
		if (*link_ref != NULL) {
			g_object_unref (*link_ref);
			*link_ref = NULL;
		}
	}
}

static void
desktop_home_visible_changed (gpointer callback_data)
{
	NautilusDesktopLinkMonitor *monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

	update_link_visibility (NAUTILUS_DESKTOP_LINK_MONITOR (monitor),
				&monitor->details->home_link,
				NAUTILUS_DESKTOP_LINK_HOME,
				NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE);
}

static void
desktop_computer_visible_changed (gpointer callback_data)
{
	NautilusDesktopLinkMonitor *monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

	update_link_visibility (NAUTILUS_DESKTOP_LINK_MONITOR (callback_data),
				&monitor->details->computer_link,
				NAUTILUS_DESKTOP_LINK_COMPUTER,
				NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_VISIBLE);
}

static void
desktop_trash_visible_changed (gpointer callback_data)
{
	NautilusDesktopLinkMonitor *monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

	update_link_visibility (NAUTILUS_DESKTOP_LINK_MONITOR (callback_data),
				&monitor->details->trash_link,
				NAUTILUS_DESKTOP_LINK_TRASH,
				NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE);
}

static void
desktop_network_visible_changed (gpointer callback_data)
{
	NautilusDesktopLinkMonitor *monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

	update_link_visibility (NAUTILUS_DESKTOP_LINK_MONITOR (callback_data),
				&monitor->details->network_link,
				NAUTILUS_DESKTOP_LINK_NETWORK,
				NAUTILUS_PREFERENCES_DESKTOP_NETWORK_VISIBLE);
}

static void
desktop_volumes_visible_changed (gpointer callback_data)
{
	GnomeVFSVolumeMonitor *volume_monitor;
	NautilusDesktopLinkMonitor *monitor;
	GList *l, *volumes;
	
	volume_monitor = gnome_vfs_get_volume_monitor ();
	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

	if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_VOLUMES_VISIBLE)) {
		if (monitor->details->volume_links == NULL) {
			volumes = gnome_vfs_volume_monitor_get_mounted_volumes (volume_monitor);
			for (l = volumes; l != NULL; l = l->next) {
				create_volume_link (monitor, l->data);
				gnome_vfs_volume_unref (l->data);
			}
			g_list_free (volumes);
		}
	} else {
		g_list_foreach (monitor->details->volume_links, (GFunc)g_object_unref, NULL);
		g_list_free (monitor->details->volume_links);
		monitor->details->volume_links = NULL;
	}
}


static gint
compare_link_file(NautilusDesktopLink *link, NautilusFile *file) {
    NautilusFile *other_file;

    other_file = nautilus_desktop_link_get_file(link);

    /* FIXME: Is this the right way to compare two NautilusFiles ? */
    return g_utf8_collate(nautilus_file_get_uri(other_file), 
            (nautilus_file_get_uri(file)));

    nautilus_file_unref(other_file);
}

static void
global_dir_files_added (NautilusDirectory *directory, GList *changed_files, gpointer callback_data) 
{
    NautilusDesktopLinkMonitor *monitor;
    GList *l;

    monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

    for (l = changed_files; l != NULL; l = l->next) {
	    /* We filter out anything other than .desktop files */
        if (g_str_has_suffix(nautilus_file_get_uri(NAUTILUS_FILE(l->data)), ".desktop")) { 
            monitor->details->global_links = g_list_prepend (monitor->details->global_links, 
                    nautilus_desktop_link_new_from_file(NAUTILUS_FILE(l->data)));
        }
    }
}

static void 
global_dir_files_changed (NautilusDirectory *directory, GList *changed_files, gpointer callback_data) {
    NautilusDesktopLinkMonitor *monitor;
    GList *l, *deleted_link;
    deleted_link = NULL;

    monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

    for (l = changed_files; l != NULL; l = l->next) {
        if (nautilus_file_is_gone (l->data)) {/* We need to run this only if a file gets deleted */
            deleted_link = 
                g_list_find_custom(monitor->details->global_links, NAUTILUS_FILE(l->data), 
                        (GCompareFunc )compare_link_file);

            if (deleted_link != NULL) { /* Found a link, unref it */
                
                NAUTILUS_DESKTOP_LINK_MONITOR(callback_data)->details->global_links =
                    g_list_remove_link(monitor->details->global_links, deleted_link);                
                g_object_unref(deleted_link->data);
                g_list_free(deleted_link);
                deleted_link = NULL;
            }
        }   
    }
}

static void
desktop_global_items_visible_changed (gpointer callback_data)
{
    NautilusDesktopLinkMonitor *monitor;
    gchar *global_dir_uri;

    monitor = NAUTILUS_DESKTOP_LINK_MONITOR (callback_data);

    if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_GLOBAL_ITEMS_VISIBLE)) {     
	    if (monitor->details->global_links == NULL) {
            global_dir_uri = eel_preferences_get(NAUTILUS_PREFERENCES_DESKTOP_GLOBAL_ITEMS_DIR);

            if (global_dir_uri == NULL || !g_utf8_collate(global_dir_uri, "")) /* Just a sanity check */
        	    return;

        	monitor->details->global_dir = nautilus_directory_get(global_dir_uri);

        	monitor->details->global_dir_client = g_new0 (int, 1);

            nautilus_directory_file_monitor_add (monitor->details->global_dir,
                monitor->details->global_dir_client, FALSE, FALSE,
                NAUTILUS_FILE_ATTRIBUTE_METADATA,
                global_dir_files_added, monitor);

    	    g_signal_connect (monitor->details->global_dir, "files_added", 
                    G_CALLBACK (global_dir_files_added), monitor);
    	    g_signal_connect (monitor->details->global_dir, "files_changed", 
                    G_CALLBACK (global_dir_files_changed), monitor);

        }
    }	
    else {
        if (monitor->details->global_links != NULL) {
            g_list_foreach (monitor->details->global_links, (GFunc)g_object_unref, NULL);
    	    g_list_free (monitor->details->global_links);
	        monitor->details->global_links = NULL;

    	    nautilus_directory_file_monitor_remove (monitor->details->global_dir, 
                    monitor->details->global_dir_client);
	        nautilus_directory_unref (monitor->details->global_dir);
    	    monitor->details->global_dir = NULL;
        }
    }

}

static void
create_link_and_add_preference (NautilusDesktopLink   **link_ref,
				NautilusDesktopLinkType link_type,
				const char             *preference_key,
				EelPreferencesCallback  callback,
				gpointer                callback_data)
{
	if (eel_preferences_get_boolean (preference_key)) {
		*link_ref = nautilus_desktop_link_new (link_type);
	}

	eel_preferences_add_callback (preference_key, callback, callback_data);
}
	     
static void
nautilus_desktop_link_monitor_init (gpointer object, gpointer klass)
{
	NautilusDesktopLinkMonitor *monitor;
	GList *l, *volumes;
	GnomeVFSVolume *volume;
	GnomeVFSVolumeMonitor *volume_monitor;
    char *global_dir_uri;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (object);

	the_link_monitor = monitor;
	
	monitor->details = g_new0 (NautilusDesktopLinkMonitorDetails, 1);

	/* We keep around a ref to the desktop dir */
	monitor->details->desktop_dir = nautilus_directory_get (EEL_DESKTOP_URI);

	/* Default links */

	create_link_and_add_preference (&monitor->details->home_link,
					NAUTILUS_DESKTOP_LINK_HOME,
					NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE,
					desktop_home_visible_changed,
					monitor);

	create_link_and_add_preference (&monitor->details->computer_link,
					NAUTILUS_DESKTOP_LINK_COMPUTER,
					NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_VISIBLE,
					desktop_computer_visible_changed,
					monitor);
	
	create_link_and_add_preference (&monitor->details->trash_link,
					NAUTILUS_DESKTOP_LINK_TRASH,
					NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE,
					desktop_trash_visible_changed,
					monitor);

	create_link_and_add_preference (&monitor->details->network_link,
					NAUTILUS_DESKTOP_LINK_NETWORK,
					NAUTILUS_PREFERENCES_DESKTOP_NETWORK_VISIBLE,
					desktop_network_visible_changed,
					monitor);

	/* Volume links */

	volume_monitor = gnome_vfs_get_volume_monitor ();
	
	volumes = gnome_vfs_volume_monitor_get_mounted_volumes (volume_monitor);
	for (l = volumes; l != NULL; l = l->next) {
		volume = l->data;
		create_volume_link (monitor, volume);
		gnome_vfs_volume_unref (volume);
	}
	g_list_free (volumes);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_VOLUMES_VISIBLE,
				      desktop_volumes_visible_changed,
				      monitor);

	monitor->details->mount_id = g_signal_connect_object (volume_monitor, "volume_mounted",
							      G_CALLBACK (volume_mounted_callback), monitor, 0);
	monitor->details->unmount_id = g_signal_connect_object (volume_monitor, "volume_unmounted",
								G_CALLBACK (volume_unmounted_callback), monitor, 0);

    /* Global Items */

    global_dir_uri = eel_preferences_get(NAUTILUS_PREFERENCES_DESKTOP_GLOBAL_ITEMS_DIR);

    if (eel_preferences_get_boolean (NAUTILUS_PREFERENCES_DESKTOP_GLOBAL_ITEMS_VISIBLE) 
            && global_dir_uri != NULL && g_utf8_collate(global_dir_uri, "")) {
        
        monitor->details->global_dir = nautilus_directory_get(global_dir_uri);

        monitor->details->global_dir_client = g_new0 (int, 1);

        nautilus_directory_file_monitor_add (monitor->details->global_dir,
            monitor->details->global_dir_client, FALSE, FALSE,
            NAUTILUS_FILE_ATTRIBUTE_METADATA,
            global_dir_files_added, monitor);

        g_signal_connect (monitor->details->global_dir, 
                "files_added", G_CALLBACK (global_dir_files_added), monitor);
        g_signal_connect (monitor->details->global_dir, 
                "files_changed", G_CALLBACK (global_dir_files_changed), monitor);

    }
    

    eel_preferences_add_callback (NAUTILUS_PREFERENCES_DESKTOP_GLOBAL_ITEMS_VISIBLE,
                                  desktop_global_items_visible_changed,
                                  monitor);
    
}

static void
remove_link_and_preference (NautilusDesktopLink   **link_ref,
			    const char             *preference_key,
			    EelPreferencesCallback  callback,
			    gpointer                callback_data)
{
	if (*link_ref != NULL) {
		g_object_unref (*link_ref);
		*link_ref = NULL;
	}

	eel_preferences_remove_callback (preference_key, callback, callback_data);
}

static void
desktop_link_monitor_finalize (GObject *object)
{
	NautilusDesktopLinkMonitor *monitor;

	monitor = NAUTILUS_DESKTOP_LINK_MONITOR (object);

	/* Default links */

	remove_link_and_preference (&monitor->details->home_link,
				    NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE,
				    desktop_home_visible_changed,
				    monitor);

	remove_link_and_preference (&monitor->details->computer_link,
				    NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_VISIBLE,
				    desktop_computer_visible_changed,
				    monitor);

	remove_link_and_preference (&monitor->details->trash_link,
				    NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE,
				    desktop_trash_visible_changed,
				    monitor);

	remove_link_and_preference (&monitor->details->network_link,
				    NAUTILUS_PREFERENCES_DESKTOP_NETWORK_VISIBLE,
				    desktop_network_visible_changed,
				    monitor);

	/* Volumes */

	g_list_foreach (monitor->details->volume_links, (GFunc)g_object_unref, NULL);
	g_list_free (monitor->details->volume_links);
	monitor->details->volume_links = NULL;
		
	nautilus_directory_unref (monitor->details->desktop_dir);
	monitor->details->desktop_dir = NULL;

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_DESKTOP_VOLUMES_VISIBLE,
					 desktop_volumes_visible_changed,
					 monitor);

	if (monitor->details->mount_id != 0) {
		g_source_remove (monitor->details->mount_id);
	}
	if (monitor->details->unmount_id != 0) {
		g_source_remove (monitor->details->unmount_id);
	}

    /* Global Items */

    g_list_foreach (monitor->details->global_links, (GFunc)g_object_unref, NULL);
    g_list_free (monitor->details->global_links);
    monitor->details->global_links = NULL;

    nautilus_directory_file_monitor_remove (monitor->details->global_dir, monitor->details->global_dir_client);
    nautilus_directory_unref (monitor->details->global_dir);
    monitor->details->global_dir = NULL;
	
	g_free (monitor->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_desktop_link_monitor_class_init (gpointer klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = desktop_link_monitor_finalize;

}
