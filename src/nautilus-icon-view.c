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

#include <config.h>

#include "nautilus-icon-view.h"

#include "nautilus-list-model.h"
#include "nautilus-error-reporting.h"
#include "nautilus-view-dnd.h"
#include "nautilus-view-factory.h"

#include <string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <libnautilus-extension/nautilus-column-provider.h>
#include <libnautilus-private/nautilus-clipboard-monitor.h>
#include <libnautilus-private/nautilus-column-utilities.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-clipboard.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_ICON_VIEW
#include <libnautilus-private/nautilus-debug.h>

/* Wait for the rename to end when activating a file being renamed */
#define WAIT_FOR_RENAME_ON_ACTIVATE 200

static GdkCursor *hand_cursor = NULL;

struct NautilusIconViewDetails {
	GtkIconView *icon_view;

	GtkCellRenderer *pixbuf_cell;
	GtkCellRenderer *text_cell;
	int text_column_num;
	GtkCellEditable *editable_widget;

	NautilusListModel *model;
	GtkActionGroup *icon_action_group;
	guint icon_merge_id;

	NautilusZoomLevel zoom_level;

	GtkTreePath *double_click_path[2]; /* Both clicks in a double click need to be on the same item */
	GtkTreePath *new_selection_path;   /* Path of the new selection after removing a file */
	GtkTreePath *hover_path;

	guint drag_button;
	int drag_x;
	int drag_y;

	gboolean drag_started;
	gboolean ignore_button_release;
	gboolean item_selected_on_button_down;
	gboolean menus_ready;
	gboolean active;

	GHashTable *columns;
	GtkWidget *column_editor;

	NautilusFile *renaming_file;
	gboolean rename_done;
	guint renaming_file_activate_timeout;
	char *original_name;

	GQuark last_sort_attr;
};

struct SelectionForeachData {
	GList *list;
	GtkTreeSelection *selection;
};

G_DEFINE_TYPE (NautilusIconView, nautilus_icon_view, NAUTILUS_TYPE_VIEW);

static void
selection_changed_callback (GtkIconView *icon_view, gpointer user_data)
{
	NautilusView *view;

	view = NAUTILUS_VIEW (user_data);

	nautilus_view_notify_selection_changed (view);
}

static GList *
nautilus_icon_view_get_selection (NautilusView *view)
{
	GList *paths;
	GList *files = NULL;
	GList *l;

	paths = gtk_icon_view_get_selected_items (NAUTILUS_ICON_VIEW (view)->details->icon_view);
	for (l = paths; l != NULL; l = l->next) {
		GtkTreePath *path = l->data;
		NautilusFile *file;

		file = nautilus_list_model_file_for_path (NAUTILUS_ICON_VIEW (view)->details->model, path);
		if (file != NULL)
			files = g_list_prepend (files, file);
	}

	g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);

	return g_list_reverse (files);
}

static void
activate_selected_items (NautilusIconView *view)
{
	GList *file_list;

	file_list = nautilus_icon_view_get_selection (NAUTILUS_VIEW (view));

	if (view->details->renaming_file) {
		/* We're currently renaming a file, wait until the rename is
		   finished, or the activation uri will be wrong */
		if (view->details->renaming_file_activate_timeout == 0) {
			view->details->renaming_file_activate_timeout =
				g_timeout_add (WAIT_FOR_RENAME_ON_ACTIVATE, (GSourceFunc) activate_selected_items, view);
		}
		return;
	}

	if (view->details->renaming_file_activate_timeout != 0) {
		g_source_remove (view->details->renaming_file_activate_timeout);
		view->details->renaming_file_activate_timeout = 0;
	}

	nautilus_view_activate_files (NAUTILUS_VIEW (view),
				      file_list,
				      0, TRUE);
	nautilus_file_list_free (file_list);

}

static void
activate_selected_items_alternate (NautilusIconView *view,
				   NautilusFile     *file,
				   gboolean          open_in_tab)
{
	GList *file_list;
	NautilusWindowOpenFlags flags;

	flags = 0;

	if (open_in_tab) {
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
	} else {
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
	}

	if (file != NULL) {
		nautilus_file_ref (file);
		file_list = g_list_prepend (NULL, file);
	} else {
		file_list = nautilus_icon_view_get_selection (NAUTILUS_VIEW (view));
	}
	nautilus_view_activate_files (NAUTILUS_VIEW (view),
				      file_list,
				      flags,
				      TRUE);
	nautilus_file_list_free (file_list);

}

static void
preview_selected_items (NautilusIconView *view)
{
	GList *file_list;

	file_list = nautilus_icon_view_get_selection (NAUTILUS_VIEW (view));

	if (file_list != NULL) {
		nautilus_view_preview_files (NAUTILUS_VIEW (view),
					     file_list, NULL);
		nautilus_file_list_free (file_list);
	}
}

static gboolean
key_press_callback (GtkWidget   *widget,
		    GdkEventKey *event,
		    gpointer     callback_data)
{
	NautilusView *view;
	GdkEventButton button_event = { 0 };
	gboolean handled;

	view = NAUTILUS_VIEW (callback_data);
	handled = FALSE;

	switch (event->keyval) {
	case GDK_KEY_F10:
		if (event->state & GDK_CONTROL_MASK) {
			nautilus_view_pop_up_background_context_menu (view, &button_event);
			handled = TRUE;
		}
		break;
	case GDK_KEY_space:
		if (event->state & GDK_CONTROL_MASK) {
			handled = FALSE;
			break;
		}
		if (!gtk_widget_has_focus (GTK_WIDGET (NAUTILUS_ICON_VIEW (view)->details->icon_view))) {
			handled = FALSE;
			break;
		}
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			activate_selected_items_alternate (NAUTILUS_ICON_VIEW (view), NULL, TRUE);
		} else {
			preview_selected_items (NAUTILUS_ICON_VIEW (view));
		}
		handled = TRUE;
		break;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			activate_selected_items_alternate (NAUTILUS_ICON_VIEW (view), NULL, TRUE);
		} else {
			activate_selected_items (NAUTILUS_ICON_VIEW (view));
		}
		handled = TRUE;
		break;
	case GDK_KEY_v:
		/* Eat Control + v to not enable type ahead */
		if ((event->state & GDK_CONTROL_MASK) != 0) {
			handled = TRUE;
		}
		break;

	default:
		handled = FALSE;
	}

	return handled;
}

static void
nautilus_icon_view_reveal_selection (NautilusView *view)
{
	GList *selection;

	g_return_if_fail (NAUTILUS_IS_ICON_VIEW (view));

        selection = nautilus_view_get_selection (view);

	/* Make sure at least one of the selected items is scrolled into view */
	if (selection != NULL) {
		NautilusIconView *icon_view;
		NautilusFile *file;
		GtkTreeIter iter;
		GtkTreePath *path;

		icon_view = NAUTILUS_ICON_VIEW (view);
		file = selection->data;
		if (nautilus_list_model_get_first_iter_for_file (icon_view->details->model, file, &iter)) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (icon_view->details->model), &iter);
			gtk_icon_view_scroll_to_path (icon_view->details->icon_view, path, FALSE, 0.0, 0.0);
			gtk_tree_path_free (path);
		}
	}

        nautilus_file_list_free (selection);
}

static void
editable_focus_out_cb (GtkWidget *widget,
		       GdkEvent *event,
		       gpointer user_data)
{
	NautilusIconView *view = user_data;

	view->details->editable_widget = NULL;

	nautilus_view_set_is_renaming (NAUTILUS_VIEW (view), FALSE);
	nautilus_view_unfreeze_updates (NAUTILUS_VIEW (view));
}

static void
cell_renderer_editing_started_cb (GtkCellRenderer  *renderer,
				  GtkCellEditable  *editable,
				  const gchar      *path_str,
				  NautilusIconView *icon_view)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (editable);
	icon_view->details->editable_widget = editable;

	/* Free a previously allocated original_name */
	g_free (icon_view->details->original_name);

	icon_view->details->original_name = g_strdup (gtk_entry_get_text (entry));

	g_signal_connect (entry, "focus-out-event",
			  G_CALLBACK (editable_focus_out_cb), icon_view);

	nautilus_clipboard_set_up_editable (GTK_EDITABLE (entry),
					    nautilus_view_get_ui_manager (NAUTILUS_VIEW (icon_view)),
					    FALSE);
}

static void
cell_renderer_editing_canceled_cb (GtkCellRendererText *cell,
				   NautilusIconView    *view)
{
	view->details->editable_widget = NULL;

	nautilus_view_set_is_renaming (NAUTILUS_VIEW (view), FALSE);
	nautilus_view_unfreeze_updates (NAUTILUS_VIEW (view));
}

static void
nautilus_icon_view_rename_callback (NautilusFile *file,
				    GFile        *result_location,
				    GError       *error,
				    gpointer      callback_data)
{
	NautilusIconView *view;

	view = NAUTILUS_ICON_VIEW (callback_data);

	if (view->details->renaming_file) {
		view->details->rename_done = TRUE;

		if (error != NULL) {
			/* If the rename failed (or was cancelled), kill renaming_file.
			 * We won't get a change event for the rename, so otherwise
			 * it would stay around forever.
			 */
			nautilus_file_unref (view->details->renaming_file);
			view->details->renaming_file = NULL;
		}
	}

	g_object_unref (view);
}

static void
cell_renderer_edited_cb (GtkCellRendererText *cell,
			 const char          *path_str,
			 const char          *new_text,
			 NautilusIconView    *view)
{
	GtkTreePath *path;
	NautilusFile *file;
	GtkTreeIter iter;

	view->details->editable_widget = NULL;
	nautilus_view_set_is_renaming (NAUTILUS_VIEW (view), FALSE);

	/* Don't allow a rename with an empty string. Revert to original
	 * without notifying the user.
	 */
	if (new_text[0] == '\0') {
		g_object_set (G_OBJECT (view->details->text_cell),
			      "editable", FALSE,
			      NULL);
		nautilus_view_unfreeze_updates (NAUTILUS_VIEW (view));
		return;
	}

	path = gtk_tree_path_new_from_string (path_str);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
				 &iter, path);

	gtk_tree_path_free (path);

	gtk_tree_model_get (GTK_TREE_MODEL (view->details->model),
			    &iter,
			    NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	/* Only rename if name actually changed */
	if (strcmp (new_text, view->details->original_name) != 0) {
		view->details->renaming_file = nautilus_file_ref (file);
		view->details->rename_done = FALSE;
		nautilus_rename_file (file, new_text, nautilus_icon_view_rename_callback, g_object_ref (view));
		g_free (view->details->original_name);
		view->details->original_name = g_strdup (new_text);
	}

	nautilus_file_unref (file);

	/*We're done editing - make the text-cell readonly again.*/
	g_object_set (G_OBJECT (view->details->text_cell),
		      "editable", FALSE,
		      NULL);

	nautilus_view_unfreeze_updates (NAUTILUS_VIEW (view));
}

static void
update_text_cell (NautilusIconView *view)
{
	if (view->details->text_column_num == -1) {
		if (view->details->text_cell != NULL) {
			/* FIXME: remove it */
		}
	} else {
		if (view->details->text_cell == NULL) {
			view->details->text_cell = gtk_cell_renderer_text_new ();
			gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (view->details->icon_view),
						  view->details->text_cell,
						  FALSE);
		}
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (view->details->icon_view),
						view->details->text_cell,
						"text", view->details->text_column_num,
						NULL);
		g_object_set (view->details->text_cell,
			      "alignment", PANGO_ALIGN_CENTER,
			      "wrap-mode", PANGO_WRAP_WORD_CHAR,
			      "ellipsize", PANGO_ELLIPSIZE_MIDDLE,
			      "xalign", 0.5,
			      "yalign", 0.0,
			      NULL);

		g_signal_connect (view->details->text_cell, "edited",
				  G_CALLBACK (cell_renderer_edited_cb), view);
		g_signal_connect (view->details->text_cell, "editing-canceled",
				  G_CALLBACK (cell_renderer_editing_canceled_cb), view);
		g_signal_connect (view->details->text_cell, "editing-started",
				  G_CALLBACK (cell_renderer_editing_started_cb), view);

	}
}

static void
icon_view_selection_foreach_set_boolean (GtkIconView *icon_view,
					 GtkTreePath *path,
					 gpointer callback_data)
{
	* (gboolean *) callback_data = TRUE;
}

static gboolean
icon_view_has_selection (GtkIconView *icon_view)
{
	gboolean not_empty;

	not_empty = FALSE;
	gtk_icon_view_selected_foreach (icon_view,
					icon_view_selection_foreach_set_boolean,
					&not_empty);
	return not_empty;
}

static void
do_popup_menu (GtkWidget        *widget,
	       NautilusIconView *view,
	       GdkEventButton   *event)
{
	if (icon_view_has_selection (GTK_ICON_VIEW (widget))) {
		nautilus_view_pop_up_selection_context_menu (NAUTILUS_VIEW (view), event);
	} else {
                nautilus_view_pop_up_background_context_menu (NAUTILUS_VIEW (view), event);
	}
}

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
	return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static int
get_click_policy (void)
{
	return g_settings_get_enum (nautilus_preferences,
				    NAUTILUS_PREFERENCES_CLICK_POLICY);
}

static void
nautilus_icon_view_did_not_drag (NautilusIconView *view,
				 GdkEventButton   *event)
{
	GtkIconView *icon_view;
	GtkTreePath *path;

	icon_view = view->details->icon_view;

	path = gtk_icon_view_get_path_at_pos (icon_view, event->x, event->y);
	if (path != NULL) {
		if ((event->button == 1 || event->button == 2)
		    && ((event->state & GDK_CONTROL_MASK) != 0 ||
			(event->state & GDK_SHIFT_MASK) == 0)
		    && view->details->item_selected_on_button_down) {
			if (!button_event_modifies_selection (event)) {
				gtk_icon_view_unselect_all (icon_view);
				gtk_icon_view_select_path (icon_view, path);
			} else {
				gtk_icon_view_unselect_path (icon_view, path);
			}
		}

		if ((get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE)
		    && !button_event_modifies_selection (event)) {
			if (event->button == 1) {
				activate_selected_items (view);
			} else if (event->button == 2) {
				activate_selected_items_alternate (view, NULL, TRUE);
			}
		}
		gtk_tree_path_free (path);
	}
}

static void
item_activated_callback (GtkIconView      *iconview,
			 GtkTreePath      *path,
			 NautilusIconView *view)
{
	activate_selected_items (view);
}

static gboolean
button_press_callback (GtkWidget *widget,
		       GdkEventButton *event,
		       gpointer callback_data)
{
	NautilusIconView *view;
	GtkIconView *icon_view;
	GtkTreePath *path;
	gboolean call_parent;
	GtkWidgetClass *icon_view_class;
	gint64 current_time;
	static gint64 last_click_time = 0;
	static int click_count = 0;
	int double_click_time;

	view = NAUTILUS_ICON_VIEW (callback_data);
	icon_view = GTK_ICON_VIEW (widget);
	icon_view_class = GTK_WIDGET_GET_CLASS (icon_view);

	/* Don't handle extra mouse buttons here */
	if (event->button > 5) {
		return FALSE;
	}

#if 0
	if (event->window != gtk_tree_view_get_bin_window (tree_view)) {
		return FALSE;
	}
#endif

#if 0
	nautilus_list_model_set_drag_view
		(NAUTILUS_LIST_MODEL (gtk_icon_view_get_model (icon_view)),
		 icon_view,
		 event->x, event->y);
#endif

	g_object_get (G_OBJECT (gtk_widget_get_settings (widget)),
		      "gtk-double-click-time", &double_click_time,
		      NULL);

	/* Determine click count */
	current_time = g_get_monotonic_time ();
	if (current_time - last_click_time < double_click_time * 1000) {
		click_count++;
	} else {
		click_count = 0;
	}

	/* Stash time for next compare */
	last_click_time = current_time;

	/* Ignore double click if we are in single click mode */
	if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE && click_count >= 2) {
		return TRUE;
	}

	call_parent = TRUE;
	path = gtk_icon_view_get_path_at_pos (icon_view, event->x, event->y);
	if (path != NULL) {
		/* Keep track of path of last click so double clicks only happen
		 * on the same item */
		if ((event->button == 1 || event->button == 2)  &&
		    event->type == GDK_BUTTON_PRESS) {
			if (view->details->double_click_path[1]) {
				gtk_tree_path_free (view->details->double_click_path[1]);
			}
			view->details->double_click_path[1] = view->details->double_click_path[0];
			view->details->double_click_path[0] = gtk_tree_path_copy (path);
		}
		if (event->type == GDK_2BUTTON_PRESS) {
			/* Double clicking does not trigger a D&D action. */
			view->details->drag_button = 0;
			if (view->details->double_click_path[1] &&
			    gtk_tree_path_compare (view->details->double_click_path[0], view->details->double_click_path[1]) == 0) {
				/* NOTE: Activation can actually destroy the view if we're switching */
				if (!button_event_modifies_selection (event)) {
					if ((event->button == 1 || event->button == 3)) {
						activate_selected_items (view);
					} else if (event->button == 2) {
						activate_selected_items_alternate (view, NULL, TRUE);
					}
				} else if (event->button == 1 &&
					   (event->state & GDK_SHIFT_MASK) != 0) {
					NautilusFile *file;
					file = nautilus_list_model_file_for_path (view->details->model, path);
					if (file != NULL) {
						activate_selected_items_alternate (view, file, TRUE);
						nautilus_file_unref (file);
					}
				}
			} else {
				icon_view_class->button_press_event (widget, event);
			}
		} else {
			/* We're going to filter out some situations where
			 * we can't let the default code run because all
			 * but one item would be would be deselected. We don't
			 * want that; we want the right click menu or single
			 * click to apply to everything that's currently selected. */

			if (event->button == 3 && gtk_icon_view_path_is_selected (icon_view, path)) {
				call_parent = FALSE;
			}

			if ((event->button == 1 || event->button == 2) &&
			    ((event->state & GDK_CONTROL_MASK) != 0 ||
			     (event->state & GDK_SHIFT_MASK) == 0)) {
				view->details->item_selected_on_button_down = gtk_icon_view_path_is_selected (icon_view, path);
				if (view->details->item_selected_on_button_down) {
					call_parent = FALSE;
				} else if ((event->state & GDK_CONTROL_MASK) != 0) {
					GList *selected_items;
					GList *l;

					call_parent = FALSE;
					if ((event->state & GDK_SHIFT_MASK) != 0) {
						GtkTreePath *cursor;
						gtk_icon_view_get_cursor (icon_view, &cursor, NULL);
						if (cursor != NULL) {
							// FIXME gtk_tree_selection_select_range (selection, cursor, path);
						} else {
							gtk_icon_view_select_path (icon_view, path);
						}
					} else {
						gtk_icon_view_select_path (icon_view, path);
					}
					selected_items = gtk_icon_view_get_selected_items (icon_view);

					/* This unselects everything */
					gtk_icon_view_set_cursor (icon_view, path, NULL, FALSE);

					/* So select it again */
					l = selected_items;
					while (l != NULL) {
						GtkTreePath *p = l->data;
						l = l->next;
						gtk_icon_view_select_path (icon_view, p);
						gtk_tree_path_free (p);
					}
					g_list_free (selected_items);
				}
			}

			if (call_parent) {
				g_signal_handlers_block_by_func (icon_view,
								 item_activated_callback,
								 view);

				icon_view_class->button_press_event (widget, event);

				g_signal_handlers_unblock_by_func (icon_view,
								   item_activated_callback,
								   view);
			} else if (gtk_icon_view_path_is_selected (icon_view, path)) {
				gtk_widget_grab_focus (widget);
			}

			if ((event->button == 1 || event->button == 2) &&
			    event->type == GDK_BUTTON_PRESS) {
				view->details->drag_started = FALSE;
				view->details->drag_button = event->button;
				view->details->drag_x = event->x;
				view->details->drag_y = event->y;
			}

			if (event->button == 3) {
				do_popup_menu (widget, view, event);
			}
		}

		gtk_tree_path_free (path);
	} else {
		if ((event->button == 1 || event->button == 2)  &&
		    event->type == GDK_BUTTON_PRESS) {
			if (view->details->double_click_path[1]) {
				gtk_tree_path_free (view->details->double_click_path[1]);
			}
			view->details->double_click_path[1] = view->details->double_click_path[0];
			view->details->double_click_path[0] = NULL;
		}
		/* Deselect if people click outside any item. It's OK to
		   let default code run; it won't reselect anything. */
		gtk_icon_view_unselect_all (icon_view);
		icon_view_class->button_press_event (widget, event);

		if (event->button == 3) {
			do_popup_menu (widget, view, event);
		}
	}

	/* We chained to the default handler in this method, so never
	 * let the default handler run */
	return TRUE;
}

static void
stop_drag_check (NautilusIconView *view)
{
	view->details->drag_button = 0;
}

static gboolean
button_release_callback (GtkWidget      *widget,
			 GdkEventButton *event,
			 gpointer        callback_data)
{
	NautilusIconView *view;

	view = NAUTILUS_ICON_VIEW (callback_data);

	if (event->button == view->details->drag_button) {
		stop_drag_check (view);
		if (!view->details->drag_started &&
		    !view->details->ignore_button_release) {
			nautilus_icon_view_did_not_drag (view, event);
		}
	}
	return FALSE;
}

static gboolean
popup_menu_callback (GtkWidget *widget,
		     gpointer   callback_data)
{
 	NautilusIconView *view;

	view = NAUTILUS_ICON_VIEW (callback_data);

	do_popup_menu (widget, view, NULL);

	return TRUE;
}

static void
set_up_pixbuf_size (NautilusIconView *view)
{
	int icon_size;

	/* Make all items the same size. */
	icon_size = nautilus_get_icon_size_for_zoom_level (view->details->zoom_level);
	gtk_icon_view_set_item_width (GTK_ICON_VIEW (view->details->icon_view), icon_size);
}

static void
create_and_set_up_icon_view (NautilusIconView *view)
{
	AtkObject *atk_obj;
	GList *nautilus_columns;
	GList *l;

	view->details->icon_view = GTK_ICON_VIEW (gtk_icon_view_new ());

	g_signal_connect (view->details->icon_view,
			  "selection-changed",
			  G_CALLBACK (selection_changed_callback), view);
	g_signal_connect_object (view->details->icon_view, "button_press_event",
				 G_CALLBACK (button_press_callback), view, 0);
	g_signal_connect_object (view->details->icon_view, "button_release_event",
				 G_CALLBACK (button_release_callback), view, 0);
	g_signal_connect_object (view->details->icon_view, "key_press_event",
				 G_CALLBACK (key_press_callback), view, 0);
	g_signal_connect_object (view->details->icon_view, "popup_menu",
                                 G_CALLBACK (popup_menu_callback), view, 0);
	g_signal_connect_object (view->details->icon_view, "item-activated",
                                 G_CALLBACK (item_activated_callback), view, 0);

	view->details->model = g_object_new (NAUTILUS_TYPE_LIST_MODEL, NULL);
	gtk_icon_view_set_model (view->details->icon_view, GTK_TREE_MODEL (view->details->model));

	gtk_icon_view_set_selection_mode (view->details->icon_view, GTK_SELECTION_MULTIPLE);

	nautilus_columns = nautilus_get_all_columns ();

	view->details->pixbuf_cell = gtk_cell_renderer_pixbuf_new ();
	g_object_set (view->details->pixbuf_cell,
		      "xalign", 0.5,
		      "yalign", 1.0,
		      NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (view->details->icon_view),
				    view->details->pixbuf_cell,
				    FALSE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (view->details->icon_view),
					view->details->pixbuf_cell,
					"pixbuf", nautilus_list_model_get_column_id_from_zoom_level (view->details->zoom_level),
					NULL);

	for (l = nautilus_columns; l != NULL; l = l->next) {
		NautilusColumn *nautilus_column;
		int column_num;
		char *name;
		char *label;
		float xalign;
		GtkSortType sort_order;

		nautilus_column = NAUTILUS_COLUMN (l->data);

		g_object_get (nautilus_column,
			      "name", &name,
			      "label", &label,
			      "xalign", &xalign,
			      "default-sort-order", &sort_order,
			      NULL);

		column_num = nautilus_list_model_add_column (view->details->model,
							     nautilus_column);

		if (strcmp (name, "name") == 0) {
			view->details->text_column_num = column_num;
			update_text_cell (view);
		}

		g_free (name);
		g_free (label);
	}
	nautilus_column_list_free (nautilus_columns);

	gtk_widget_show (GTK_WIDGET (view->details->icon_view));
	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (view->details->icon_view));

        atk_obj = gtk_widget_get_accessible (GTK_WIDGET (view->details->icon_view));
        atk_object_set_name (atk_obj, _("List View"));
}

static void
nautilus_icon_view_add_file (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	NautilusListModel *model;

	model = NAUTILUS_ICON_VIEW (view)->details->model;
	nautilus_list_model_add_file (model, file, directory);
}

static NautilusZoomLevel
get_default_zoom_level (void) {
	NautilusZoomLevel default_zoom_level;

	default_zoom_level = g_settings_get_enum (nautilus_icon_view_preferences,
						  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL);

	return CLAMP (default_zoom_level, NAUTILUS_ZOOM_LEVEL_SMALLEST, NAUTILUS_ZOOM_LEVEL_LARGEST);
}

static void
nautilus_icon_view_set_zoom_level (NautilusIconView *view,
				   NautilusZoomLevel new_level,
				   gboolean always_emit)
{
	int column;

	g_return_if_fail (NAUTILUS_IS_ICON_VIEW (view));
	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	if (view->details->zoom_level == new_level) {
		if (always_emit) {
			g_signal_emit_by_name (NAUTILUS_VIEW (view), "zoom_level_changed");
		}
		return;
	}

	view->details->zoom_level = new_level;
	g_signal_emit_by_name (NAUTILUS_VIEW(view), "zoom_level_changed");

	/* Select correctly scaled icons. */
	column = nautilus_list_model_get_column_id_from_zoom_level (new_level);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (view->details->icon_view),
					view->details->pixbuf_cell,
					"pixbuf", column,
					NULL);

	nautilus_view_update_menus (NAUTILUS_VIEW (view));

	set_up_pixbuf_size (view);
}

static void
nautilus_icon_view_reset_to_defaults (NautilusView *view)
{
	nautilus_icon_view_set_zoom_level (NAUTILUS_ICON_VIEW (view), get_default_zoom_level (), FALSE);
}

static void
nautilus_icon_view_begin_loading (NautilusView *view)
{
	/* update sort */
}

static void
nautilus_icon_view_clear (NautilusView *view)
{
	NautilusIconView *icon_view;

	icon_view = NAUTILUS_ICON_VIEW (view);

	if (icon_view->details->model != NULL) {
		nautilus_list_model_clear (icon_view->details->model);
	}
}

static void
nautilus_icon_view_file_changed (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	NautilusIconView *listview;

	listview = NAUTILUS_ICON_VIEW (view);

	nautilus_list_model_file_changed (listview->details->model, file, directory);
}

static char *
nautilus_icon_view_get_backing_uri (NautilusView *view)
{
	NautilusListModel *list_model;
	NautilusFile *file;
	GtkIconView *icon_view;
	GtkTreePath *path;
	GList *paths;
	guint length;
	char *uri = NULL;

	g_return_val_if_fail (NAUTILUS_IS_ICON_VIEW (view), NULL);

	list_model = NAUTILUS_ICON_VIEW (view)->details->model;
	icon_view = NAUTILUS_ICON_VIEW (view)->details->icon_view;

	g_assert (list_model);

	/* We currently handle three common cases here:
	 * (a) if the selection contains non-filesystem items (i.e., the
	 *     "(Empty)" label), we return the uri of the parent.
	 * (b) if the selection consists of exactly one _expanded_ directory, we
	 *     return its URI.
	 * (c) if the selection consists of either exactly one item which is not
	 *     an expanded directory) or multiple items in the same directory,
	 *     we return the URI of the common parent.
	 */

	paths = gtk_icon_view_get_selected_items (icon_view);
	length = g_list_length (paths);

	if (length > 0) {
		path = (GtkTreePath *) paths->data;

		file = nautilus_list_model_file_for_path (list_model, path);
		g_assert (file != NULL);
		uri = nautilus_file_get_parent_uri (file);
		nautilus_file_unref (file);
	}

	g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);

	if (uri != NULL) {
		return uri;
	}

	return NAUTILUS_VIEW_CLASS (nautilus_icon_view_parent_class)->get_backing_uri (view);
}

static GList *
nautilus_icon_view_get_selection_for_file_transfer (NautilusView *view)
{
	GList *paths;
	GList *files = NULL;
	GList *l;

	paths = gtk_icon_view_get_selected_items (NAUTILUS_ICON_VIEW (view)->details->icon_view);
	for (l = paths; l != NULL; l = l->next) {
		GtkTreePath *path = l->data;
		NautilusFile *file;

		file = nautilus_list_model_file_for_path (NAUTILUS_ICON_VIEW (view)->details->model, path);

		if (file != NULL) {
			files = g_list_prepend (files, file);
		}
	}

	g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);

	return g_list_reverse (files);
}

static gboolean
nautilus_icon_view_is_empty (NautilusView *view)
{
	return nautilus_list_model_is_empty (NAUTILUS_ICON_VIEW (view)->details->model);
}

static void
nautilus_icon_view_end_file_changes (NautilusView *view)
{

}

static void
nautilus_icon_view_remove_file (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	GtkTreePath *path;
	GtkTreePath *file_path;
	GtkTreeIter iter;
	GtkTreeIter temp_iter;
	GtkTreeRowReference* row_reference;
	NautilusIconView *icon_view;
	GtkTreeModel* tree_model;

	path = NULL;
	row_reference = NULL;
	icon_view = NAUTILUS_ICON_VIEW (view);
	tree_model = GTK_TREE_MODEL(icon_view->details->model);

	if (nautilus_list_model_get_tree_iter_from_file (icon_view->details->model, file, directory, &iter)) {
		gboolean is_selected;

		file_path = gtk_tree_model_get_path (tree_model, &iter);
		is_selected = gtk_icon_view_path_is_selected (NAUTILUS_ICON_VIEW (view)->details->icon_view, path);
		if (is_selected) {
			/* get reference for next element in the list view. If the element to be deleted is the
			 * last one, get reference to previous element. If there is only one element in view
			 * no need to select anything.
			 */
			temp_iter = iter;

			if (gtk_tree_model_iter_next (tree_model, &iter)) {
				path = gtk_tree_model_get_path (tree_model, &iter);
				row_reference = gtk_tree_row_reference_new (tree_model, path);
			} else {
				path = gtk_tree_model_get_path (tree_model, &temp_iter);
				if (gtk_tree_path_prev (path)) {
					row_reference = gtk_tree_row_reference_new (tree_model, path);
				}
			}
			gtk_tree_path_free (path);
		}

		gtk_tree_path_free (file_path);

		nautilus_list_model_remove_file (icon_view->details->model, file, directory);

		if (gtk_tree_row_reference_valid (row_reference)) {
			if (icon_view->details->new_selection_path) {
				gtk_tree_path_free (icon_view->details->new_selection_path);
			}
			icon_view->details->new_selection_path = gtk_tree_row_reference_get_path (row_reference);
		}

		if (row_reference) {
			gtk_tree_row_reference_free (row_reference);
		}
	}
}

static void
nautilus_icon_view_set_selection (NautilusView *view, GList *selection)
{
	NautilusIconView *icon_view;
	GList *node;
	GList *iters, *l;
	NautilusFile *file;

	icon_view = NAUTILUS_ICON_VIEW (view);

	g_signal_handlers_block_by_func (icon_view, selection_changed_callback, view);

	gtk_icon_view_unselect_all (icon_view->details->icon_view);
	for (node = selection; node != NULL; node = node->next) {
		file = node->data;
		iters = nautilus_list_model_get_all_iters_for_file (icon_view->details->model, file);

		for (l = iters; l != NULL; l = l->next) {
			GtkTreeIter *iter = l->data;
			GtkTreePath *path;
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (icon_view->details->model), iter);
			gtk_icon_view_select_path (icon_view->details->icon_view, path);
			gtk_tree_path_free (path);
		}
		g_list_free_full (iters, g_free);
	}

	g_signal_handlers_unblock_by_func (icon_view, selection_changed_callback, view);
	nautilus_view_notify_selection_changed (view);
}

static void
nautilus_icon_view_invert_selection (NautilusView *view)
{
	NautilusIconView *icon_view;
	GList *node;
	GList *selection = NULL;

	icon_view = NAUTILUS_ICON_VIEW (view);

	g_signal_handlers_block_by_func (icon_view, selection_changed_callback, view);

	selection = nautilus_icon_view_get_selection (view);
	gtk_icon_view_select_all (icon_view->details->icon_view);

	for (node = selection; node != NULL; node = node->next) {
		NautilusFile *file = node->data;
		GList *iters, *l;

		iters = nautilus_list_model_get_all_iters_for_file (icon_view->details->model, file);

		for (l = iters; l != NULL; l = l->next) {
			GtkTreeIter *iter = l->data;
			GtkTreePath *path;
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (icon_view->details->model), iter);
			gtk_icon_view_unselect_path (icon_view->details->icon_view, path);
			gtk_tree_path_free (path);
		}
		g_list_free_full (iters, g_free);
	}

	g_list_free (selection);
	/* FIXME: leaking files? */

	g_signal_handlers_unblock_by_func (icon_view, selection_changed_callback, view);
	nautilus_view_notify_selection_changed (view);
}

static void
nautilus_icon_view_select_all (NautilusView *view)
{
	gtk_icon_view_select_all (NAUTILUS_ICON_VIEW (view)->details->icon_view);
}

static void
nautilus_icon_view_select_first (NautilusView *view)
{
	GtkTreePath *path;
	GtkTreeIter  iter;

	if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (NAUTILUS_ICON_VIEW (view)->details->model), &iter)) {
		return;
	}
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (NAUTILUS_ICON_VIEW (view)->details->model), &iter);
	gtk_icon_view_select_path (GTK_ICON_VIEW (NAUTILUS_ICON_VIEW (view)->details->icon_view), path);
	gtk_tree_path_free (path);
}

static void
nautilus_icon_view_merge_menus (NautilusView *view)
{
	NautilusIconView *icon_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;

	icon_view = NAUTILUS_ICON_VIEW (view);

	NAUTILUS_VIEW_CLASS (nautilus_icon_view_parent_class)->merge_menus (view);

	ui_manager = nautilus_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("IconViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	icon_view->details->icon_action_group = action_group;

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	icon_view->details->icon_merge_id =
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/gnome/nautilus/nautilus-icon-view-ui.xml", NULL);

	icon_view->details->menus_ready = TRUE;
}

static void
nautilus_icon_view_unmerge_menus (NautilusView *view)
{
	NautilusIconView *icon_view;
	GtkUIManager *ui_manager;

	icon_view = NAUTILUS_ICON_VIEW (view);

	NAUTILUS_VIEW_CLASS (nautilus_icon_view_parent_class)->unmerge_menus (view);

	ui_manager = nautilus_view_get_ui_manager (view);
	if (ui_manager != NULL) {
		nautilus_ui_unmerge_ui (ui_manager,
					&icon_view->details->icon_merge_id,
					&icon_view->details->icon_action_group);
	}
}

static void
nautilus_icon_view_update_menus (NautilusView *view)
{
	NautilusIconView *icon_view;

        icon_view = NAUTILUS_ICON_VIEW (view);

	/* don't update if the menus aren't ready */
	if (!icon_view->details->menus_ready) {
		return;
	}

	NAUTILUS_VIEW_CLASS (nautilus_icon_view_parent_class)->update_menus (view);
}

static void
nautilus_icon_view_bump_zoom_level (NautilusView *view, int zoom_increment)
{
	NautilusIconView *icon_view;
	gint new_level;

	g_return_if_fail (NAUTILUS_IS_ICON_VIEW (view));

	icon_view = NAUTILUS_ICON_VIEW (view);
	new_level = icon_view->details->zoom_level + zoom_increment;

	if (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
	    new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST) {
		nautilus_icon_view_set_zoom_level (icon_view, new_level, FALSE);
	}
}

static NautilusZoomLevel
nautilus_icon_view_get_zoom_level (NautilusView *view)
{
	NautilusIconView *icon_view;

	g_return_val_if_fail (NAUTILUS_IS_ICON_VIEW (view), NAUTILUS_ZOOM_LEVEL_STANDARD);

	icon_view = NAUTILUS_ICON_VIEW (view);

	return icon_view->details->zoom_level;
}

static void
nautilus_icon_view_zoom_to_level (NautilusView *view,
				  NautilusZoomLevel zoom_level)
{
	NautilusIconView *icon_view;

	g_return_if_fail (NAUTILUS_IS_ICON_VIEW (view));

	icon_view = NAUTILUS_ICON_VIEW (view);

	nautilus_icon_view_set_zoom_level (icon_view, zoom_level, FALSE);
}

static void
nautilus_icon_view_restore_default_zoom_level (NautilusView *view)
{
	NautilusIconView *icon_view;

	g_return_if_fail (NAUTILUS_IS_ICON_VIEW (view));

	icon_view = NAUTILUS_ICON_VIEW (view);

	nautilus_icon_view_set_zoom_level (icon_view, get_default_zoom_level (), FALSE);
}

static gboolean
nautilus_icon_view_can_zoom_in (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_VIEW (view), FALSE);

	return NAUTILUS_ICON_VIEW (view)->details->zoom_level	< NAUTILUS_ZOOM_LEVEL_LARGEST;
}

static gboolean
nautilus_icon_view_can_zoom_out (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_VIEW (view), FALSE);

	return NAUTILUS_ICON_VIEW (view)->details->zoom_level > NAUTILUS_ZOOM_LEVEL_SMALLEST;
}

static void
nautilus_icon_view_start_renaming_file (NautilusView *view,
					NautilusFile *file,
					gboolean select_all)
{
	NautilusIconView *icon_view;
	GtkTreeIter iter;
	GtkTreePath *path;
	gint start_offset, end_offset;

	icon_view = NAUTILUS_ICON_VIEW (view);

	/* Select all if we are in renaming mode already */
	if (icon_view->details->text_column_num && icon_view->details->editable_widget) {
		gtk_editable_select_region (GTK_EDITABLE (icon_view->details->editable_widget),
					    0,
					    -1);
		return;
	}

	if (!nautilus_list_model_get_first_iter_for_file (icon_view->details->model, file, &iter)) {
		return;
	}

	/* call parent class to make sure the right icon is selected */
	NAUTILUS_VIEW_CLASS (nautilus_icon_view_parent_class)->start_renaming_file (view, file, select_all);

	/* Freeze updates to the view to prevent losing rename focus when the tree view updates */
	nautilus_view_freeze_updates (NAUTILUS_VIEW (view));

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (icon_view->details->model), &iter);

	/* Make filename-cells editable. */
	g_object_set (G_OBJECT (icon_view->details->text_cell),
		      "editable", TRUE,
		      NULL);

	gtk_icon_view_scroll_to_path (icon_view->details->icon_view,
				      path,
				      TRUE, 0.0, 0.0);
	gtk_icon_view_set_cursor (icon_view->details->icon_view,
				  path,
				  GTK_CELL_RENDERER (icon_view->details->text_cell),
				  TRUE);

	/* set cursor also triggers editing-started, where we save the editable widget */
	if (icon_view->details->editable_widget != NULL) {
		eel_filename_get_rename_region (icon_view->details->original_name,
						&start_offset, &end_offset);

		gtk_editable_select_region (GTK_EDITABLE (icon_view->details->editable_widget),
					    start_offset, end_offset);
	}

	gtk_tree_path_free (path);
}

static int
nautilus_icon_view_compare_files (NautilusView *view, NautilusFile *file1, NautilusFile *file2)
{
	NautilusIconView *icon_view;

	icon_view = NAUTILUS_ICON_VIEW (view);
	return nautilus_list_model_compare_func (icon_view->details->model, file1, file2);
}

static gboolean
nautilus_icon_view_using_manual_layout (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_ICON_VIEW (view), FALSE);

	return FALSE;
}

static void
nautilus_icon_view_click_policy_changed (NautilusView *directory_view)
{
	GdkWindow *win;
	GdkDisplay *display;
	NautilusIconView *view;
	GtkTreeIter iter;
	GtkIconView *icon_view;

	view = NAUTILUS_ICON_VIEW (directory_view);

	/* ensure that we unset the hand cursor and refresh underlined rows */
	if (get_click_policy () == NAUTILUS_CLICK_POLICY_DOUBLE) {
		if (view->details->hover_path != NULL) {
			if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
						     &iter, view->details->hover_path)) {
				gtk_tree_model_row_changed (GTK_TREE_MODEL (view->details->model),
							    view->details->hover_path, &iter);
			}

			gtk_tree_path_free (view->details->hover_path);
			view->details->hover_path = NULL;
		}

		icon_view = view->details->icon_view;
		if (gtk_widget_get_realized (GTK_WIDGET (icon_view))) {
			win = gtk_widget_get_window (GTK_WIDGET (icon_view));
			gdk_window_set_cursor (win, NULL);

			display = gtk_widget_get_display (GTK_WIDGET (view));
			if (display != NULL) {
				gdk_display_flush (display);
			}
		}

		g_clear_object (&hand_cursor);
	} else if (get_click_policy () == NAUTILUS_CLICK_POLICY_SINGLE) {
		if (hand_cursor == NULL) {
			hand_cursor = gdk_cursor_new (GDK_HAND2);
		}
	}
}

static void
nautilus_icon_view_sort_directories_first_changed (NautilusView *view)
{
	NautilusIconView *icon_view;

	icon_view = NAUTILUS_ICON_VIEW (view);

	nautilus_list_model_set_should_sort_directories_first (icon_view->details->model,
							       nautilus_view_should_sort_directories_first (view));
}

static void
nautilus_icon_view_dispose (GObject *object)
{
	NautilusIconView *icon_view;

	icon_view = NAUTILUS_ICON_VIEW (object);

	if (icon_view->details->model) {
		g_object_unref (icon_view->details->model);
		icon_view->details->model = NULL;
	}

	G_OBJECT_CLASS (nautilus_icon_view_parent_class)->dispose (object);
}

static void
nautilus_icon_view_finalize (GObject *object)
{
	NautilusIconView *icon_view;

	icon_view = NAUTILUS_ICON_VIEW (object);

	g_free (icon_view->details->original_name);
	icon_view->details->original_name = NULL;

	if (icon_view->details->new_selection_path) {
		gtk_tree_path_free (icon_view->details->new_selection_path);
	}

	if (icon_view->details->hover_path != NULL) {
		gtk_tree_path_free (icon_view->details->hover_path);
	}

	G_OBJECT_CLASS (nautilus_icon_view_parent_class)->finalize (object);
}

static char *
nautilus_icon_view_get_first_visible_file (NautilusView *view)
{
	NautilusFile *file;
	GtkTreePath *path;
	GtkTreeIter iter;
	NautilusIconView *icon_view;

	icon_view = NAUTILUS_ICON_VIEW (view);

	path = gtk_icon_view_get_path_at_pos (icon_view->details->icon_view, 0, 0);
	if (path != NULL) {
		gtk_tree_model_get_iter (GTK_TREE_MODEL (icon_view->details->model),
					 &iter, path);

		gtk_tree_path_free (path);

		gtk_tree_model_get (GTK_TREE_MODEL (icon_view->details->model),
				    &iter,
				    NAUTILUS_LIST_MODEL_FILE_COLUMN, &file,
				    -1);
		if (file) {
			char *uri;

			uri = nautilus_file_get_uri (file);

			nautilus_file_unref (file);

			return uri;
		}
	}

	return NULL;
}

static void
nautilus_icon_view_scroll_to_file (NautilusIconView *view,
				   NautilusFile *file)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!nautilus_list_model_get_first_iter_for_file (view->details->model, file, &iter)) {
		return;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->details->model), &iter);

	gtk_icon_view_scroll_to_path (view->details->icon_view,
				      path, TRUE, 0.0, 0.0);

	gtk_tree_path_free (path);
}

static void
icon_view_scroll_to_file (NautilusView *view,
			  const char *uri)
{
	NautilusFile *file;

	if (uri != NULL) {
		/* Only if existing, since we don't want to add the file to
		   the directory if it has been removed since then */
		file = nautilus_file_get_existing_by_uri (uri);
		if (file != NULL) {
			nautilus_icon_view_scroll_to_file (NAUTILUS_ICON_VIEW (view), file);
			nautilus_file_unref (file);
		}
	}
}

static void
icon_view_notify_clipboard_info (NautilusClipboardMonitor *monitor,
                                 NautilusClipboardInfo *info,
                                 NautilusIconView *view)
{
	/* this could be called as a result of _end_loading() being
	 * called after _dispose(), where the model is cleared.
	 */
	if (view->details->model == NULL) {
		return;
	}

	if (info != NULL && info->cut) {
		nautilus_list_model_set_highlight_for_files (view->details->model, info->files);
	} else {
		nautilus_list_model_set_highlight_for_files (view->details->model, NULL);
	}
}

static void
nautilus_icon_view_end_loading (NautilusView *view,
				gboolean all_files_seen)
{
	NautilusClipboardMonitor *monitor;
	NautilusClipboardInfo *info;

	monitor = nautilus_clipboard_monitor_get ();
	info = nautilus_clipboard_monitor_get_clipboard_info (monitor);

	icon_view_notify_clipboard_info (monitor, info, NAUTILUS_ICON_VIEW (view));
}

static const char *
nautilus_icon_view_get_id (NautilusView *view)
{
	return NAUTILUS_ICON_VIEW_ID;
}

static void
nautilus_icon_view_class_init (NautilusIconViewClass *class)
{
	NautilusViewClass *nautilus_view_class;

	nautilus_view_class = NAUTILUS_VIEW_CLASS (class);

	G_OBJECT_CLASS (class)->dispose = nautilus_icon_view_dispose;
	G_OBJECT_CLASS (class)->finalize = nautilus_icon_view_finalize;

	nautilus_view_class->add_file = nautilus_icon_view_add_file;
	nautilus_view_class->begin_loading = nautilus_icon_view_begin_loading;
	nautilus_view_class->end_loading = nautilus_icon_view_end_loading;
	nautilus_view_class->bump_zoom_level = nautilus_icon_view_bump_zoom_level;
	nautilus_view_class->can_zoom_in = nautilus_icon_view_can_zoom_in;
	nautilus_view_class->can_zoom_out = nautilus_icon_view_can_zoom_out;
        nautilus_view_class->click_policy_changed = nautilus_icon_view_click_policy_changed;
	nautilus_view_class->clear = nautilus_icon_view_clear;
	nautilus_view_class->file_changed = nautilus_icon_view_file_changed;
	nautilus_view_class->get_backing_uri = nautilus_icon_view_get_backing_uri;
	nautilus_view_class->get_selection = nautilus_icon_view_get_selection;
	nautilus_view_class->get_selection_for_file_transfer = nautilus_icon_view_get_selection_for_file_transfer;
	nautilus_view_class->is_empty = nautilus_icon_view_is_empty;
	nautilus_view_class->remove_file = nautilus_icon_view_remove_file;
	nautilus_view_class->merge_menus = nautilus_icon_view_merge_menus;
	nautilus_view_class->unmerge_menus = nautilus_icon_view_unmerge_menus;
	nautilus_view_class->update_menus = nautilus_icon_view_update_menus;
	nautilus_view_class->reset_to_defaults = nautilus_icon_view_reset_to_defaults;
	nautilus_view_class->restore_default_zoom_level = nautilus_icon_view_restore_default_zoom_level;
	nautilus_view_class->reveal_selection = nautilus_icon_view_reveal_selection;
	nautilus_view_class->select_all = nautilus_icon_view_select_all;
	nautilus_view_class->select_first = nautilus_icon_view_select_first;
	nautilus_view_class->set_selection = nautilus_icon_view_set_selection;
	nautilus_view_class->invert_selection = nautilus_icon_view_invert_selection;
	nautilus_view_class->compare_files = nautilus_icon_view_compare_files;
	nautilus_view_class->sort_directories_first_changed = nautilus_icon_view_sort_directories_first_changed;
	nautilus_view_class->start_renaming_file = nautilus_icon_view_start_renaming_file;
	nautilus_view_class->get_zoom_level = nautilus_icon_view_get_zoom_level;
	nautilus_view_class->zoom_to_level = nautilus_icon_view_zoom_to_level;
	nautilus_view_class->end_file_changes = nautilus_icon_view_end_file_changes;
	nautilus_view_class->using_manual_layout = nautilus_icon_view_using_manual_layout;
	nautilus_view_class->get_view_id = nautilus_icon_view_get_id;
	nautilus_view_class->get_first_visible_file = nautilus_icon_view_get_first_visible_file;
	nautilus_view_class->scroll_to_file = icon_view_scroll_to_file;

	g_type_class_add_private (class, sizeof (NautilusIconViewDetails));
}

static void
nautilus_icon_view_init (NautilusIconView *icon_view)
{
	icon_view->details = G_TYPE_INSTANCE_GET_PRIVATE (icon_view,
							  NAUTILUS_TYPE_ICON_VIEW,
							  NautilusIconViewDetails);

	/* ensure that the zoom level is always set before settings up the tree view columns */
	icon_view->details->zoom_level = get_default_zoom_level ();

	create_and_set_up_icon_view (icon_view);

	nautilus_icon_view_set_zoom_level (icon_view, get_default_zoom_level (), TRUE);
}

static NautilusView *
nautilus_icon_view_create (NautilusWindowSlot *slot)
{
	NautilusIconView *view;

	view = g_object_new (NAUTILUS_TYPE_ICON_VIEW,
			     "window-slot", slot,
			     NULL);
	return NAUTILUS_VIEW (view);
}

static gboolean
nautilus_icon_view_supports_uri (const char *uri,
				 GFileType file_type,
				 const char *mime_type)
{
	if (file_type == G_FILE_TYPE_DIRECTORY) {
		return TRUE;
	}
	if (strcmp (mime_type, NAUTILUS_SAVED_SEARCH_MIMETYPE) == 0){
		return TRUE;
	}
	if (g_str_has_prefix (uri, "trash:")) {
		return TRUE;
	}
	if (g_str_has_prefix (uri, EEL_SEARCH_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NautilusViewInfo nautilus_icon_view = {
	NAUTILUS_ICON_VIEW_ID,
	/* translators: this is used in the view selection dropdown
	 * of navigation windows and in the preferences dialog */
	N_("Icon View"),
	/* translators: this is used in the view menu */
	N_("_Icon"),
	N_("The icon view encountered an error."),
	N_("The icon view encountered an error while starting up."),
	N_("Display this location with the icon view."),
	nautilus_icon_view_create,
	nautilus_icon_view_supports_uri
};

void
nautilus_icon_view_register (void)
{
	nautilus_icon_view.view_combo_label = _(nautilus_icon_view.view_combo_label);
	nautilus_icon_view.view_menu_label_with_mnemonic = _(nautilus_icon_view.view_menu_label_with_mnemonic);
	nautilus_icon_view.error_label = _(nautilus_icon_view.error_label);
	nautilus_icon_view.startup_error_label = _(nautilus_icon_view.startup_error_label);
	nautilus_icon_view.display_location_label = _(nautilus_icon_view.display_location_label);

	nautilus_view_factory_register (&nautilus_icon_view);
}

GtkIconView *
nautilus_icon_view_get_icon_view (NautilusIconView *icon_view)
{
	return icon_view->details->icon_view;
}
