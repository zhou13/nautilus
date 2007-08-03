/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-lockdown-manager.h: singleton that manages lockdown 
    
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
  
   Based on nautilus-desktop-link-monitor.h by Alexander Larsson
*/


#ifndef NAUTILUS_LOCKDOWN_MANAGER_H
#define NAUTILUS_LOCKDOWN_MANAGER_H

#include <gtk/gtkwidget.h>


#define NAUTILUS_TYPE_LOCKDOWN_MANAGER \
	(nautilus_lockdown_manager_get_type ())
#define NAUTILUS_LOCKDOWN_MANAGER(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_LOCKDOWN_MANAGER, NautilusLockdownManager))
#define NAUTILUS_LOCKDOWN_MANAGER_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LOCKDOWN_MANAGER, NautilusLockdownManager))
#define NAUTILUS_IS_LOCKDOWN_MANAGER(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_LOCKDOWN_MANAGER))
#define NAUTILUS_IS_LOCKDOWN_MANAGER_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_LOCKDOWN_MANAGER))

typedef struct NautilusLockdownManagerDetails NautilusLockdownManagerDetails;

typedef struct {
	GObject parent_slot;
	NautilusLockdownManagerDetails *details;
} NautilusLockdownManager;

typedef struct {
	GObjectClass parent_slot;
} NautilusLockdownManagerClass;

GType   nautilus_lockdown_manager_get_type (void);

NautilusLockdownManager *   nautilus_lockdown_manager_get (void);

gboolean nautilus_lockdown_manager_is_uri_allowed (NautilusLockdownManager *manager,
                const char *uri);

#endif /* NAUTILUS_LOCKDOWN_MANAGER_H */
