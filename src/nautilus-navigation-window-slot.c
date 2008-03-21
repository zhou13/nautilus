/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-navigation-window-slot.c: Nautilus navigation window slot
 
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
#include <libnautilus-private/nautilus-window-slot-info.h>
#include <eel/eel-gtk-macros.h>

static void nautilus_navigation_window_slot_init       (NautilusNavigationWindowSlot *slot);
static void nautilus_navigation_window_slot_class_init (NautilusNavigationWindowSlotClass *class);

G_DEFINE_TYPE (NautilusNavigationWindowSlot, nautilus_navigation_window_slot, NAUTILUS_TYPE_WINDOW_SLOT)
#define parent_class nautilus_navigation_window_slot_parent_class

void
nautilus_navigation_window_slot_clear_forward_list (NautilusNavigationWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW_SLOT (slot));

	eel_g_object_list_free (slot->forward_list);
	slot->forward_list = NULL;
}

void
nautilus_navigation_window_slot_clear_back_list (NautilusNavigationWindowSlot *slot)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW_SLOT (slot));

	eel_g_object_list_free (slot->back_list);
	slot->back_list = NULL;
}

static void
nautilus_navigation_window_slot_active (NautilusWindowSlot *slot)
{
	NautilusNavigationWindow *window;
	NautilusNavigationWindowSlot *navigation_slot;
	int page_num;

	navigation_slot = NAUTILUS_NAVIGATION_WINDOW_SLOT (slot);
	window = NAUTILUS_NAVIGATION_WINDOW (slot->window);

	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (window->notebook),
					  slot->content_box);
	g_assert (page_num >= 0);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (window->notebook), page_num);

	EEL_CALL_PARENT (NAUTILUS_WINDOW_SLOT_CLASS, active, (slot));

	nautilus_navigation_window_load_extension_toolbar_items (window);
}
 
static NautilusWindowSlot *
nautilus_navigation_window_slot_get_close_successor (NautilusWindowSlot *slot)
{
	NautilusWindowSlot *successor;
	NautilusNavigationWindow *window;
	GtkNotebook *notebook;
	GtkWidget *widget;
	int page_num, n_pages;

	window = NAUTILUS_NAVIGATION_WINDOW (slot->window);
	notebook = GTK_NOTEBOOK (window->notebook);

	n_pages = gtk_notebook_get_n_pages (notebook);

	page_num = gtk_notebook_page_num (notebook, slot->content_box);
	g_assert (page_num >= 0);

	if (page_num == n_pages - 1) {
		/* use previous page */
		page_num--;
	} else {
		/* use next page */
		page_num++;
	}

	successor = NULL;

	widget = gtk_notebook_get_nth_page (notebook, page_num);
	if (widget != NULL) {
		successor = nautilus_window_get_slot_for_content_box (slot->window, widget);
		if (successor == slot) {
			successor = NULL;
		}
	}

	return successor;
}

static void
nautilus_navigation_window_slot_finalize (GObject *object)
{
	NautilusNavigationWindowSlot *slot;

	slot = NAUTILUS_NAVIGATION_WINDOW_SLOT (object);

	nautilus_navigation_window_slot_clear_forward_list (slot);
	nautilus_navigation_window_slot_clear_back_list (slot);
}

static void
nautilus_navigation_window_slot_init (NautilusNavigationWindowSlot *slot)
{
}

static void
nautilus_navigation_window_slot_class_init (NautilusNavigationWindowSlotClass *class)
{
	NAUTILUS_WINDOW_SLOT_CLASS (class)->active = nautilus_navigation_window_slot_active; 
	NAUTILUS_WINDOW_SLOT_CLASS (class)->get_close_successor = nautilus_navigation_window_slot_get_close_successor;

	G_OBJECT_CLASS (class)->finalize = nautilus_navigation_window_slot_finalize;
}
