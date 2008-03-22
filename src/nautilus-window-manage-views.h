/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef NAUTILUS_WINDOW_MANAGE_VIEWS_H
#define NAUTILUS_WINDOW_MANAGE_VIEWS_H

#include "nautilus-window.h"
#include "nautilus-navigation-window.h"

void                    nautilus_window_manage_views_destroy          (NautilusWindow           *window);

void                    nautilus_window_manage_views_close_slot       (NautilusWindow           *window,
								       NautilusWindowSlot       *slot);

void                    nautilus_navigation_window_set_sidebar_panels (NautilusNavigationWindow *window,
                                                                       GList                    *view_identifier_list);

/* view interaction (through slots) */
void                    nautilus_window_slot_open_location            (NautilusWindowSlot       *slot,
                                                                       GFile                    *location,
                                                                       gboolean                  close_behind);
void                    nautilus_window_slot_open_location_with_selection (NautilusWindowSlot       *slot,
                                                                       GFile                    *location,
                                                                       GList                    *selection,
                                                                       gboolean                  close_behind);
void                    nautilus_window_slot_open_location_full       (NautilusWindowSlot       *slot,
                                                                       GFile                    *location,
                                                                       NautilusWindowOpenMode    mode,
                                                                       NautilusWindowOpenFlags   flags,
                                                                       GList                    *new_selection);
void                    nautilus_window_slot_stop_loading             (NautilusWindowSlot       *slot);

void                    nautilus_window_slot_set_content_view         (NautilusWindowSlot       *slot,
                                                                       const char               *id);
const char             *nautilus_window_slot_get_content_view_id      (NautilusWindowSlot       *slot);
gboolean                nautilus_window_slot_content_view_matches_iid (NautilusWindowSlot       *slot,
                                                                       const char               *iid);


/* NautilusWindowInfo implementation, exposed to the view API */
void nautilus_window_report_load_underway     (NautilusWindow     *window,
                                               NautilusView       *view);
void nautilus_window_report_selection_changed (NautilusWindow     *window);
void nautilus_window_report_view_failed       (NautilusWindow     *window,
                                               NautilusView       *view);
void nautilus_window_report_load_complete     (NautilusWindow     *window,
                                               NautilusView       *view);
void nautilus_window_report_location_change   (NautilusWindow     *window);

#endif /* NAUTILUS_WINDOW_MANAGE_VIEWS_H */
