/*
 *  Copyright (C) 2002 Christophe Fergeau
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Based on ephy-arrow-toolbutton.h from Epiphany
 */

#ifndef NAUTILUS_ARROW_TOOLBUTTON_H
#define NAUTILUS_ARROW_TOOLBUTTON_H

#include <glib.h>
#include <gtk/gtkmenushell.h>
#include <gtk/gtktoolbutton.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_ARROW_TOOLBUTTON	   (nautilus_arrow_toolbutton_get_type ())
#define NAUTILUS_ARROW_TOOLBUTTON(o)	   (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_ARROW_TOOLBUTTON, NautilusArrowToolButton))
#define NAUTILUS_ARROW_TOOLBUTTON_CLASS(k)	   (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_ARROW_TOOLBUTTON, NautilusArrowToolButtonClass))
#define NAUTILUS_IS_ARROW_TOOLBUTTON(o)	   (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_ARROW_TOOLBUTTON))
#define NAUTILUS_IS_ARROW_TOOLBUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_ARROW_TOOLBUTTON))
#define NAUTILUS_ARROW_TOOLBUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_ARROW_TOOLBUTTON, NautilusArrowToolButtonClass))

typedef struct NautilusArrowToolButtonClass NautilusArrowToolButtonClass;
typedef struct NautilusArrowToolButton NautilusArrowToolButton;
typedef struct NautilusArrowToolButtonPrivate NautilusArrowToolButtonPrivate;

struct NautilusArrowToolButton
{
	GtkToolButton parent;

	/*< private >*/
        NautilusArrowToolButtonPrivate *priv;
};

struct NautilusArrowToolButtonClass
{
        GtkToolButtonClass parent_class;

	void (*menu_activated) (NautilusArrowToolButton *b);
};

GType		nautilus_arrow_toolbutton_get_type		(void);

GtkMenuShell    *nautilus_arrow_toolbutton_get_menu		(NautilusArrowToolButton *b);

G_END_DECLS;

#endif /* NAUTILUS_ARROW_TOOLBUTTON_H */
