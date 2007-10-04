/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
   nautilus-trash-monitor.c: Nautilus trash state watcher.
 
   Copyright (C) 2000, 2001 Eazel, Inc.
  
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
  
   Author: Pavel Cisler <pavel@eazel.com>
*/

#include <config.h>
#include "nautilus-trash-monitor.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory.h"
#include "nautilus-file-attributes.h"
#include <eel/eel-debug.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtksignal.h>

struct NautilusTrashMonitorDetails {
	gboolean empty;
};

enum {
	TRASH_STATE_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static NautilusTrashMonitor *nautilus_trash_monitor = NULL;

static void nautilus_trash_monitor_class_init (NautilusTrashMonitorClass *klass);
static void nautilus_trash_monitor_init       (gpointer                   object,
					       gpointer                   klass);

EEL_CLASS_BOILERPLATE (NautilusTrashMonitor, nautilus_trash_monitor, G_TYPE_OBJECT)

static void
nautilus_trash_monitor_class_init (NautilusTrashMonitorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	signals[TRASH_STATE_CHANGED] = g_signal_new
		("trash_state_changed",
		 G_TYPE_FROM_CLASS (object_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusTrashMonitorClass, trash_state_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__BOOLEAN,
		 G_TYPE_NONE, 1,
		 G_TYPE_BOOLEAN);
}

static void
nautilus_trash_monitor_init (gpointer object, gpointer klass)
{
	NautilusTrashMonitor *trash_monitor;

	trash_monitor = NAUTILUS_TRASH_MONITOR (object);

	trash_monitor->details = g_new0 (NautilusTrashMonitorDetails, 1);
	trash_monitor->details->empty = TRUE;

	/* TODO-gio: How to handle this in gio world */
}



static void
unref_trash_monitor (void)
{
	g_object_unref (nautilus_trash_monitor);
}

NautilusTrashMonitor *
nautilus_trash_monitor_get (void)
{
	if (nautilus_trash_monitor == NULL) {
		/* not running yet, start it up */

		nautilus_trash_monitor = NAUTILUS_TRASH_MONITOR
			(g_object_new (NAUTILUS_TYPE_TRASH_MONITOR, NULL));
		eel_debug_call_at_shutdown (unref_trash_monitor);
	}

	return nautilus_trash_monitor;
}

gboolean
nautilus_trash_monitor_is_empty (void)
{
	NautilusTrashMonitor *monitor;

	monitor = nautilus_trash_monitor_get ();
	return monitor->details->empty;
}

void
nautilus_trash_monitor_add_new_trash_directories (void)
{
	/* We trashed something... */
}
