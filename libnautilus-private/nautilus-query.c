/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>
#include <string.h>

#include <eel/eel-glib-extensions.h>
#include <glib/gi18n.h>

#include "nautilus-file-utilities.h"
#include "nautilus-query.h"
#include "nautilus-private-enum-types.h"

struct _NautilusQuery {
        GObject parent;

	char *text;
	GFile *location;
	GList *mime_types;
	gboolean show_hidden;
        GDateTime *datetime;
        NautilusQuerySearchType search_type;
        NautilusQuerySearchContent search_content;

        gboolean searching : 1;
        gboolean recursive : 1;
	char **prepared_words;
};

static void  nautilus_query_class_init       (NautilusQueryClass *class);
static void  nautilus_query_init             (NautilusQuery      *query);

G_DEFINE_TYPE (NautilusQuery, nautilus_query, G_TYPE_OBJECT);

enum {
        PROP_0,
        PROP_DATE,
        PROP_LOCATION,
        PROP_MIMETYPES,
        PROP_RECURSIVE,
        PROP_SEARCH_TYPE,
        PROP_SEARCHING,
        PROP_SHOW_HIDDEN,
        PROP_TEXT,
        LAST_PROP
};

static void
finalize (GObject *object)
{
	NautilusQuery *query;

	query = NAUTILUS_QUERY (object);

        g_free (query->text);
        g_strfreev (query->prepared_words);
        g_clear_object (&query->location);
        g_clear_pointer (&query->datetime, g_date_time_unref);

	G_OBJECT_CLASS (nautilus_query_parent_class)->finalize (object);
}

static void
nautilus_query_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
        NautilusQuery *self = NAUTILUS_QUERY (object);

        switch (prop_id) {
        case PROP_DATE:
                g_value_set_boxed (value, self->datetime);
                break;

        case PROP_LOCATION:
                g_value_set_object (value, self->location);
                break;

        case PROP_MIMETYPES:
                g_value_set_pointer (value, self->mime_types);
                break;

	case PROP_RECURSIVE:
		g_value_set_boolean (value, self->recursive);
		break;

        case PROP_SEARCH_TYPE:
                g_value_set_enum (value, self->search_type);
                break;

        case PROP_SEARCHING:
                g_value_set_boolean (value, self->searching);
                break;

        case PROP_SHOW_HIDDEN:
                g_value_set_boolean (value, self->show_hidden);
                break;

        case PROP_TEXT:
                g_value_set_string (value, self->text);
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
nautilus_query_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
        NautilusQuery *self = NAUTILUS_QUERY (object);

        switch (prop_id) {
        case PROP_DATE:
                nautilus_query_set_date (self, g_value_get_boxed (value));
                break;

        case PROP_LOCATION:
                nautilus_query_set_location (self, g_value_get_object (value));
                break;

        case PROP_MIMETYPES:
                nautilus_query_set_mime_types (self, g_value_get_pointer (value));
                break;

	case PROP_RECURSIVE:
		nautilus_query_set_recursive (self, g_value_get_boolean (value));
		break;

        case PROP_SEARCH_TYPE:
                nautilus_query_set_search_type (self, g_value_get_enum (value));
                break;

        case PROP_SEARCHING:
                nautilus_query_set_searching (self, g_value_get_boolean (value));
                break;
        case PROP_SHOW_HIDDEN:
                nautilus_query_set_show_hidden_files (self, g_value_get_boolean (value));
                break;

        case PROP_TEXT:
                nautilus_query_set_text (self, g_value_get_string (value));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
nautilus_query_class_init (NautilusQueryClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;
        gobject_class->get_property = nautilus_query_get_property;
        gobject_class->set_property = nautilus_query_set_property;

        /**
         * NautilusQuery::date:
         *
         * The initial date of the query.
         *
         */
        g_object_class_install_property (gobject_class,
                                         PROP_DATE,
                                         g_param_spec_boxed ("date",
                                                             "Date of the query",
                                                             "The initial date of the query",
                                                             G_TYPE_DATE_TIME,
                                                             G_PARAM_READWRITE));

        /**
         * NautilusQuery::location:
         *
         * The location of the query.
         *
         */
        g_object_class_install_property (gobject_class,
                                         PROP_LOCATION,
                                         g_param_spec_object ("location",
                                                              "Location of the query",
                                                              "The location of the query",
                                                              G_TYPE_FILE,
                                                              G_PARAM_READWRITE));

        /**
         * NautilusQuery::mimetypes:
         *
         * MIME types the query holds.
         *
         */
        g_object_class_install_property (gobject_class,
                                         PROP_MIMETYPES,
                                         g_param_spec_pointer ("mimetypes",
                                                               "MIME types of the query",
                                                               "The MIME types of the query",
                                                               G_PARAM_READWRITE));

	/**
         * NautilusQuery::recursive:
         *
         * Whether the query is being performed on subdirectories or not.
         *
         */
        g_object_class_install_property (gobject_class,
                                         PROP_RECURSIVE,
                                         g_param_spec_boolean ("recursive",
                                                               "Whether the query is being performed on subdirectories",
                                                               "Whether the query is being performed on subdirectories or not",
                                                               FALSE,
                                                               G_PARAM_READWRITE));

        /**
         * NautilusQuery::search-type:
         *
         * The search type of the query.
         *
         */
        g_object_class_install_property (gobject_class,
                                         PROP_SEARCH_TYPE,
                                         g_param_spec_enum ("search-type",
                                                            "Type of the query",
                                                            "The type of the query",
                                                            NAUTILUS_TYPE_QUERY_SEARCH_TYPE,
                                                            NAUTILUS_QUERY_SEARCH_TYPE_LAST_MODIFIED,
                                                            G_PARAM_READWRITE));

        /**
         * NautilusQuery::searching:
         *
         * Whether the query is being performed or not.
         *
         */
        g_object_class_install_property (gobject_class,
                                         PROP_SEARCHING,
                                         g_param_spec_boolean ("searching",
                                                               "Whether the query is being performed",
                                                               "Whether the query is being performed or not",
                                                               FALSE,
                                                               G_PARAM_READWRITE));

        /**
         * NautilusQuery::show-hidden:
         *
         * Whether the search should include hidden files.
         *
         */
        g_object_class_install_property (gobject_class,
                                         PROP_SHOW_HIDDEN,
                                         g_param_spec_boolean ("show-hidden",
                                                               "Show hidden files",
                                                               "Whether the search should show hidden files",
                                                               FALSE,
                                                               G_PARAM_READWRITE));

        /**
         * NautilusQuery::text:
         *
         * The search string.
         *
         */
        g_object_class_install_property (gobject_class,
                                         PROP_TEXT,
                                         g_param_spec_string ("text",
                                                              "Text of the search",
                                                              "The text string of the search",
                                                              NULL,
                                                              G_PARAM_READWRITE));
}

static void
nautilus_query_init (NautilusQuery *query)
{
        query->show_hidden = TRUE;
        query->location = g_file_new_for_path (g_get_home_dir ());
        query->search_type = NAUTILUS_QUERY_SEARCH_TYPE_LAST_MODIFIED;
        query->search_content = NAUTILUS_QUERY_SEARCH_CONTENT_SIMPLE;
}

static gchar *
prepare_string_for_compare (const gchar *string)
{
	gchar *normalized, *res;

	normalized = g_utf8_normalize (string, -1, G_NORMALIZE_NFD);
	res = g_utf8_strdown (normalized, -1);
	g_free (normalized);

	return res;
}

gdouble
nautilus_query_matches_string (NautilusQuery *query,
			       const gchar *string)
{
	gchar *prepared_string, *ptr;
	gboolean found;
	gdouble retval;
	gint idx, nonexact_malus;

        if (!query->text) {
		return -1;
	}

        if (!query->prepared_words) {
                prepared_string = prepare_string_for_compare (query->text);
                query->prepared_words = g_strsplit (prepared_string, " ", -1);
		g_free (prepared_string);
	}

	prepared_string = prepare_string_for_compare (string);
	found = TRUE;
	ptr = NULL;
	nonexact_malus = 0;

        for (idx = 0; query->prepared_words[idx] != NULL; idx++) {
                if ((ptr = strstr (prepared_string, query->prepared_words[idx])) == NULL) {
			found = FALSE;
			break;
		}

                nonexact_malus += strlen (ptr) - strlen (query->prepared_words[idx]);
	}

	if (!found) {
		g_free (prepared_string);
		return -1;
	}

	retval = MAX (10.0, 50.0 - (gdouble) (ptr - prepared_string) - nonexact_malus);
	g_free (prepared_string);

	return retval;
}

NautilusQuery *
nautilus_query_new (void)
{
	return g_object_new (NAUTILUS_TYPE_QUERY,  NULL);
}


char *
nautilus_query_get_text (NautilusQuery *query)
{
        g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NULL);

        return g_strdup (query->text);
}

void 
nautilus_query_set_text (NautilusQuery *query, const char *text)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        g_free (query->text);
        query->text = g_strstrip (g_strdup (text));

        g_strfreev (query->prepared_words);
        query->prepared_words = NULL;

        g_object_notify (G_OBJECT (query), "text");
}

GFile*
nautilus_query_get_location (NautilusQuery *query)
{
        g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NULL);

        return g_object_ref (query->location);
}

void
nautilus_query_set_location (NautilusQuery *query,
                             GFile         *location)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        if (g_set_object (&query->location, location)) {
                g_object_notify (G_OBJECT (query), "location");
        }

}

GList *
nautilus_query_get_mime_types (NautilusQuery *query)
{
        g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NULL);

        return g_list_copy_deep (query->mime_types, (GCopyFunc) g_strdup, NULL);
}

void
nautilus_query_set_mime_types (NautilusQuery *query, GList *mime_types)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        g_list_free_full (query->mime_types, g_free);
        query->mime_types = g_list_copy_deep (mime_types, (GCopyFunc) g_strdup, NULL);

        g_object_notify (G_OBJECT (query), "mimetypes");
}

void
nautilus_query_add_mime_type (NautilusQuery *query, const char *mime_type)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        query->mime_types = g_list_append (query->mime_types, g_strdup (mime_type));

        g_object_notify (G_OBJECT (query), "mimetypes");
}

gboolean
nautilus_query_get_show_hidden_files (NautilusQuery *query)
{
        g_return_val_if_fail (NAUTILUS_IS_QUERY (query), FALSE);

        return query->show_hidden;
}

void
nautilus_query_set_show_hidden_files (NautilusQuery *query, gboolean show_hidden)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        if (query->show_hidden != show_hidden) {
                query->show_hidden = show_hidden;
                g_object_notify (G_OBJECT (query), "show-hidden");
        }
}

char *
nautilus_query_to_readable_string (NautilusQuery *query)
{
        if (!query || !query->text || query->text[0] == '\0') {
		return g_strdup (_("Search"));
	}

        return g_strdup_printf (_("Search for “%s”"), query->text);
}

NautilusQuerySearchContent
nautilus_query_get_search_content (NautilusQuery *query)
{
        g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NAUTILUS_QUERY_SEARCH_CONTENT_SIMPLE);

        return query->search_content;
}

void
nautilus_query_set_search_content (NautilusQuery              *query,
                                   NautilusQuerySearchContent  content)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        if (query->search_content != content) {
                query->search_content = content;
                g_object_notify (G_OBJECT (query), "search-type");
        }
}

NautilusQuerySearchType
nautilus_query_get_search_type (NautilusQuery *query)
{
        g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NAUTILUS_QUERY_SEARCH_TYPE_LAST_MODIFIED);

        return query->search_type;
}

void
nautilus_query_set_search_type (NautilusQuery           *query,
                                NautilusQuerySearchType  type)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        if (query->search_type != type) {
                query->search_type = type;
                g_object_notify (G_OBJECT (query), "search-type");
        }
}

GDateTime*
nautilus_query_get_date (NautilusQuery *query)
{
        g_return_val_if_fail (NAUTILUS_IS_QUERY (query), NULL);

        return query->datetime;
}

void
nautilus_query_set_date (NautilusQuery *query,
                         GDateTime     *date)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        if (query->datetime != date) {
                g_clear_pointer (&query->datetime, g_date_time_unref);
                if (date) {
                        /* Assign the new date */
                        query->datetime = g_date_time_ref (date);
                }

                g_object_notify (G_OBJECT (query), "date");
        }
}

gboolean
nautilus_query_get_searching (NautilusQuery *query)
{
        g_return_val_if_fail (NAUTILUS_IS_QUERY (query), FALSE);

        return query->searching;
}

void
nautilus_query_set_searching (NautilusQuery *query,
			      gboolean       searching)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        searching = !!searching;

        if (query->searching != searching) {
                query->searching = searching;

	        g_object_notify (G_OBJECT (query), "searching");
        }
}

gboolean
nautilus_query_get_recursive (NautilusQuery *query)
{
        g_return_val_if_fail (NAUTILUS_IS_QUERY (query), FALSE);

        return query->recursive;
}

void
nautilus_query_set_recursive (NautilusQuery *query,
                              gboolean       recursive)
{
        g_return_if_fail (NAUTILUS_IS_QUERY (query));

        recursive = !!recursive;

        if (query->recursive != recursive) {
                query->recursive = recursive;

                g_object_notify (G_OBJECT (query), "recursive");
        }
}
