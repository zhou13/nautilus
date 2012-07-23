/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef NAUTILUS_ICON_VIEW_H
#define NAUTILUS_ICON_VIEW_H

#include "nautilus-view.h"

#define NAUTILUS_TYPE_ICON_VIEW nautilus_icon_view_get_type()
#define NAUTILUS_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ICON_VIEW, NautilusIconView))
#define NAUTILUS_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICON_VIEW, NautilusIconViewClass))
#define NAUTILUS_IS_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ICON_VIEW))
#define NAUTILUS_IS_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ICON_VIEW))
#define NAUTILUS_ICON_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_ICON_VIEW, NautilusIconViewClass))

#define NAUTILUS_ICON_VIEW_ID "OAFIID:Nautilus_File_Manager_Icon_View"

typedef struct NautilusIconViewDetails NautilusIconViewDetails;

typedef struct {
	NautilusView parent_instance;
	NautilusIconViewDetails *details;
} NautilusIconView;

typedef struct {
	NautilusViewClass parent_class;
} NautilusIconViewClass;

GType nautilus_icon_view_get_type (void);
void  nautilus_icon_view_register (void);

GtkIconView * nautilus_icon_view_get_icon_view (NautilusIconView *icon_view);

#endif /* NAUTILUS_ICON_VIEW_H */
