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
 *  Based on ephy-arrow-toolbutton.c from Epiphany
 */

#include <config.h>

#include "nautilus-arrow-toolbutton.h"

#include <gtk/gtkarrow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmain.h>
#include <gdk/gdkkeysyms.h>

#define NAUTILUS_ARROW_TOOLBUTTON_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), NAUTILUS_TYPE_ARROW_TOOLBUTTON, NautilusArrowToolButtonPrivate))

struct NautilusArrowToolButtonPrivate
{
	GtkWidget *arrow_widget;
	GtkWidget *button;
	GtkMenu *menu;
};

enum NautilusArrowToolButtonSignalsEnum {
	NAUTILUS_ARROW_TOOL_BUTTON_MENU_ACTIVATED,
	NAUTILUS_ARROW_TOOL_BUTTON_LAST_SIGNAL
};

/* GObject boilerplate code */
static void nautilus_arrow_toolbutton_init         (NautilusArrowToolButton *arrow_toolbutton);
static void nautilus_arrow_toolbutton_class_init   (NautilusArrowToolButtonClass *klass);
static void nautilus_arrow_toolbutton_finalize     (GObject *object);

static GObjectClass *parent_class = NULL;

static gint NautilusArrowToolButtonSignals[NAUTILUS_ARROW_TOOL_BUTTON_LAST_SIGNAL];

GType
nautilus_arrow_toolbutton_get_type (void)
{
        static GType nautilus_arrow_toolbutton_type = 0;
	
        if (nautilus_arrow_toolbutton_type == 0) {
		static const GTypeInfo our_info = {
			sizeof (NautilusArrowToolButtonClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) nautilus_arrow_toolbutton_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (NautilusArrowToolButton),
			0, /* n_preallocs */
			(GInstanceInitFunc) nautilus_arrow_toolbutton_init
		};
		
		nautilus_arrow_toolbutton_type = g_type_register_static (GTK_TYPE_TOOL_BUTTON,
									 "NautilusArrowToolButton",
									 &our_info, 0);
        }
	
        return nautilus_arrow_toolbutton_type;
}


static gboolean
nautilus_arrow_toolbutton_set_tooltip (GtkToolItem *tool_item,
				       GtkTooltips *tooltips,
				       const char *tip_text,
				       const char *tip_private)
{
	NautilusArrowToolButton *button = NAUTILUS_ARROW_TOOLBUTTON (tool_item);
	
	g_return_val_if_fail (NAUTILUS_IS_ARROW_TOOLBUTTON (button), FALSE);

	gtk_tooltips_set_tip (tooltips, button->priv->arrow_widget, tip_text, tip_private);
	gtk_tooltips_set_tip (tooltips, button->priv->button, tip_text, tip_private);

	return TRUE;
}

static void
nautilus_arrow_toolbutton_class_init (NautilusArrowToolButtonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkToolItemClass *tool_item_class = GTK_TOOL_ITEM_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = nautilus_arrow_toolbutton_finalize;

	tool_item_class->set_tooltip = nautilus_arrow_toolbutton_set_tooltip;

	NautilusArrowToolButtonSignals[NAUTILUS_ARROW_TOOL_BUTTON_MENU_ACTIVATED] =
		g_signal_new
		("menu-activated", G_OBJECT_CLASS_TYPE (klass),
		 G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
                 G_STRUCT_OFFSET (NautilusArrowToolButtonClass, menu_activated),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

	g_type_class_add_private (object_class, sizeof (NautilusArrowToolButtonPrivate));
}

static void
button_state_changed_cb (GtkWidget *widget,
			 GtkStateType previous_state,
			 NautilusArrowToolButton *b)
{
	NautilusArrowToolButtonPrivate *p = b->priv;
	GtkWidget *button;
	GtkStateType state = GTK_WIDGET_STATE (widget);

	button = (widget == p->arrow_widget) ? p->button : p->arrow_widget;

	g_signal_handlers_block_by_func
		(G_OBJECT (button),
		 G_CALLBACK (button_state_changed_cb),
		 b);
	if (state == GTK_STATE_PRELIGHT &&
	    previous_state != GTK_STATE_ACTIVE) {
		gtk_widget_set_state (button, state);
	} else if (state == GTK_STATE_NORMAL) {
		gtk_widget_set_state (button, state);
	} else if (state == GTK_STATE_ACTIVE) {
		gtk_widget_set_state (button, GTK_STATE_NORMAL);
	}
	g_signal_handlers_unblock_by_func
		(G_OBJECT (button),
		 G_CALLBACK (button_state_changed_cb),
		 b);
}

static void
menu_position_under_widget (GtkMenu   *menu,
			    gint      *x,
			    gint      *y,
			    gboolean  *push_in,
			    gpointer	user_data)
{
	GtkWidget *w = GTK_WIDGET (user_data);
	gint screen_width, screen_height;
	GtkRequisition requisition;
	gboolean rtl;

	rtl = (gtk_widget_get_direction (w) == GTK_TEXT_DIR_RTL);

	gdk_window_get_origin (w->window, x, y);
	gtk_widget_size_request (GTK_WIDGET (menu), &requisition);

	/* FIXME multihead */
	screen_width = gdk_screen_width ();
	screen_height = gdk_screen_height ();

	if (rtl)
	{
		*x += w->allocation.x + w->allocation.width - requisition.width;
	}
	else
	{
		*x += w->allocation.x;
	}

	*y += w->allocation.y + w->allocation.height;

	*x = CLAMP (*x, 0, MAX (0, screen_width - requisition.width));
	*y = CLAMP (*y, 0, MAX (0, screen_height - requisition.height));
}


static void
popup_menu_under_arrow (NautilusArrowToolButton *b, GdkEventButton *event)
{
	NautilusArrowToolButtonPrivate *p = b->priv;
	
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (p->arrow_widget), TRUE);
	
	g_signal_emit (b, NautilusArrowToolButtonSignals[NAUTILUS_ARROW_TOOL_BUTTON_MENU_ACTIVATED], 0);
	gtk_menu_popup (p->menu, NULL, NULL, menu_position_under_widget, b,
			event ? event->button : 0,
			event ? event->time : gtk_get_current_event_time ());
}

static void
menu_deactivated_cb (GtkMenuShell *ms, NautilusArrowToolButton *b)
{
	NautilusArrowToolButtonPrivate *p = b->priv;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (p->arrow_widget), FALSE);
}

static gboolean
arrow_button_press_event_cb  (GtkWidget *widget, GdkEventButton *event, NautilusArrowToolButton *b)
{
	popup_menu_under_arrow (b, event);
	return TRUE;
}

static gboolean
arrow_key_press_event_cb (GtkWidget *widget, GdkEventKey *event, NautilusArrowToolButton *b)
{
	if (event->keyval == GDK_space
	    || event->keyval == GDK_KP_Space
	    || event->keyval == GDK_Return
	    || event->keyval == GDK_KP_Enter
	    || event->keyval == GDK_Menu) {
		popup_menu_under_arrow (b, NULL);
	}

	return FALSE;
}

static void
nautilus_arrow_toolbutton_init (NautilusArrowToolButton *arrowtb)
{
	GtkWidget *hbox;
	GtkWidget *arrow;
	GtkWidget *arrow_button;
	GtkWidget *real_button;

	arrowtb->priv = NAUTILUS_ARROW_TOOLBUTTON_GET_PRIVATE (arrowtb);

	gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (arrowtb), FALSE);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	real_button = GTK_BIN (arrowtb)->child;
	g_object_ref (real_button);
	gtk_container_remove (GTK_CONTAINER (arrowtb), real_button);
	gtk_container_add (GTK_CONTAINER (hbox), real_button);
	gtk_container_add (GTK_CONTAINER (arrowtb), hbox);
	g_object_unref (real_button);

	arrow_button = gtk_toggle_button_new ();
	gtk_widget_show (arrow_button);
	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_button_set_relief (GTK_BUTTON (arrow_button), GTK_RELIEF_NONE);
	gtk_container_add (GTK_CONTAINER (arrow_button), arrow);

	gtk_box_pack_end (GTK_BOX (hbox), arrow_button,
			  FALSE, FALSE, 0);

	arrowtb->priv->button = real_button;
	arrowtb->priv->arrow_widget = arrow_button;

	arrowtb->priv->menu = GTK_MENU (gtk_menu_new ());
	g_object_ref (arrowtb->priv->menu);
	gtk_object_sink (GTK_OBJECT (arrowtb->priv->menu));

	g_signal_connect (arrowtb->priv->menu, "deactivate",
			  G_CALLBACK (menu_deactivated_cb), arrowtb);

	g_signal_connect (real_button, "state_changed",
			  G_CALLBACK (button_state_changed_cb),
			  arrowtb);
	g_signal_connect (arrow_button, "state_changed",
			  G_CALLBACK (button_state_changed_cb),
			  arrowtb);
	g_signal_connect (arrow_button, "key_press_event",
			  G_CALLBACK (arrow_key_press_event_cb),
			  arrowtb);
	g_signal_connect (arrow_button, "button_press_event",
			  G_CALLBACK (arrow_button_press_event_cb),
			  arrowtb);
}

static void
nautilus_arrow_toolbutton_finalize (GObject *object)
{
	NautilusArrowToolButton *arrow_toolbutton = NAUTILUS_ARROW_TOOLBUTTON (object);

	g_object_unref (arrow_toolbutton->priv->menu);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkMenuShell *
nautilus_arrow_toolbutton_get_menu (NautilusArrowToolButton *b)
{
	return GTK_MENU_SHELL (b->priv->menu);
}
