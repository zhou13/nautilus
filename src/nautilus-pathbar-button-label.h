/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nautilus-pathbar-button-label.h
 *
 * Copyright (C) 2014  Carlos Soriano <carlos.soriano89@gmail.com>
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors Carlos Soriano <carlos.soriano89@gmail.com>
 */

#ifndef NAUTILUS_PATHBAR_BUTTON_LABEL_H
#define NAUTILUS_PATHBAR_BUTTON_LABEL_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PATHBAR_BUTTON_LABEL            (nautilus_pathbar_button_label_get_type())
#define NAUTILUS_PATHBAR_BUTTON_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PATHBAR_BUTTON_LABEL, NautilusPathbarButtonLabel))
#define NAUTILUS_PATHBAR_BUTTON_LABEL_CONST(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PATHBAR_BUTTON_LABEL, NautilusPathbarButtonLabel const))
#define NAUTILUS_PATHBAR_BUTTON_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NAUTILUS_TYPE_PATHBAR_BUTTON_LABEL, NautilusPathbarButtonLabelClass))
#define NAUTILUS_IS_PATHBAR_BUTTON_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_PATHBAR_BUTTON_LABEL))
#define NAUTILUS_IS_PATHBAR_BUTTON_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NAUTILUS_TYPE_PATHBAR_BUTTON_LABEL))
#define NAUTILUS_PATHBAR_BUTTON_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NAUTILUS_TYPE_PATHBAR_BUTTON_LABEL, NautilusPathbarButtonLabelClass))

typedef struct _NautilusPathbarButtonLabel        NautilusPathbarButtonLabel;
typedef struct _NautilusPathbarButtonLabelClass   NautilusPathbarButtonLabelClass;
typedef struct _NautilusPathbarButtonLabelPrivate NautilusPathbarButtonLabelPrivate;

struct _NautilusPathbarButtonLabel
{
	GtkBox parent;

	/*< private >*/
	NautilusPathbarButtonLabelPrivate *priv;
};

struct _NautilusPathbarButtonLabelClass
{
	GtkBoxClass parent;
};

GType nautilus_pathbar_button_label_get_type (void) G_GNUC_CONST;
NautilusPathbarButtonLabel *nautilus_pathbar_button_label_new (void);

void nautilus_pathbar_button_label_set_text(NautilusPathbarButtonLabel *self,
				            gchar *dir_name);
void nautilus_pathbar_button_label_set_bold(NautilusPathbarButtonLabel *self,
				       	    gboolean		       *bold);

G_END_DECLS

#endif /* NAUTILUS_PATHBAR_BUTTON_LABEL_H */
