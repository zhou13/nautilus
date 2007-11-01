/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-mime-actions.c - uri-specific versions of mime action functions

   Copyright (C) 2000, 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Maciej Stachowiak <mjs@eazel.com>
*/

#include <config.h>
#include "nautilus-mime-actions.h"
 
#include "nautilus-file-attributes.h"
#include "nautilus-file.h"
#include "nautilus-metadata.h"
#include <eel/eel-glib-extensions.h>
#include <string.h>

static GList*
filter_nautilus_handler (GList *apps)
{
	GList *l, *next;
	GAppInfo *application;

	l = apps;
	while (l != NULL) {
		application = (GAppInfo *) l->data;
		next = l->next;

		if (strcmp (g_app_info_get_id (application),
			    "nautilus-folder-handler.desktop") == 0) {
			g_object_unref (application);
			apps = g_list_delete_link (apps, l); 
		}

		l = next;
	}

	return apps;
}

static GList*
filter_non_uri_apps (GList *apps)
{
	GList *l, *next;
	GAppInfo *app;

	for (l = apps; l != NULL; l = next) {
		app = l->data;
		next = l->next;
		
		if (!g_app_info_supports_uris (app)) {
			apps = g_list_delete_link (apps, l);
			g_object_unref (app);
		}
	}
	return apps;
}


static gboolean
nautilus_mime_actions_check_if_required_attributes_ready (NautilusFile *file)
{
	NautilusFileAttributes attributes;
	gboolean ready;

	attributes = nautilus_mime_actions_get_required_file_attributes ();
	ready = nautilus_file_check_if_ready (file, attributes);

	return ready;
}

NautilusFileAttributes 
nautilus_mime_actions_get_required_file_attributes (void)
{
	return NAUTILUS_FILE_ATTRIBUTE_INFO |
		NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
		NAUTILUS_FILE_ATTRIBUTE_METADATA;
}

static gboolean
file_has_local_path (NautilusFile *file)
{
	GFile *location;
	char *path;
	gboolean res;

	/* Don't check _is_native, because we want to support
	   using the fuse path */
	location = nautilus_file_get_location (file);
	path = g_file_get_path (location);

	res = path != NULL;
	
	g_free (path);
	g_object_unref (location);

	return res;
}

GAppInfo *
nautilus_mime_get_default_application_for_file (NautilusFile *file)
{
	GAppInfo *app;
	char *mime_type;
	char *uri_scheme;

	if (!nautilus_mime_actions_check_if_required_attributes_ready (file)) {
		return NULL;
	}

	mime_type = nautilus_file_get_mime_type (file);
	app = g_app_info_get_default_for_type (mime_type, file_has_local_path (file));
	g_free (mime_type);

	if (app == NULL) {
		uri_scheme = nautilus_file_get_uri_scheme (file);
		if (uri_scheme != NULL) {
			app = g_app_info_get_default_for_uri_scheme (uri_scheme);
			g_free (uri_scheme);
		}
	}
	
	return app;
}

static int
file_compare_by_mime_type (NautilusFile *file_a,
			   NautilusFile *file_b)
{
	char *mime_type_a, *mime_type_b;
	int ret;
	
	mime_type_a = nautilus_file_get_mime_type (file_a);
	mime_type_b = nautilus_file_get_mime_type (file_b);
	
	ret = strcmp (mime_type_a, mime_type_b);
	
	g_free (mime_type_a);
	g_free (mime_type_b);
	
	return ret;
}

static int
file_compare_by_parent_uri (NautilusFile *file_a,
			    NautilusFile *file_b) {
	char *parent_uri_a, *parent_uri_b;
	int ret;

	parent_uri_a = nautilus_file_get_parent_uri (file_a);
	parent_uri_b = nautilus_file_get_parent_uri (file_b);

	ret = strcmp (parent_uri_a, parent_uri_b);

	g_free (parent_uri_a);
	g_free (parent_uri_b);

	return ret;
}

static int
application_compare_by_name (const GAppInfo *app_a,
			     const GAppInfo *app_b)
{
	return g_utf8_collate (g_app_info_get_name ((GAppInfo *)app_a),
			       g_app_info_get_name ((GAppInfo *)app_b));
}

static int
application_compare_by_id (const GAppInfo *app_a,
			   const GAppInfo *app_b)
{
	return g_utf8_collate (g_app_info_get_id ((GAppInfo *)app_a),
			       g_app_info_get_id ((GAppInfo *)app_b));
}

GList *
nautilus_mime_get_applications_for_file (NautilusFile *file)
{
	char *mime_type;
	char *uri_scheme;
	GList *result;
	GAppInfo *uri_handler;

	if (!nautilus_mime_actions_check_if_required_attributes_ready (file)) {
		return NULL;
	}
	mime_type = nautilus_file_get_mime_type (file);
	result = g_app_info_get_all_for_type (mime_type);

	uri_scheme = nautilus_file_get_uri_scheme (file);
	if (uri_scheme != NULL) {
		uri_handler = g_app_info_get_default_for_uri_scheme (uri_scheme);
		if (uri_handler) {
			result = g_list_prepend (result, uri_handler);
		}
		g_free (uri_scheme);
	}
	
	if (!file_has_local_path (file)) {
		/* Filter out non-uri supporting apps */
		result = filter_non_uri_apps (result);
	}
	
	result = g_list_sort (result, (GCompareFunc) application_compare_by_name);
	g_free (mime_type);

	return filter_nautilus_handler (result);
}

gboolean
nautilus_mime_has_any_applications_for_file (NautilusFile *file)
{
	GList *apps;
	char *mime_type;
	gboolean result;
	char *uri_scheme;
	GAppInfo *uri_handler;

	mime_type = nautilus_file_get_mime_type (file);
	
	apps = g_app_info_get_all_for_type (mime_type);

	uri_scheme = nautilus_file_get_uri_scheme (file);
	if (uri_scheme != NULL) {
		uri_handler = g_app_info_get_default_for_uri_scheme (uri_scheme);
		if (uri_handler) {
			apps = g_list_prepend (apps, uri_handler);
		}
		g_free (uri_scheme);
	}
	
	if (!file_has_local_path (file)) {
		/* Filter out non-uri supporting apps */
		apps = filter_non_uri_apps (apps);
	}
	apps = filter_nautilus_handler (apps);
		
	if (apps) {
		result = TRUE;
		eel_g_object_list_free (apps);
	} else {
		result = FALSE;
	}
	
	g_free (mime_type);

	return result;
}

GAppInfo *
nautilus_mime_get_default_application_for_files (GList *files)
{
	GList *l, *sorted_files;
	NautilusFile *file;
	GAppInfo *app, *one_app;

	g_assert (files != NULL);

	sorted_files = g_list_sort (g_list_copy (files), (GCompareFunc) file_compare_by_mime_type);

	app = NULL;
	for (l = sorted_files; l != NULL; l = l->next) {
		file = l->data;

		if (l->prev &&
		    file_compare_by_mime_type (file, l->prev->data) == 0 &&
		    file_compare_by_parent_uri (file, l->prev->data) == 0) {
			continue;
		}

		one_app = nautilus_mime_get_default_application_for_file (file);
		if (one_app == NULL || (app != NULL && !g_app_info_equal (app, one_app))) {
			if (app) {
				g_object_unref (app);
			}
			if (one_app) {
				g_object_unref (one_app);
			}
			app = NULL;
			break;
		}

		if (app == NULL) {
			app = one_app;
		} else {
			g_object_unref (one_app);
		}
	}

	g_list_free (sorted_files);

	return app;
}

/* returns an intersection of two mime application lists,
 * and returns a new list, freeing a, b and all applications
 * that are not in the intersection set.
 * The lists are assumed to be pre-sorted by their IDs */
static GList *
intersect_application_lists (GList *a,
			     GList *b)
{
	GList *l, *m;
	GList *ret;
	GAppInfo *a_app, *b_app;
	int cmp;

	ret = NULL;

	l = a;
	m = b;

	while (l != NULL && m != NULL) {
		a_app = (GAppInfo *) l->data;
		b_app = (GAppInfo *) m->data;

		cmp = strcmp (g_app_info_get_id (a_app),
			      g_app_info_get_id (b_app));
		if (cmp > 0) {
			g_object_unref (b_app);
			m = m->next;
		} else if (cmp < 0) {
			g_object_unref (a_app);
			l = l->next;
		} else {
			g_object_unref (b_app);
			ret = g_list_prepend (ret, a_app);
			l = l->next;
			m = m->next;
		}
	}

	g_list_foreach (l, (GFunc) g_object_unref, NULL);
	g_list_foreach (m, (GFunc) g_object_unref, NULL);

	g_list_free (a);
	g_list_free (b);

	return g_list_reverse (ret);
}

GList *
nautilus_mime_get_applications_for_files (GList *files)
{
	GList *l, *sorted_files;
	NautilusFile *file;
	GList *one_ret, *ret;

	g_assert (files != NULL);

	sorted_files = g_list_sort (g_list_copy (files), (GCompareFunc) file_compare_by_mime_type);

	ret = NULL;
	for (l = sorted_files; l != NULL; l = l->next) {
		file = l->data;

		if (l->prev &&
		    file_compare_by_mime_type (file, l->prev->data) == 0 &&
		    file_compare_by_parent_uri (file, l->prev->data) == 0) {
			continue;
		}

		one_ret = nautilus_mime_get_applications_for_file (file);
		one_ret = g_list_sort (one_ret, (GCompareFunc) application_compare_by_id);
		if (ret != NULL) {
			ret = intersect_application_lists (ret, one_ret);
		} else {
			ret = one_ret;
		}

		if (ret == NULL) {
			break;
		}
	}

	g_list_free (sorted_files);

	ret = g_list_sort (ret, (GCompareFunc) application_compare_by_name);
	
	return ret;
}

gboolean
nautilus_mime_has_any_applications_for_files (GList *files)
{
	GList *l, *sorted_files;
	NautilusFile *file;
	gboolean ret;

	g_assert (files != NULL);

	sorted_files = g_list_sort (g_list_copy (files), (GCompareFunc) file_compare_by_mime_type);

	ret = TRUE;
	for (l = sorted_files; l != NULL; l = l->next) {
		file = NAUTILUS_FILE (l->data);

		if (l->prev &&
		    file_compare_by_mime_type (file, l->prev->data) == 0 &&
		    file_compare_by_parent_uri (file, l->prev->data) == 0) {
			continue;
		}

		if (!nautilus_mime_has_any_applications_for_file (file)) {
			ret = FALSE;
			break;
		}
	}

	g_list_free (sorted_files);

	return ret;
}
