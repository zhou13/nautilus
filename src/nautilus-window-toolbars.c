/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
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
 * Author: John Sullivan <sullivan@eazel.com> 
 */

/* nautilus-window-toolbars.c - implementation of nautilus window toolbar operations,
 * split into separate file just for convenience.
 */

#include <config.h>

#include <unistd.h>
#include "nautilus-application.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include "nautilus-window.h"
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-string.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktogglebutton.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-popup-menu.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-bookmark.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-theme.h>

/* FIXME bugzilla.gnome.org 41243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nautilus-desktop-window.h"

#define TOOLBAR_PATH_EXTENSION_ACTIONS "/Toolbar/Extra Buttons Placeholder/Extension Actions"

enum {
	TOOLBAR_ITEM_STYLE_PROP,
	TOOLBAR_ITEM_ORIENTATION_PROP
};

#ifdef BONOBO_DONE
static void
throbber_set_throbbing (NautilusNavigationWindow *window,
			gboolean        throbbing)
{
	CORBA_boolean b;
	CORBA_any     val;

	val._type = TC_CORBA_boolean;
	val._value = &b;
	b = throbbing;

	bonobo_pbclient_set_value_async (
		window->details->throbber_property_bag,
		"throbbing", &val, NULL);
}

static void
throbber_created_callback (Bonobo_Unknown     throbber,
			   CORBA_Environment *ev,
			   gpointer           user_data)
{
	char *exception_as_text;
	NautilusNavigationWindow *window;

	if (BONOBO_EX (ev)) {
		exception_as_text = bonobo_exception_get_text (ev);
		g_warning ("Throbber activation exception '%s'", exception_as_text);
		g_free (exception_as_text);
		return;
	}

	g_return_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (user_data));

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

	window->details->throbber_activating = FALSE;

	bonobo_ui_component_object_set (NAUTILUS_WINDOW (window)->details->shell_ui,
					"/Toolbar/ThrobberWrapper",
					throbber, ev);
	CORBA_exception_free (ev);

	window->details->throbber_property_bag =
		Bonobo_Control_getProperties (throbber, ev);

	if (BONOBO_EX (ev)) {
		window->details->throbber_property_bag = CORBA_OBJECT_NIL;
		CORBA_exception_free (ev);
	} else {
		throbber_set_throbbing (window, window->details->throbber_active);
	}

	bonobo_object_release_unref (throbber, ev);

	g_object_unref (window);
}

#endif

void
nautilus_navigation_window_set_throbber_active (NautilusNavigationWindow *window, 
						gboolean allow)
{
#ifdef BONOBO_DONE
	if (( window->details->throbber_active &&  allow) ||
	    (!window->details->throbber_active && !allow)) {
		return;
	}

	if (allow)
		access ("nautilus-throbber: start", 0);
	else
		access ("nautilus-throbber: stop", 0);

	nautilus_bonobo_set_sensitive (NAUTILUS_WINDOW (window)->details->shell_ui,
				       NAUTILUS_COMMAND_STOP, allow);

	window->details->throbber_active = allow;
	if (window->details->throbber_property_bag != CORBA_OBJECT_NIL) {
		throbber_set_throbbing (window, allow);
	}
#endif
}

void
nautilus_navigation_window_activate_throbber (NautilusNavigationWindow *window)
{
#ifdef BONOBO_DONE
	CORBA_Environment ev;
	char *exception_as_text;

	if (window->details->throbber_activating ||
	    window->details->throbber_property_bag != CORBA_OBJECT_NIL) {
		return;
	}

	/* FIXME bugzilla.gnome.org 41243: 
	 * We should use inheritance instead of these special cases
	 * for the desktop window.
	 */
	if (!NAUTILUS_IS_DESKTOP_WINDOW (window)) {
		CORBA_exception_init (&ev);

		g_object_ref (window);
		bonobo_get_object_async ("OAFIID:Nautilus_Throbber",
					 "IDL:Bonobo/Control:1.0",
					 &ev,
					 throbber_created_callback,
					 window);

		if (BONOBO_EX (&ev)) {
			exception_as_text = bonobo_exception_get_text (&ev);
			g_warning ("Throbber activation exception '%s'", exception_as_text);
			g_free (exception_as_text);
		}
		CORBA_exception_free (&ev);
		window->details->throbber_activating = TRUE;		
	}
#endif
}

void
nautilus_navigation_window_initialize_toolbars (NautilusNavigationWindow *window)
{
	nautilus_navigation_window_activate_throbber (window);
}


static GList *
get_extension_toolbar_items (NautilusNavigationWindow *window)
{
	GList *items;
	GList *providers;
	GList *l;
	
	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
	items = NULL;

	for (l = providers; l != NULL; l = l->next) {
		NautilusMenuProvider *provider;
		GList *file_items;
		
		provider = NAUTILUS_MENU_PROVIDER (l->data);
		file_items = nautilus_menu_provider_get_toolbar_items 
			(provider, 
			 GTK_WIDGET (window),
			 NAUTILUS_WINDOW (window)->details->viewed_file);
		items = g_list_concat (items, file_items);		
	}

	nautilus_module_extension_list_free (providers);

	return items;
}

static void
extension_action_callback (GtkAction *action,
			   gpointer callback_data)
{
	nautilus_menu_item_activate (NAUTILUS_MENU_ITEM (callback_data));
}

void
nautilus_navigation_window_load_extension_toolbar_items (NautilusNavigationWindow *window)
{
	char *name, *label, *tip, *icon;
	gboolean sensitive, priority;
	GtkActionGroup *action_group;
	GtkAction *action;
	GdkPixbuf *pixbuf;
	GtkUIManager *ui_manager;
	GList *items;
	GList *l;
	
	guint merge_id;

	ui_manager = nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window));
	if (window->details->extensions_toolbar_merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager,
					  window->details->extensions_toolbar_merge_id);
		window->details->extensions_toolbar_merge_id = 0;
	}

	if (window->details->extensions_toolbar_action_group != NULL) {
		gtk_ui_manager_remove_action_group (ui_manager,
						    window->details->extensions_toolbar_action_group);
		window->details->extensions_toolbar_action_group = NULL;
	}
	
	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	window->details->extensions_toolbar_merge_id = merge_id;
	action_group = gtk_action_group_new ("ExtensionsMenuGroup");
	window->details->extensions_toolbar_action_group = action_group;
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	items = get_extension_toolbar_items (window);

	for (l = items; l != NULL; l = l->next) {
		NautilusMenuItem *item;
		
		item = NAUTILUS_MENU_ITEM (l->data);

		g_object_get (G_OBJECT (item), 
			      "name", &name, "label", &label, 
			      "tip", &tip, "icon", &icon,
			      "sensitive", &sensitive,
			      "priority", &priority,
			      NULL);

		action = gtk_action_new (name,
					 label,
					 tip,
					 icon);

		/* TODO: This should really use themed icons, but that
		   doesn't work here yet */
		if (icon != NULL) {
			pixbuf = nautilus_icon_factory_get_pixbuf_from_name 
				(icon,
				 NULL,
				 24,
				 NULL);
			if (pixbuf != NULL) {
				g_object_set_data_full (G_OBJECT (action), "toolbar-icon",
							pixbuf,
							g_object_unref);
			}
		}
		
		gtk_action_set_sensitive (action, sensitive);
		g_object_set (action, "is-important", priority, NULL);

		g_signal_connect_data (action, "activate",
				       G_CALLBACK (extension_action_callback),
				       g_object_ref (item), 
				       (GClosureNotify)g_object_unref, 0);
		
		gtk_action_group_add_action (action_group,
					     GTK_ACTION (action));
		g_object_unref (action);
		
		gtk_ui_manager_add_ui (ui_manager,
				       merge_id,
				       TOOLBAR_PATH_EXTENSION_ACTIONS,
				       name,
				       name,
				       GTK_UI_MANAGER_TOOLITEM,
				       FALSE);

		g_free (name);
		g_free (label);
		g_free (tip);
		g_free (icon);
		
		g_object_unref (item);
	}

	g_list_free (items);
}
