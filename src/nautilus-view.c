/* nautilus-view.c
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

#include "nautilus-view.h"
#include "nautilus-enum-types.h"

#include <glib/gi18n.h>

G_DEFINE_INTERFACE (NautilusView, nautilus_view, GTK_TYPE_WIDGET)

static void
nautilus_view_default_init (NautilusViewInterface *iface)
{
  /**
   * NautilusView::action-group:
   *
   * The action group of the implementing view. Different views may
   * have different sets of actions.
   *
   * Since: 3.18
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("action-group",
                                                            _("Action group of the view"),
                                                            _("The action group of the view"),
                                                            G_TYPE_ACTION_GROUP,
                                                            G_PARAM_READABLE));

  /**
   * NautilusView::loading:
   *
   * Whether the view is loading or not.
   *
   * Since: 3.18
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("loading",
                                                            _("Whether the view is loading"),
                                                            _("Whether the view is loading or not"),
                                                            FALSE,
                                                            G_PARAM_READABLE));

  /**
   * NautilusView::location:
   *
   * The current location of the view.
   *
   * Since: 3.18
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("location",
                                                            _("Location the view is displaying"),
                                                            _("The location the view is displaying"),
                                                            G_TYPE_FILE,
                                                            G_PARAM_READABLE));

  /**
   * NautilusView::search-query:
   *
   * The search query the view should search for.
   *
   * Since: 3.18
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("search-query",
                                                            _("Search query that view is performing"),
                                                            _("The search query that view is performing"),
                                                            NAUTILUS_TYPE_QUERY,
                                                            G_PARAM_READWRITE));

  /**
   * NautilusView::operations:
   *
   * The operations the view supports.
   *
   * Since: 3.18
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_flags ("operations",
                                                           _("Operations that view supports"),
                                                           _("The operations that view supports"),
                                                           NAUTILUS_TYPE_VIEW_OPERATION,
                                                           NAUTILUS_VIEW_OPERATION_NONE,
                                                           G_PARAM_READABLE));

  /**
   * NautilusView::open-location:
   * @view: the object which received the signal.
   * @location: (type Gio.File): #GFile to which the caller should switch.
   * @flags: a single value from #GtkPlacesOpenFlags specifying how the @location
   * should be opened.
   *
   * Emited when the view wants the parent slot to open a location.
   *
   * Since: 3.18
   */
  g_signal_new ("open-location",
                G_TYPE_FROM_INTERFACE (iface),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (NautilusViewInterface, open_location),
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                2,
                G_TYPE_FILE,
                GTK_TYPE_PLACES_OPEN_FLAGS);
}

/**
 * nautilus_view_activate_selection:
 * @view: a #NautilusView
 *
 * Activates the selection of @view, or the first visible item in case
 * nothing is selected.
 *
 * Returns:
 */
void
nautilus_view_activate_selection (NautilusView *view)
{
  g_return_if_fail (NAUTILUS_IS_VIEW (view));
  g_return_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->activate_selection);

  NAUTILUS_VIEW_GET_IFACE (view)->activate_selection (view);
}

/**
 * nautilus_view_get_action_group:
 * @view: a #NautilusView
 *
 * Retrieves the #GActionGroup grom @view, or %NULL if none is set.
 *
 * Returns: (transfer none): the @view's #GActionGroup
 */
GActionGroup*
nautilus_view_get_action_group (NautilusView *view)
{
  g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
  g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_action_group, NULL);

  return NAUTILUS_VIEW_GET_IFACE (view)->get_action_group (view);
}

/**
 * nautilus_view_get_loading:
 * @view: a #NautilusView
 *
 * Retrieves the loading state of @view.
 *
 * Returns: %TRUE if @view is loading, %FALSE if it's ready.
 */
gboolean
nautilus_view_get_loading (NautilusView *view)
{
  g_return_val_if_fail (NAUTILUS_IS_VIEW (view), FALSE);
  g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_loading, FALSE);

  return NAUTILUS_VIEW_GET_IFACE (view)->get_loading (view);
}

/**
 * nautilus_view_get_location:
 *
 * Retrieves the current location that @view is displaying, or %NULL
 * if none is set.
 *
 * Returns: (transfer none): A #GFile representing the location that
 * @view is displaying.
 */
GFile*
nautilus_view_get_location (NautilusView *view)
{
  g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
  g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_location, NULL);

  return NAUTILUS_VIEW_GET_IFACE (view)->get_location (view);
}

/**
 * nautilus_view_get_search_query:
 *
 * Retrieves the current search query that @view is performing, or %NULL
 * if none is set.
 *
 * Returns: (transfer none): A #NautilusQuery representing the search that
 * @view is displaying.
 */
NautilusQuery*
nautilus_view_get_search_query (NautilusView *view)
{
  g_return_val_if_fail (NAUTILUS_IS_VIEW (view), NULL);
  g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_search_query, NULL);

  return NAUTILUS_VIEW_GET_IFACE (view)->get_search_query (view);
}

/**
 * nautilus_view_set_search_query:
 *
 * Sets the search query that @view should perform, or %NULL to stop
 * the current search. It is required that the implementing class supports
 * %NAUTILUS_VIEW_OPERATION_SEARCH operation.
 *
 * Returns:
 */
void
nautilus_view_set_search_query (NautilusView  *view,
                                NautilusQuery *query)
{
  g_return_if_fail (NAUTILUS_IS_VIEW (view));
  g_return_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->set_search_query);
  g_return_if_fail (nautilus_view_is_operation_supported (view, NAUTILUS_VIEW_OPERATION_SEARCH));

  NAUTILUS_VIEW_GET_IFACE (view)->set_search_query (view, query);
}

/**
 * nautilus_view_popup_menu:
 * @view: a #NautilusView
 * @event: a #GdkEventButton
 * @location: the location the popup-menu should be created for, or %NULL
 * for the currently displayed location.
 *
 * Shows a popup context menu for the window's pathbar.
 *
 * Returns: %TRUE if @view is loading, %FALSE if it's ready.
 */
void
nautilus_view_popup_menu (NautilusView         *view,
                          NautilusViewMenutype  menu_type,
                          GdkEventButton       *event,
                          const gchar          *location)
{
  g_return_if_fail (NAUTILUS_IS_VIEW (view));
  g_return_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->popup_menu);

  NAUTILUS_VIEW_GET_IFACE (view)->popup_menu (view, menu_type, event, location);
}

/**
 * nautilus_view_get_supported_actions:
 *
 * Retrieves the supported #NautilusViewAction flags.
 *
 * Returns: a bitflag representing the supported #NautilusViewAction
 * values
 */
gint
nautilus_view_get_supported_operations (NautilusView *view)
{
  g_return_val_if_fail (NAUTILUS_IS_VIEW (view), 0);
  g_return_val_if_fail (NAUTILUS_VIEW_GET_IFACE (view)->get_supported_operations, 0);

  return NAUTILUS_VIEW_GET_IFACE (view)->get_supported_operations (view);
}

/**
 * nautilus_view_get_supported_actions:
 *
 * Checks whether @view supports @action.
 *
 * Returns: %TRUE if @action is supported by @view,
 * %FALSE otherwise.
 */
gboolean
nautilus_view_is_operation_supported (NautilusView          *view,
                                      NautilusViewOperation  operation)
{
  return nautilus_view_get_supported_operations (view) & operation;
}
