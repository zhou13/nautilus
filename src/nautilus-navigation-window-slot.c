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

static void
nautilus_navigation_window_slot_active (NautilusWindowSlot *slot)
{
	NautilusWindow *window;

	window = slot->window;

	EEL_CALL_PARENT (NAUTILUS_WINDOW_SLOT_CLASS, active, (slot));

	nautilus_navigation_window_load_extension_toolbar_items (NAUTILUS_NAVIGATION_WINDOW (window));
}

static void
nautilus_navigation_window_slot_init (NautilusNavigationWindowSlot *slot)
{
}

static void
nautilus_navigation_window_slot_class_init (NautilusNavigationWindowSlotClass *class)
{
	NAUTILUS_WINDOW_SLOT_CLASS (class)->active = nautilus_navigation_window_slot_active; 
}
