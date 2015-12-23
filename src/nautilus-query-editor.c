/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>
#include "nautilus-query-editor.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <eel/eel-glib-extensions.h>
#include <libnautilus-private/nautilus-file-utilities.h>

typedef enum {
	NAUTILUS_QUERY_EDITOR_ROW_TYPE,
	
	NAUTILUS_QUERY_EDITOR_ROW_LAST
} NautilusQueryEditorRowType;

typedef struct {
	NautilusQueryEditorRowType type;
	NautilusQueryEditor *editor;
	GtkWidget *toolbar;
	GtkWidget *hbox;
	GtkWidget *combo;

	GtkWidget *type_widget;
} NautilusQueryEditorRow;


typedef struct {
	const char *name;
	GtkWidget * (*create_widgets)      (NautilusQueryEditorRow *row);
	void        (*add_to_query)        (NautilusQueryEditorRow *row,
					    NautilusQuery          *query);
	void        (*add_rows_from_query) (NautilusQueryEditor *editor,
					    NautilusQuery *query);
} NautilusQueryEditorRowOps;

struct NautilusQueryEditorDetails {
	GtkWidget *entry;
	gboolean change_frozen;

	GtkWidget *search_current_button;
	GtkWidget *search_all_button;
        GFile *current_location;

	GList *rows;

	NautilusQuery *query;
};

enum {
	ACTIVATED,
	CHANGED,
	CANCEL,
	LAST_SIGNAL
}; 

static guint signals[LAST_SIGNAL];

static void entry_activate_cb (GtkWidget *entry, NautilusQueryEditor *editor);
static void entry_changed_cb  (GtkWidget *entry, NautilusQueryEditor *editor);
static void nautilus_query_editor_changed_force (NautilusQueryEditor *editor,
						 gboolean             force);
static void nautilus_query_editor_changed (NautilusQueryEditor *editor);
static NautilusQueryEditorRow * nautilus_query_editor_add_row (NautilusQueryEditor *editor,
							       NautilusQueryEditorRowType type);

static GtkWidget *type_row_create_widgets      (NautilusQueryEditorRow *row);
static void       type_row_add_to_query        (NautilusQueryEditorRow *row,
					        NautilusQuery          *query);
static void       type_add_rows_from_query     (NautilusQueryEditor    *editor,
					        NautilusQuery          *query);



static NautilusQueryEditorRowOps row_type[] = {
	{ N_("File Type"),
	  type_row_create_widgets,
	  type_row_add_to_query,
	  type_add_rows_from_query
	},
};

G_DEFINE_TYPE (NautilusQueryEditor, nautilus_query_editor, GTK_TYPE_BOX);

gboolean
nautilus_query_editor_handle_event (NautilusQueryEditor *editor,
				    GdkEventKey         *event)
{
	GtkWidget *toplevel;
	GtkWidget *old_focus;
	GdkEvent *new_event;
	gboolean retval;

	/* if we're focused already, no need to handle the event manually */
	if (gtk_widget_has_focus (editor->details->entry)) {
		return FALSE;
	}

	/* never handle these events */
	if (event->keyval == GDK_KEY_slash || event->keyval == GDK_KEY_Delete) {
		return FALSE;
	}

	/* don't activate search for these events */
	if (!gtk_widget_get_visible (GTK_WIDGET (editor)) && event->keyval == GDK_KEY_space) {
		return FALSE;
	}

	/* if it's not printable we don't need it */
	if (!g_unichar_isprint (gdk_keyval_to_unicode (event->keyval))) {
		return FALSE;
	}

	if (!gtk_widget_get_realized (editor->details->entry)) {
		gtk_widget_realize (editor->details->entry);
	}

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (editor));
	if (gtk_widget_is_toplevel (toplevel)) {
		old_focus = gtk_window_get_focus (GTK_WINDOW (toplevel));
	} else {
		old_focus = NULL;
	}

	/* input methods will typically only process events after getting focus */
	gtk_widget_grab_focus (editor->details->entry);

	new_event = gdk_event_copy ((GdkEvent *) event);
	g_object_unref (((GdkEventKey *) new_event)->window);
	((GdkEventKey *) new_event)->window = g_object_ref
		(gtk_widget_get_window (editor->details->entry));
	retval = gtk_widget_event (editor->details->entry, new_event);
	gdk_event_free (new_event);

	if (!retval && old_focus) {
		gtk_widget_grab_focus (old_focus);
	}

	return retval;
}

static void
row_destroy (NautilusQueryEditorRow *row)
{
	gtk_widget_destroy (row->toolbar);
	g_free (row);
}

static void
nautilus_query_editor_dispose (GObject *object)
{
	NautilusQueryEditor *editor;

	editor = NAUTILUS_QUERY_EDITOR (object);

	g_clear_object (&editor->details->query);

	g_list_free_full (editor->details->rows, (GDestroyNotify) row_destroy);
	editor->details->rows = NULL;

	G_OBJECT_CLASS (nautilus_query_editor_parent_class)->dispose (object);
}

static void
nautilus_query_editor_grab_focus (GtkWidget *widget)
{
	NautilusQueryEditor *editor = NAUTILUS_QUERY_EDITOR (widget);

	if (gtk_widget_get_visible (widget) && !gtk_widget_is_focus (editor->details->entry)) {
		/* avoid selecting the entry text */
		gtk_widget_grab_focus (editor->details->entry);
		gtk_editable_set_position (GTK_EDITABLE (editor->details->entry), -1);
	}
}

static void
nautilus_query_editor_class_init (NautilusQueryEditorClass *class)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

	gobject_class = G_OBJECT_CLASS (class);
        gobject_class->dispose = nautilus_query_editor_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->grab_focus = nautilus_query_editor_grab_focus;

	signals[CHANGED] =
		g_signal_new ("changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusQueryEditorClass, changed),
		              NULL, NULL,
		              g_cclosure_marshal_generic,
		              G_TYPE_NONE, 2, NAUTILUS_TYPE_QUERY, G_TYPE_BOOLEAN);

	signals[CANCEL] =
		g_signal_new ("cancel",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (NautilusQueryEditorClass, cancel),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[ACTIVATED] =
		g_signal_new ("activated",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		              G_STRUCT_OFFSET (NautilusQueryEditorClass, activated),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	g_type_class_add_private (class, sizeof (NautilusQueryEditorDetails));
}

GFile *
nautilus_query_editor_get_location (NautilusQueryEditor *editor)
{
	GFile *file = NULL;
        if (editor->details->current_location != NULL)
                file = g_object_ref (editor->details->current_location);
	return file;
}

static void
entry_activate_cb (GtkWidget *entry, NautilusQueryEditor *editor)
{
	g_signal_emit (editor, signals[ACTIVATED], 0);
}

static void
entry_changed_cb (GtkWidget *entry, NautilusQueryEditor *editor)
{
	if (editor->details->change_frozen) {
		return;
	}

	nautilus_query_editor_changed (editor);
}

static void
nautilus_query_editor_on_stop_search (GtkWidget           *entry,
                                      NautilusQueryEditor *editor)
{
	g_signal_emit (editor, signals[CANCEL], 0);
}

/* Type */

static gboolean
type_separator_func (GtkTreeModel      *model,
		     GtkTreeIter       *iter,
		     gpointer           data)
{
	char *text;
	gboolean res;
	
	gtk_tree_model_get (model, iter, 0, &text, -1);

	res = text != NULL && strcmp (text, "---") == 0;
	
	g_free (text);
	return res;
}

struct {
	char *name;
	char *mimetypes[20];
} mime_type_groups[] = {
	{ N_("Documents"),
	  { "application/rtf",
	    "application/msword",
	    "application/vnd.sun.xml.writer",
	    "application/vnd.sun.xml.writer.global",
	    "application/vnd.sun.xml.writer.template",
	    "application/vnd.oasis.opendocument.text",
	    "application/vnd.oasis.opendocument.text-template",
	    "application/x-abiword",
	    "application/x-applix-word",
	    "application/x-mswrite",
	    "application/docbook+xml",
	    "application/x-kword",
	    "application/x-kword-crypt",
	    "application/x-lyx",
	    NULL
	  }
	},
	{ N_("Music"),
	  { "application/ogg",
	    "audio/x-vorbis+ogg",
	    "audio/ac3",
	    "audio/basic",
	    "audio/midi",
	    "audio/x-flac",
	    "audio/mp4",
	    "audio/mpeg",
	    "audio/x-mpeg",
	    "audio/x-ms-asx",
	    "audio/x-pn-realaudio",
	    NULL
	  }
	},
	{ N_("Video"),
	  { "video/mp4",
	    "video/3gpp",
	    "video/mpeg",
	    "video/quicktime",
	    "video/vivo",
	    "video/x-avi",
	    "video/x-mng",
	    "video/x-ms-asf",
	    "video/x-ms-wmv",
	    "video/x-msvideo",
	    "video/x-nsv",
	    "video/x-real-video",
	    NULL
	  }
	},
	{ N_("Picture"),
	  { "application/vnd.oasis.opendocument.image",
	    "application/x-krita",
	    "image/bmp",
	    "image/cgm",
	    "image/gif",
	    "image/jpeg",
	    "image/jpeg2000",
	    "image/png",
	    "image/svg+xml",
	    "image/tiff",
	    "image/x-compressed-xcf",
	    "image/x-pcx",
	    "image/x-photo-cd",
	    "image/x-psd",
	    "image/x-tga",
	    "image/x-xcf",
	    NULL
	  }
	},
	{ N_("Illustration"),
	  { "application/illustrator",
	    "application/vnd.corel-draw",
	    "application/vnd.stardivision.draw",
	    "application/vnd.oasis.opendocument.graphics",
	    "application/x-dia-diagram",
	    "application/x-karbon",
	    "application/x-killustrator",
	    "application/x-kivio",
	    "application/x-kontour",
	    "application/x-wpg",
	    NULL
	  }
	},
	{ N_("Spreadsheet"),
	  { "application/vnd.lotus-1-2-3",
	    "application/vnd.ms-excel",
	    "application/vnd.stardivision.calc",
	    "application/vnd.sun.xml.calc",
	    "application/vnd.oasis.opendocument.spreadsheet",
	    "application/x-applix-spreadsheet",
	    "application/x-gnumeric",
	    "application/x-kspread",
	    "application/x-kspread-crypt",
	    "application/x-quattropro",
	    "application/x-sc",
	    "application/x-siag",
	    NULL
	  }
	},
	{ N_("Presentation"),
	  { "application/vnd.ms-powerpoint",
	    "application/vnd.sun.xml.impress",
	    "application/vnd.oasis.opendocument.presentation",
	    "application/x-magicpoint",
	    "application/x-kpresenter",
	    NULL
	  }
	},
	{ N_("PDF / PostScript"),
	  { "application/pdf",
	    "application/postscript",
	    "application/x-dvi",
	    "image/x-eps",
	    NULL
	  }
	},
	{ N_("Text File"),
	  { "text/plain",
	    NULL
	  }
	}
};

static void
type_add_custom_type (NautilusQueryEditorRow *row,
		      const char *mime_type,
		      const char *description,
		      GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkListStore *store;
	
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (row->type_widget));
	store = GTK_LIST_STORE (model);

	gtk_list_store_append (store, iter);
	gtk_list_store_set (store, iter,
			    0, description,
			    2, mime_type,
			    -1);
}


static void
type_combo_changed (GtkComboBox *combo_box, NautilusQueryEditorRow *row)
{
	GtkTreeIter iter;
	gboolean other;
	GtkTreeModel *model;

	if (!gtk_combo_box_get_active_iter  (GTK_COMBO_BOX (row->type_widget),
					     &iter)) {
		return;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (row->type_widget));
	gtk_tree_model_get (model, &iter, 3, &other, -1);

	if (other) {
		GList *mime_infos, *l;
		GtkWidget *dialog;
		GtkWidget *scrolled, *treeview;
		GtkListStore *store;
		GtkTreeViewColumn *column;
		GtkCellRenderer *renderer;
		GtkWidget *toplevel;
		GtkTreeSelection *selection;

		mime_infos = g_content_types_get_registered ();

		store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
		for (l = mime_infos; l != NULL; l = l->next) {
			GtkTreeIter iter;
			char *mime_type = l->data;
			char *description;

			description = g_content_type_get_description (mime_type);
			if (description == NULL) {
				description = g_strdup (mime_type);
			}
			
			gtk_list_store_append (store, &iter);
			gtk_list_store_set (store, &iter,
					    0, description,
					    1, mime_type,
					    -1);
			
			g_free (mime_type);
			g_free (description);
		}
		g_list_free (mime_infos);
		

		
		toplevel = gtk_widget_get_toplevel (GTK_WIDGET (combo_box));
		dialog = gtk_dialog_new_with_buttons (_("Select type"),
						      GTK_WINDOW (toplevel),
						      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
						      _("_Cancel"), GTK_RESPONSE_CANCEL,
						      _("Select"), GTK_RESPONSE_OK,
						      NULL);
		gtk_window_set_default_size (GTK_WINDOW (dialog), 400, 600);
			
		scrolled = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
						GTK_POLICY_AUTOMATIC,
						GTK_POLICY_AUTOMATIC);

		gtk_widget_show (scrolled);
		gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 0);
		gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), scrolled, TRUE, TRUE, 0);

		treeview = gtk_tree_view_new ();
		gtk_tree_view_set_model (GTK_TREE_VIEW (treeview),
					 GTK_TREE_MODEL (store));
		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), 0,
						      GTK_SORT_ASCENDING);
		
		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
		gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);


		renderer = gtk_cell_renderer_text_new ();
		column = gtk_tree_view_column_new_with_attributes ("Name",
								   renderer,
								   "text",
								   0,
								   NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
		gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);
		
		gtk_widget_show (treeview);
		gtk_container_add (GTK_CONTAINER (scrolled), treeview);

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
			char *mimetype, *description;

			gtk_tree_selection_get_selected (selection, NULL, &iter);
			gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
					    0, &description,
					    1, &mimetype,
					    -1);

			type_add_custom_type (row, mimetype, description, &iter);
			gtk_combo_box_set_active_iter  (GTK_COMBO_BOX (row->type_widget),
							&iter);
		} else {
			gtk_combo_box_set_active (GTK_COMBO_BOX (row->type_widget), 0);
		}

		gtk_widget_destroy (dialog);
	}
	
	nautilus_query_editor_changed (row->editor);
}

static GtkWidget *
type_row_create_widgets (NautilusQueryEditorRow *row)
{
	GtkWidget *combo;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkTreeIter iter;
	int i;

	store = gtk_list_store_new (4, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_BOOLEAN);
	combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);
	
	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
					"text", 0,
					NULL);
	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo),
					      type_separator_func,
					      NULL, NULL);

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, _("Any"), -1);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, "---",  -1);

	for (i = 0; i < G_N_ELEMENTS (mime_type_groups); i++) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter,
				    0, gettext (mime_type_groups[i].name),
				    1, mime_type_groups[i].mimetypes,
				    -1);
	}

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, "---",  -1);
	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, _("Other Type…"), 3, TRUE, -1);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), 0);
	
	g_signal_connect (combo, "changed",
			  G_CALLBACK (type_combo_changed),
			  row);

	gtk_widget_show (combo);
	
	gtk_box_pack_start (GTK_BOX (row->hbox), combo, FALSE, FALSE, 0);
	
	return combo;
}

static void
type_row_add_to_query (NautilusQueryEditorRow *row,
		       NautilusQuery          *query)
{
	GtkTreeIter iter;
	char **mimetypes;
	char *mimetype;
	GtkTreeModel *model;

	if (!gtk_combo_box_get_active_iter  (GTK_COMBO_BOX (row->type_widget),
					     &iter)) {
		return;
	}

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (row->type_widget));
	gtk_tree_model_get (model, &iter, 1, &mimetypes, 2, &mimetype, -1);

	if (mimetypes != NULL) {
		while (*mimetypes != NULL) {
			nautilus_query_add_mime_type (query, *mimetypes);
			mimetypes++;
		}
	}
	if (mimetype) {
		nautilus_query_add_mime_type (query, mimetype);
		g_free (mimetype);
	}
}

static gboolean
all_group_types_in_list (char **group_types, GList *mime_types)
{
	GList *l;
	char **group_type;
	char *mime_type;
	gboolean found;

	group_type = group_types;
	while (*group_type != NULL) {
		found = FALSE;

		for (l = mime_types; l != NULL; l = l->next) {
			mime_type = l->data;

			if (strcmp (mime_type, *group_type) == 0) {
				found = TRUE;
				break;
			}
		}
		
		if (!found) {
			return FALSE;
		}
		group_type++;
	}
	return TRUE;
}

static GList *
remove_group_types_from_list (char **group_types, GList *mime_types)
{
	GList *l, *next;
	char **group_type;
	char *mime_type;

	group_type = group_types;
	while (*group_type != NULL) {
		for (l = mime_types; l != NULL; l = next) {
			mime_type = l->data;
			next = l->next;

			if (strcmp (mime_type, *group_type) == 0) {
				mime_types = g_list_remove_link (mime_types, l);
				g_free (mime_type);
				break;
			}
		}
		
		group_type++;
	}
	return mime_types;
}


static void
type_add_rows_from_query (NautilusQueryEditor    *editor,
			  NautilusQuery          *query)
{
	GList *mime_types;
	char *mime_type;
	const char *desc;
	NautilusQueryEditorRow *row;
	GtkTreeIter iter;
	int i;
	GtkTreeModel *model;
	GList *l;

	mime_types = nautilus_query_get_mime_types (query);

	if (mime_types == NULL) {
		return;
	}
	
	for (i = 0; i < G_N_ELEMENTS (mime_type_groups); i++) {
		if (all_group_types_in_list (mime_type_groups[i].mimetypes,
					     mime_types)) {
			mime_types = remove_group_types_from_list (mime_type_groups[i].mimetypes,
								   mime_types);

			row = nautilus_query_editor_add_row (editor,
							     NAUTILUS_QUERY_EDITOR_ROW_TYPE);

			model = gtk_combo_box_get_model (GTK_COMBO_BOX (row->type_widget));

			gtk_tree_model_iter_nth_child (model, &iter, NULL, i + 2);
			gtk_combo_box_set_active_iter  (GTK_COMBO_BOX (row->type_widget),
							&iter);
		}
	}

	for (l = mime_types; l != NULL; l = l->next) {
		mime_type = l->data;

		desc = g_content_type_get_description (mime_type);
		if (desc == NULL) {
			desc = mime_type;
		}

		row = nautilus_query_editor_add_row (editor,
						     NAUTILUS_QUERY_EDITOR_ROW_TYPE);
		
		type_add_custom_type (row, mime_type, desc, &iter);
		gtk_combo_box_set_active_iter  (GTK_COMBO_BOX (row->type_widget),
						&iter);
	}

	g_list_free_full (mime_types, g_free);
}

/* End of row types */

static NautilusQueryEditorRowType
get_next_free_type (NautilusQueryEditor *editor)
{
	NautilusQueryEditorRow *row;
	NautilusQueryEditorRowType type;
	gboolean found;
	GList *l;

	
	for (type = 0; type < NAUTILUS_QUERY_EDITOR_ROW_LAST; type++) {
		found = FALSE;
		for (l = editor->details->rows; l != NULL; l = l->next) {
			row = l->data;
			if (row->type == type) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			return type;
		}
	}
	return NAUTILUS_QUERY_EDITOR_ROW_TYPE;
}

static void
remove_row_cb (GtkButton *clicked_button, NautilusQueryEditorRow *row)
{
	NautilusQueryEditor *editor;

	editor = row->editor;
	editor->details->rows = g_list_remove (editor->details->rows, row);
	row_destroy (row);

	nautilus_query_editor_changed (editor);
}

static void
create_type_widgets (NautilusQueryEditorRow *row)
{
	row->type_widget = row_type[row->type].create_widgets (row);
}

static void
row_type_combo_changed_cb (GtkComboBox *combo_box, NautilusQueryEditorRow *row)
{
	NautilusQueryEditorRowType type;

	type = gtk_combo_box_get_active (combo_box);

	if (type == row->type) {
		return;
	}

	if (row->type_widget != NULL) {
		gtk_widget_destroy (row->type_widget);
		row->type_widget = NULL;
	}

	row->type = type;
	
	create_type_widgets (row);

	nautilus_query_editor_changed (row->editor);
}

static NautilusQueryEditorRow *
nautilus_query_editor_add_row (NautilusQueryEditor *editor,
			       NautilusQueryEditorRowType type)
{
	GtkWidget *hbox, *button, *image, *combo;
	GtkToolItem *item;
	NautilusQueryEditorRow *row;
	int i;

	row = g_new0 (NautilusQueryEditorRow, 1);
	row->editor = editor;
	row->type = type;

	/* create the toolbar and the box container for its contents */
	row->toolbar = gtk_toolbar_new ();
	gtk_style_context_add_class (gtk_widget_get_style_context (row->toolbar),
				     "search-bar");
	gtk_box_pack_start (GTK_BOX (editor), row->toolbar, TRUE, TRUE, 0);

	item = gtk_tool_item_new ();
	gtk_tool_item_set_expand (item, TRUE);
	gtk_toolbar_insert (GTK_TOOLBAR (row->toolbar), item, -1);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add (GTK_CONTAINER (item), hbox);
	row->hbox = hbox;

	/* create the criterion selector combobox */
	combo = gtk_combo_box_text_new ();
	row->combo = combo;
	for (i = 0; i < NAUTILUS_QUERY_EDITOR_ROW_LAST; i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo), gettext (row_type[i].name));
	}
	gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);

	gtk_combo_box_set_active (GTK_COMBO_BOX (combo), row->type);

	editor->details->rows = g_list_append (editor->details->rows, row);

	g_signal_connect (combo, "changed",
			  G_CALLBACK (row_type_combo_changed_cb), row);
	
	create_type_widgets (row);

	/* create the remove row button */
	button = gtk_button_new ();
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
				     GTK_STYLE_CLASS_RAISED);
	gtk_widget_set_tooltip_text (button,
				     _("Remove this criterion from the search"));
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	image = gtk_image_new_from_icon_name ("window-close-symbolic",
					      GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (button), image);

	g_signal_connect (button, "clicked",
			  G_CALLBACK (remove_row_cb), row);

	/* show everything */
	gtk_widget_show_all (row->toolbar);

	return row;
}

static void
add_new_row_cb (GtkButton *clicked_button, NautilusQueryEditor *editor)
{
	nautilus_query_editor_add_row (editor, get_next_free_type (editor));
	nautilus_query_editor_changed (editor);
}

static void
nautilus_query_editor_init (NautilusQueryEditor *editor)
{
	editor->details = G_TYPE_INSTANCE_GET_PRIVATE (editor, NAUTILUS_TYPE_QUERY_EDITOR,
						       NautilusQueryEditorDetails);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (editor), GTK_ORIENTATION_VERTICAL);
}

static void
on_location_button_toggled (GtkToggleButton     *button,
		       NautilusQueryEditor *editor)
{
	nautilus_query_editor_changed (editor);
}

static gboolean
entry_key_press_event_cb (GtkWidget           *widget,
			  GdkEventKey         *event,
			  NautilusQueryEditor *editor)
{
	if (event->keyval == GDK_KEY_Down) {
		gtk_widget_grab_focus (gtk_widget_get_toplevel (GTK_WIDGET (widget)));
	}
	return FALSE;
}

static void
setup_widgets (NautilusQueryEditor *editor)
{
	GtkToolItem *item;
	GtkWidget *toolbar, *button_box, *hbox;
	GtkWidget *button, *image;

	/* create the toolbar and the box container for its contents */
	toolbar = gtk_toolbar_new ();
	gtk_toolbar_set_show_arrow (GTK_TOOLBAR (toolbar), FALSE);
	gtk_style_context_add_class (gtk_widget_get_style_context (toolbar),
				     "search-bar");
	gtk_box_pack_start (GTK_BOX (editor), toolbar, TRUE, TRUE, 0);

	item = gtk_tool_item_new ();
	gtk_tool_item_set_expand (item, TRUE);
	gtk_toolbar_insert (GTK_TOOLBAR (toolbar), item, -1);

	hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_container_add (GTK_CONTAINER (item), hbox);

	/* create the search entry */
	editor->details->entry = gtk_search_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), editor->details->entry, TRUE, TRUE, 0);

	g_signal_connect (editor->details->entry, "key-press-event",
			  G_CALLBACK (entry_key_press_event_cb), editor);
	g_signal_connect (editor->details->entry, "activate",
			  G_CALLBACK (entry_activate_cb), editor);
	g_signal_connect (editor->details->entry, "search-changed",
			  G_CALLBACK (entry_changed_cb), editor);
	g_signal_connect (editor->details->entry, "stop-search",
                          G_CALLBACK (nautilus_query_editor_on_stop_search), editor);

	/* create the Current/All Files selector */
	editor->details->search_current_button = gtk_radio_button_new_with_label (NULL, _("Current"));
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (editor->details->search_current_button), FALSE);
	editor->details->search_all_button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (editor->details->search_current_button),
											  _("All Files"));
	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (editor->details->search_all_button), FALSE);

	/* connect to the signal only on one of the two, since they're mutually exclusive */
	g_signal_connect (editor->details->search_current_button, "toggled",
			  G_CALLBACK (on_location_button_toggled), editor);

	button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button_box, FALSE, FALSE, 0);
	gtk_style_context_add_class (gtk_widget_get_style_context (button_box),
				     GTK_STYLE_CLASS_LINKED);
	gtk_style_context_add_class (gtk_widget_get_style_context (button_box),
				     GTK_STYLE_CLASS_RAISED);

	gtk_box_pack_start (GTK_BOX (button_box), editor->details->search_current_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (button_box), editor->details->search_all_button, FALSE, FALSE, 0);

	/* finally, create the add new row button */
	button = gtk_button_new ();
	gtk_style_context_add_class (gtk_widget_get_style_context (button),
				     GTK_STYLE_CLASS_RAISED);
	gtk_widget_set_tooltip_text (button,
				     _("Add a new criterion to this search"));
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	image = gtk_image_new_from_icon_name ("list-add-symbolic",
					      GTK_ICON_SIZE_MENU);
	gtk_container_add (GTK_CONTAINER (button), image);

	g_signal_connect (button, "clicked",
			  G_CALLBACK (add_new_row_cb), editor);

	/* show everything */
	gtk_widget_show_all (toolbar);
}

static void
nautilus_query_editor_changed_force (NautilusQueryEditor *editor, gboolean force_reload)
{
	NautilusQuery *query;

	if (editor->details->change_frozen) {
		return;
	}

	query = nautilus_query_editor_get_query (editor);
	g_signal_emit (editor, signals[CHANGED], 0,
		       query, force_reload);
	g_object_unref (query);
}

static void
nautilus_query_editor_changed (NautilusQueryEditor *editor)
{
	nautilus_query_editor_changed_force (editor, TRUE);
}

static void
add_location_to_query (NautilusQueryEditor *editor,
		       NautilusQuery       *query)
{
        GFile *location;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->details->search_all_button))) {
		location = g_file_new_for_uri (nautilus_get_home_directory_uri ());
	} else {
                location = nautilus_query_editor_get_location (editor);
	}

	nautilus_query_set_location (query, location);
	g_clear_object (&location);
}

NautilusQuery *
nautilus_query_editor_get_query (NautilusQueryEditor *editor)
{
	const char *query_text;
	NautilusQuery *query;
	GList *l;
	NautilusQueryEditorRow *row;

	if (editor == NULL || editor->details == NULL || editor->details->entry == NULL) {
		return NULL;
	}

	query_text = gtk_entry_get_text (GTK_ENTRY (editor->details->entry));

	query = nautilus_query_new ();
	nautilus_query_set_text (query, query_text);

	add_location_to_query (editor, query);

	for (l = editor->details->rows; l != NULL; l = l->next) {
		row = l->data;
		
		row_type[row->type].add_to_query (row, query);
	}
	
	return query;
}

GtkWidget *
nautilus_query_editor_new (void)
{
	GtkWidget *editor;

	editor = g_object_new (NAUTILUS_TYPE_QUERY_EDITOR, NULL);
	setup_widgets (NAUTILUS_QUERY_EDITOR (editor));

	return editor;
}

static void
update_location (NautilusQueryEditor *editor)
{
	NautilusFile *file;
	GtkWidget *label;

        file = nautilus_file_get (editor->details->current_location);

	if (file != NULL) {
		char *name;
		if (nautilus_file_is_home (file)) {
			name = g_strdup (_("Home"));
		} else {
			name = nautilus_file_get_display_name (file);
		}

		gtk_button_set_label (GTK_BUTTON (editor->details->search_current_button), name);
		g_free (name);

		label = gtk_bin_get_child (GTK_BIN (editor->details->search_current_button));
		gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
		g_object_set (label, "max-width-chars", 30, NULL);

		nautilus_file_unref (file);
	}
}

void
nautilus_query_editor_set_location (NautilusQueryEditor *editor,
				    GFile               *location)
{
        NautilusDirectory *directory;
        NautilusDirectory *base_model;

	g_clear_object (&editor->details->current_location);

        /* The client could set us a location that is actually a search directory,
         * like what happens with the slot when updating the query editor location.
         * However here we want the real location used as a model for the search,
         * not the search directory invented uri. */
        directory = nautilus_directory_get (location);
        if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
                base_model = nautilus_search_directory_get_base_model (NAUTILUS_SEARCH_DIRECTORY (directory));
                editor->details->current_location = nautilus_directory_get_location (base_model);
        } else {
                editor->details->current_location = g_object_ref (location);
        }
	update_location (editor);

        g_clear_object (&directory);
}

static void
update_rows (NautilusQueryEditor *editor,
	     NautilusQuery       *query)
{
	NautilusQueryEditorRowType type;

	/* if we were just created, set the rows from query,
	 * otherwise, re-use the query setting we have already.
	 */
	if (query != NULL && editor->details->query == NULL) {
		for (type = 0; type < NAUTILUS_QUERY_EDITOR_ROW_LAST; type++) {
			row_type[type].add_rows_from_query (editor, query);
		}
	} else if (query == NULL && editor->details->query != NULL) {
		g_list_free_full (editor->details->rows, (GDestroyNotify) row_destroy);
		editor->details->rows = NULL;
	}
}

void
nautilus_query_editor_set_query (NautilusQueryEditor	*editor,
				 NautilusQuery		*query)
{
	char *text = NULL;
	char *current_text = NULL;

	if (query != NULL) {
		text = nautilus_query_get_text (query);
	}

	if (!text) {
		text = g_strdup ("");
	}

	editor->details->change_frozen = TRUE;

	current_text = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (editor->details->entry))));
	if (!g_str_equal (current_text, text)) {
		gtk_entry_set_text (GTK_ENTRY (editor->details->entry), text);
	}
	g_free (current_text);

        g_clear_object (&editor->details->current_location);

	update_rows (editor, query);
	g_clear_object (&editor->details->query);

	if (query != NULL) {
		editor->details->query = g_object_ref (query);
		editor->details->current_location = nautilus_query_get_location (query);
		update_location (editor);
	}

	editor->details->change_frozen = FALSE;

	g_free (text);
}
