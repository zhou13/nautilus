/* nautilus-view.h
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_VIEW_H
#define NAUTILUS_VIEW_H

#include <glib.h>
#include <gtk/gtk.h>

#include <libnautilus-private/nautilus-query.h>

G_BEGIN_DECLS

typedef enum
{
  NAUTILUS_VIEW_MENU_BACKGROUND,
  NAUTILUS_VIEW_MENU_PATHBAR,
  NAUTILUS_VIEW_MENU_SELECTION
} NautilusViewMenutype;

typedef enum
{
  NAUTILUS_VIEW_OPERATION_NONE            = 0,
  NAUTILUS_VIEW_OPERATION_BROWSE_GRID     = 1 << 1,
  NAUTILUS_VIEW_OPERATION_BROWSE_LIST     = 1 << 2,
  NAUTILUS_VIEW_OPERATION_SEARCH          = 1 << 3,
  NAUTILUS_VIEW_OPERATION_SELECTION       = 1 << 4,
  NAUTILUS_VIEW_OPERATION_SORT            = 1 << 5
} NautilusViewOperation;

#define NAUTILUS_TYPE_VIEW          (nautilus_view_get_type ())

G_DECLARE_INTERFACE (NautilusView, nautilus_view, NAUTILUS, VIEW, GtkWidget)

struct _NautilusViewInterface
{
  GTypeInterface parent;

  /* methods */
  void             (*activate_selection)                       (NautilusView         *view);

  GActionGroup*    (*get_action_group)                         (NautilusView         *view);

  gboolean         (*get_loading)                              (NautilusView         *view);

  GFile*           (*get_location)                             (NautilusView         *view);

  NautilusQuery*   (*get_search_query)                         (NautilusView         *view);

  void             (*set_search_query)                         (NautilusView         *view,
                                                                NautilusQuery        *query);

  gint             (*get_supported_operations)                 (NautilusView         *view);

  void             (*popup_menu)                               (NautilusView         *view,
                                                                NautilusViewMenutype  menu_type,
                                                                GdkEventButton       *event,
                                                                const gchar          *location);

  /* signal slots */
  void             (*open_location)                            (NautilusView         *view,
                                                                GFile                *location,
                                                                GtkPlacesOpenFlags    flags);
};

void               nautilus_view_activate_selection            (NautilusView         *view);

GActionGroup*      nautilus_view_get_action_group              (NautilusView         *view);

gboolean           nautilus_view_get_loading                   (NautilusView         *view);

GFile*             nautilus_view_get_location                  (NautilusView         *view);

NautilusQuery*     nautilus_view_get_search_query              (NautilusView         *view);

void               nautilus_view_set_search_query              (NautilusView         *view,
                                                                NautilusQuery        *query);

gint               nautilus_view_get_supported_operations      (NautilusView         *view);

void               nautilus_view_popup_menu                    (NautilusView         *view,
                                                                NautilusViewMenutype  menu_type,
                                                                GdkEventButton       *event,
                                                                const gchar          *location);

gboolean           nautilus_view_is_operation_supported        (NautilusView         *view,
                                                                NautilusViewOperation action);

G_END_DECLS

#endif /* NAUTILUS_VIEW_H */
