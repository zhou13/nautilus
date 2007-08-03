/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-lockdown-manager.c: singleton that manages lockdown 
    
   Copyright (C) 2003 Red Hat, Inc.
   Copyright (C) 2007 Sayamindu Dasgupta <sayamindu@gnome.org>
  
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
  
   Based on nautilus-desktop-link-monitor.c by Alexander Larsson
*/

#include <config.h>
#include "nautilus-lockdown-manager.h"
#include "nautilus-global-preferences.h"
#include "nautilus-file-utilities.h"

#include <eel/eel-debug.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkstock.h>
#include <glib/gi18n.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <string.h>

struct NautilusLockdownManagerDetails {
    gboolean restricted_views_enabled;
	GList *allowed_uris;

    gulong mount_id;
    gulong unmount_id;
};


static void nautilus_lockdown_manager_init       (gpointer              object,
						      gpointer              klass);
static void nautilus_lockdown_manager_class_init (gpointer              klass);

EEL_CLASS_BOILERPLATE (NautilusLockdownManager,
		       nautilus_lockdown_manager,
		       G_TYPE_OBJECT)

static NautilusLockdownManager *the_lockdown_manager = NULL;

static void
destroy_lockdown_manager (void)
{
	if (the_lockdown_manager != NULL) {
		g_object_unref (the_lockdown_manager);
	}
}

NautilusLockdownManager *
nautilus_lockdown_manager_get (void)
{
	if (the_lockdown_manager == NULL) {
		g_object_new (NAUTILUS_TYPE_LOCKDOWN_MANAGER, NULL);
		eel_debug_call_at_shutdown (destroy_lockdown_manager);
	}
	return the_lockdown_manager;
}

gboolean 
nautilus_lockdown_manager_is_uri_allowed (NautilusLockdownManager *manager,
        const char *uri)
{
    GList *l;
    gchar *match_uri;

    if (!manager->details->restricted_views_enabled) {
        /* The feature is disabled, so we allow everything */
        return TRUE;
    }

    for (l = manager->details->allowed_uris; l != NULL; l = l->next) {
        match_uri = gnome_vfs_make_uri_from_input(l->data);
        if (g_str_has_prefix(uri, match_uri)) {
            g_free(match_uri);
            return TRUE;
        }
        g_free(match_uri);
    }
    
    return FALSE;
}


static void
volume_mounted_callback (GnomeVFSVolumeMonitor *volume_monitor,
			 GnomeVFSVolume *volume, 
			 NautilusLockdownManager *manager)
{
    if (!gnome_vfs_volume_is_user_visible (volume)) {
        return;
    }

    if (manager->details->restricted_views_enabled && manager->details->allowed_uris != NULL) {
        manager->details->allowed_uris  = g_list_append(manager->details->allowed_uris,
                gnome_vfs_volume_get_activation_uri(volume));
    }
}


static void
volume_unmounted_callback (GnomeVFSVolumeMonitor *volume_monitor,
			   GnomeVFSVolume *volume, 
			   NautilusLockdownManager *manager)
{
    GList *l;

    if (!gnome_vfs_volume_is_user_visible (volume)) {
        return;
    }

    if (manager->details->restricted_views_enabled && manager->details->allowed_uris != NULL) {
        /* Search for the mount point, and remove it */
        for (l = manager->details->allowed_uris; l != NULL; l = l->next) {
            if (!eel_strcmp(l->data, gnome_vfs_volume_get_activation_uri(volume))) {
                manager->details->allowed_uris = g_list_remove(manager->details->allowed_uris,
                        l->data);
                return;
            }
        }
    }
    

}

static void
update_allowed_uri_list(NautilusLockdownManager *manager)
{
    GList *l, *volumes;
    GnomeVFSVolume *volume;
    GnomeVFSVolumeMonitor *volume_monitor;


    manager->details->allowed_uris = 
        eel_preferences_get_string_glist(NAUTILUS_PREFERENCES_LOCKDOWN_ALLOWED_URIS);

    /* We add all the mount points for removable devices since policy for removable 
     * devices is determined by HAL, or more specifically, PolicyKit */
    volume_monitor = gnome_vfs_get_volume_monitor ();
    volumes = gnome_vfs_volume_monitor_get_mounted_volumes (volume_monitor);

    for (l = volumes; l != NULL; l = l->next) {
        volume = l->data;
        if (gnome_vfs_volume_is_user_visible (volume)) {
            manager->details->allowed_uris  = g_list_append(manager->details->allowed_uris,
                    gnome_vfs_volume_get_activation_uri(volume));
        }
        gnome_vfs_volume_unref (volume);
    }
    g_list_free (volumes);


    /* We also add computer:///, etc and  $HOME to the allowed URI list */
    manager->details->allowed_uris  = g_list_prepend(manager->details->allowed_uris,
            "computer:///");
    manager->details->allowed_uris = g_list_prepend(manager->details->allowed_uris,
            "trash:///");
    manager->details->allowed_uris = g_list_prepend(manager->details->allowed_uris,
            "network:///");
    manager->details->allowed_uris = g_list_prepend(manager->details->allowed_uris,
            "burn:///");
    manager->details->allowed_uris = g_list_prepend(manager->details->allowed_uris,
            "search:///");
    manager->details->allowed_uris  = g_list_prepend(manager->details->allowed_uris, 
            nautilus_get_home_directory_uri());

}

static void 
restricted_views_enabled_changed (gpointer callback_data)
{
    NautilusLockdownManager *manager;

    manager = NAUTILUS_LOCKDOWN_MANAGER (callback_data);

    if (eel_preferences_get_boolean(NAUTILUS_PREFERENCES_LOCKDOWN_RESTRICTED_VIEW_ENABLED)) {
        manager->details->restricted_views_enabled = TRUE;
        update_allowed_uri_list(manager);
    }
    else {
        manager->details->restricted_views_enabled = FALSE;
        g_list_free (manager->details->allowed_uris);
        manager->details->allowed_uris = NULL;
    }
}


static void
nautilus_lockdown_manager_init (gpointer object, gpointer klass)
{
	NautilusLockdownManager *manager;
    GnomeVFSVolumeMonitor *volume_monitor;

	manager = NAUTILUS_LOCKDOWN_MANAGER (object);

	the_lockdown_manager = manager;
	
	manager->details = g_new0 (NautilusLockdownManagerDetails, 1);

    manager->details->allowed_uris = NULL;

    if (eel_preferences_get_boolean(NAUTILUS_PREFERENCES_LOCKDOWN_RESTRICTED_VIEW_ENABLED)) {
        manager->details->restricted_views_enabled = TRUE;
        update_allowed_uri_list(manager);
    }
    else {
        manager->details->restricted_views_enabled = FALSE;
    }

    eel_preferences_add_callback(NAUTILUS_PREFERENCES_LOCKDOWN_RESTRICTED_VIEW_ENABLED,
            restricted_views_enabled_changed, manager);

    /* Add handlers for volume mount/unmount events */
    volume_monitor = gnome_vfs_get_volume_monitor ();

    manager->details->mount_id = g_signal_connect_object (volume_monitor, "volume_mounted",
            G_CALLBACK (volume_mounted_callback), manager, 0);
    manager->details->unmount_id = g_signal_connect_object (volume_monitor, "volume_unmounted",
            G_CALLBACK (volume_unmounted_callback), manager, 0);

}

static void
lockdown_manager_finalize (GObject *object)
{
	NautilusLockdownManager *manager;

	manager = NAUTILUS_LOCKDOWN_MANAGER (object);

    eel_preferences_remove_callback (NAUTILUS_PREFERENCES_LOCKDOWN_RESTRICTED_VIEW_ENABLED,
            restricted_views_enabled_changed, manager);
    
    if (manager->details->allowed_uris != NULL) {
        g_list_free (manager->details->allowed_uris);
        manager->details->allowed_uris = NULL;
    }

    if (manager->details->mount_id != 0) {
        g_source_remove (manager->details->mount_id);
    }
    if (manager->details->unmount_id != 0) {
        g_source_remove (manager->details->unmount_id);
    }

	g_free (manager->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_lockdown_manager_class_init (gpointer klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = lockdown_manager_finalize;

}
