/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-slot.c: Nautilus window slot
 
   Copyright (C) 2008 Free Software Foundation, Inc.
  
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
  
   Author: Christian Neumair <cneumair@gnome.org>
*/
#include "nautilus-window-slot.h"
#include "nautilus-navigation-window-slot.h"

#include "nautilus-window-private.h"
#include "nautilus-window-manage-views.h"
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-window-slot-info.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>

static void nautilus_window_slot_init       (NautilusWindowSlot *slot);
static void nautilus_window_slot_class_init (NautilusWindowSlotClass *class);
static void nautilus_window_slot_finalize   (GObject *object);

static void nautilus_window_slot_info_iface_init (NautilusWindowSlotInfoIface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusWindowSlot,
			 nautilus_window_slot,
			 G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_WINDOW_SLOT_INFO,
						nautilus_window_slot_info_iface_init))

static void
real_active (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	window = slot->window;

	/* sync window to new slot */
	nautilus_window_sync_status (window);
	nautilus_window_sync_allow_stop (window);
	nautilus_window_sync_title (window);
	nautilus_window_sync_location_widgets (window);

	if (slot->viewed_file != NULL) {
		nautilus_window_load_view_as_menus (window);
		nautilus_window_load_extension_menus (window);
	}
}

static void
nautilus_window_slot_active (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	window = NAUTILUS_WINDOW (slot->window);
	g_assert (slot == window->details->active_slot);

	EEL_CALL_METHOD (NAUTILUS_WINDOW_SLOT_CLASS, slot,
			 active, (slot));
}

static void
real_inactive (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (slot->window);
	g_assert (slot == window->details->active_slot);

	/* multiview-TODO write back locaton widget state */
}

static void
nautilus_window_slot_inactive (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	g_assert (slot == window->details->active_slot);

	EEL_CALL_METHOD (NAUTILUS_WINDOW_SLOT_CLASS, slot,
			 inactive, (slot));
}


static void
nautilus_window_slot_init (NautilusWindowSlot *slot)
{
	GtkWidget *content_box, *eventbox, *extras_vbox;

	content_box = gtk_vbox_new (FALSE, 0);
	slot->content_box = content_box;
	gtk_widget_show (content_box);

	eventbox = gtk_event_box_new ();
	gtk_widget_set_name (eventbox, "nautilus-extra-view-widget");
	gtk_box_pack_start (GTK_BOX (content_box), eventbox, FALSE, FALSE, 0);
	gtk_widget_show (eventbox);
	
	extras_vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (extras_vbox), 6);
	slot->extra_location_widgets = extras_vbox;
	gtk_container_add (GTK_CONTAINER (eventbox), extras_vbox);

	slot->view_box = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (content_box), slot->view_box, TRUE, TRUE, 0);
	gtk_widget_show (slot->view_box);
}

static void
nautilus_window_slot_class_init (NautilusWindowSlotClass *class)
{
	class->active = real_active;
	class->inactive = real_inactive;
	G_OBJECT_CLASS (class)->finalize = nautilus_window_slot_finalize;
}

static int
nautilus_window_slot_get_selection_count (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	if (slot->content_view != NULL) {
		return nautilus_view_get_selection_count (slot->content_view);
	}
	return 0;
}

GFile *
nautilus_window_slot_get_location (NautilusWindowSlot *slot)
{
	g_assert (slot != NULL);
	g_assert (NAUTILUS_IS_WINDOW (slot->window));

	if (slot->location != NULL) {
		return g_object_ref (slot->location);
	}
	return NULL;
}

char *
nautilus_window_slot_get_location_uri (NautilusWindowSlotInfo *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	if (slot->location) {
		return g_file_get_uri (slot->location);
	}
	return NULL;
}

char *
nautilus_window_slot_get_title (NautilusWindowSlot *slot)
{
	char *title;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	title = NULL;
	if (slot->new_content_view != NULL) {
		title = nautilus_view_get_title (slot->new_content_view);
	} else if (slot->content_view != NULL) {
		title = nautilus_view_get_title (slot->content_view);
	}

	if (title == NULL) {
		title = nautilus_compute_title_for_location (slot->location);
        }

	return title;
}

static NautilusWindow *
nautilus_window_slot_get_window (NautilusWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));
	return slot->window;
}

/* nautilus_window_slot_set_title:
 *
 * Sets slot->title, and if it changed
 * synchronizes the actual GtkWindow title which
 * might look a bit different (e.g. with "file browser:" added)
 */
static void
nautilus_window_slot_set_title (NautilusWindowSlot *slot,
				const char *title)
{
	NautilusWindow *window;
	gboolean changed;

	g_assert (NAUTILUS_IS_WINDOW_SLOT (slot));

	window = NAUTILUS_WINDOW (slot->window);

	changed = FALSE;

	if (eel_strcmp (title, slot->title) != 0) {
		changed = TRUE;

		g_free (slot->title);
		slot->title = g_strdup (title);
	}

        if (eel_strlen (slot->title) > 0 && slot->current_location_bookmark &&
            nautilus_bookmark_set_name (slot->current_location_bookmark,
					slot->title)) {
		changed = TRUE;

                /* Name of item in history list changed, tell listeners. */
                nautilus_send_history_list_changed ();
        }

	if (changed && window->details->active_slot == slot) {
		nautilus_window_sync_title (window);
	}
}


/* nautilus_window_slot_update_title:
 * 
 * Re-calculate the slot title.
 * Called when the location or view has changed.
 * @slot: The NautilusWindowSlot in question.
 * 
 */
void
nautilus_window_slot_update_title (NautilusWindowSlot *slot)
{
	char *title;

	title = nautilus_window_slot_get_title (slot);
	nautilus_window_slot_set_title (slot, title);
	g_free (title);
}

/* nautilus_window_slot_update_icon:
 * 
 * Re-calculate the slot icon
 * Called when the location or view or icon set has changed.
 * @slot: The NautilusWindowSlot in question.
 */
void
nautilus_window_slot_update_icon (NautilusWindowSlot *slot)
{
	NautilusWindow *window;
	NautilusIconInfo *info;
	const char *icon_name;
	GdkPixbuf *pixbuf;

	window = slot->window;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	info = EEL_CALL_METHOD_WITH_RETURN_VALUE (NAUTILUS_WINDOW_CLASS, window,
						 get_icon, (window, slot));

	icon_name = NULL;
	if (info) {
		icon_name = nautilus_icon_info_get_used_name (info);
		if (icon_name != NULL) {
			gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
		} else {
			pixbuf = nautilus_icon_info_get_pixbuf_nodefault (info);
			
			if (pixbuf) {
				gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
				g_object_unref (pixbuf);
			} 
		}
		
		g_object_unref (info);
	}
}


static void
remove_all (GtkWidget *widget,
	    gpointer data)
{
	GtkContainer *container;
	container = GTK_CONTAINER (data);

	gtk_container_remove (container, widget);
}

void
nautilus_window_slot_remove_extra_location_widgets (NautilusWindowSlot *slot)
{
	gtk_container_foreach (GTK_CONTAINER (slot->extra_location_widgets),
			       remove_all,
			       slot->extra_location_widgets);
	gtk_widget_hide (slot->extra_location_widgets);
}

void
nautilus_window_slot_add_extra_location_widget (NautilusWindowSlot *slot,
						GtkWidget *widget)
{
	gtk_box_pack_start (GTK_BOX (slot->extra_location_widgets),
			    widget, TRUE, TRUE, 0);
	gtk_widget_show (slot->extra_location_widgets);
}

static void
nautilus_window_slot_finalize (GObject *object)
{
	NautilusWindowSlot *slot = NAUTILUS_WINDOW_SLOT (object);

	nautilus_window_slot_set_viewed_file (slot, NULL);
	/* TODO? why do we unref here? the file is NULL.
 	 * It was already here before the slot move, though */
	nautilus_file_unref (slot->viewed_file);

	if (slot->location) {
		/* TODO? why do we ref here, instead of unreffing?
		 * It was already here before the slot move, though */
		g_object_ref (slot->location);
	}
	eel_g_list_free_deep (slot->pending_selection);

	if (slot->current_location_bookmark != NULL) {
		g_object_unref (slot->current_location_bookmark);
	}
	if (slot->last_location_bookmark != NULL) {
		g_object_unref (slot->last_location_bookmark);
	}

	if (slot->find_mount_cancellable != NULL) {
		g_cancellable_cancel (slot->find_mount_cancellable);
		slot->find_mount_cancellable = NULL;
	}

	g_free (slot->title);
}

static void
nautilus_window_slot_info_iface_init (NautilusWindowSlotInfoIface *iface)
{
	iface->active = nautilus_window_slot_active;
	iface->inactive = nautilus_window_slot_inactive;
	iface->get_window = nautilus_window_slot_get_window;
	iface->get_selection_count = nautilus_window_slot_get_selection_count;
	iface->get_current_location = nautilus_window_slot_get_location_uri;
	iface->set_status = nautilus_window_slot_set_status;
	iface->get_title = nautilus_window_slot_get_title;
	iface->open_location = nautilus_window_slot_open_location_full;
}
