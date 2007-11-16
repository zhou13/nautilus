/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-progress-info.h: file operation progress info.
 
   Copyright (C) 2007 Red Hat, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include <math.h>
#include <glib/gi18n.h>
#include <eel/eel-string.h>
#include "nautilus-progress-info.h"

enum {
  CHANGED,
  PROGRESS_CHANGED,
  STARTED,
  FINISHED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _NautilusProgressInfo
{
  GObject parent_instance;

  GCancellable *cancellable;
  
  char *status;
  char *details;
  double progress;
  gboolean activity_mode;
  gboolean started;
  gboolean finished;

  guint idle_tag;

  gboolean start_at_idle;
  gboolean finish_at_idle;
  gboolean changed_at_idle;
  gboolean progress_at_idle;
};

struct _NautilusProgressInfoClass
{
  GObjectClass parent_class;
};


G_LOCK_DEFINE_STATIC(progress_info);

G_DEFINE_TYPE (NautilusProgressInfo, nautilus_progress_info, G_TYPE_OBJECT)

static void
nautilus_progress_info_finalize (GObject *object)
{
  NautilusProgressInfo *info;

  info = NAUTILUS_PROGRESS_INFO (object);

  g_free (info->status);
  g_free (info->details);
  g_object_unref (info->cancellable);

  /* idle refs info, so should never be outstanding at this point */
  g_assert (info->idle_tag == 0);
  
  if (G_OBJECT_CLASS (nautilus_progress_info_parent_class)->finalize)
    (*G_OBJECT_CLASS (nautilus_progress_info_parent_class)->finalize) (object);
}

static void
nautilus_progress_info_class_init (NautilusProgressInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = nautilus_progress_info_finalize;

  signals[CHANGED] =
    g_signal_new ("changed",
		  NAUTILUS_TYPE_PROGRESS_INFO,
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  signals[PROGRESS_CHANGED] =
    g_signal_new ("progress-changed",
		  NAUTILUS_TYPE_PROGRESS_INFO,
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  signals[STARTED] =
    g_signal_new ("started",
		  NAUTILUS_TYPE_PROGRESS_INFO,
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  signals[FINISHED] =
    g_signal_new ("finished",
		  NAUTILUS_TYPE_PROGRESS_INFO,
		  G_SIGNAL_RUN_LAST,
		  0,
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
}

static void
nautilus_progress_info_init (NautilusProgressInfo *info)
{
  info->cancellable = g_cancellable_new ();
}

NautilusProgressInfo *
nautilus_progress_info_new (void)
{
  NautilusProgressInfo *info;

  info = g_object_new (NAUTILUS_TYPE_PROGRESS_INFO, NULL);
  
  return info;
}

char *
nautilus_progress_info_get_status (NautilusProgressInfo *info)
{
  char *res;
  
  G_LOCK (progress_info);

  if (info->status)
    res = g_strdup (info->status);
  else
    res = g_strdup (_("Preparing"));
  
  G_UNLOCK (progress_info);

  return res;
}

char *
nautilus_progress_info_get_details (NautilusProgressInfo *info)
{
  char *res;
  
  G_LOCK (progress_info);

  if (info->details)
    res = g_strdup (info->details);
  else
    res = g_strdup (_("Preparing"));
  
  G_UNLOCK (progress_info);

  return res;
}

double
nautilus_progress_info_get_progress (NautilusProgressInfo *info)
{
  double res;
  
  G_LOCK (progress_info);

  res = info->progress;
  
  G_UNLOCK (progress_info);

  return res;
}

GCancellable *
nautilus_progress_info_get_cancellable (NautilusProgressInfo *info)
{
  GCancellable *c;
  
  G_LOCK (progress_info);

  c = g_object_ref (info->cancellable);
  
  G_UNLOCK (progress_info);

  return c;
}

gboolean
nautilus_progress_info_get_is_started (NautilusProgressInfo *info)
{
  gboolean res;
  
  G_LOCK (progress_info);

  res = info->started;
  
  G_UNLOCK (progress_info);

  return res;
}

gboolean
nautilus_progress_info_get_is_finished (NautilusProgressInfo *info)
{
  gboolean res;
  
  G_LOCK (progress_info);

  res = info->finished;
  
  G_UNLOCK (progress_info);

  return res;
}

static gboolean
idle_callback (gpointer data)
{
  NautilusProgressInfo *info = data;
  gboolean start_at_idle;
  gboolean finish_at_idle;
  gboolean changed_at_idle;
  gboolean progress_at_idle;
  
  G_LOCK (progress_info);

  info->idle_tag = 0;
  
  start_at_idle = info->start_at_idle;
  finish_at_idle = info->finish_at_idle;
  changed_at_idle = info->changed_at_idle;
  progress_at_idle = info->progress_at_idle;

  info->start_at_idle = FALSE;
  info->finish_at_idle = FALSE;
  info->changed_at_idle = FALSE;
  info->progress_at_idle = FALSE;
  
  G_UNLOCK (progress_info);

  if (start_at_idle)
    g_signal_emit (info,
		   STARTED,
		   0);

  if (changed_at_idle)
    g_signal_emit (info,
		   CHANGED,
		   0);
  
  if (progress_at_idle)
    g_signal_emit (info,
		   PROGRESS_CHANGED,
		   0);
  
  if (finish_at_idle)
    g_signal_emit (info,
		   FINISHED,
		   0);

  g_object_unref (info);

  return FALSE;
}

/* Called with lock held */
static void
queue_idle (NautilusProgressInfo *info)
{
  if (info->idle_tag != 0)
    info->idle_tag = g_idle_add (idle_callback, g_object_ref (info));
}

void
nautilus_progress_info_start (NautilusProgressInfo *info)
{
  G_LOCK (progress_info);

  if (!info->started)
    {
      info->started = TRUE;

      info->start_at_idle = TRUE;
      queue_idle (info);
    }
  
  G_UNLOCK (progress_info);
}

void
nautilus_progress_info_finish (NautilusProgressInfo *info)
{
  G_LOCK (progress_info);

  if (!info->finished)
    {
      info->finished = TRUE;

      info->finish_at_idle = TRUE;
      queue_idle (info);
    }
  
  G_UNLOCK (progress_info);
}

void
nautilus_progress_info_set_status (NautilusProgressInfo *info,
				   const char *status)
{
  G_LOCK (progress_info);

  if (eel_strcmp (info->status, status) != 0)
    {
      g_free (info->status);
      info->status = g_strdup (status);

      info->changed_at_idle = TRUE;
      queue_idle (info);
    }
  
  G_UNLOCK (progress_info);
}

void
nautilus_progress_info_set_details (NautilusProgressInfo *info,
				    const char           *details)
{
  G_LOCK (progress_info);

  if (eel_strcmp (info->details, details) != 0)
    {
      g_free (info->details);
      info->details = g_strdup (details);

      info->changed_at_idle = TRUE;
      queue_idle (info);
    }
  
  G_UNLOCK (progress_info);
}

void
nautilus_progress_info_set_progress (NautilusProgressInfo *info,
				     gboolean              activity_mode,
				     double                current_percent)
{
  G_LOCK (progress_info);

  if (activity_mode || /* Always pulse if activity mode */
      info->activity_mode || /* emit on switch from activity mode */
      fabs (current_percent - info->progress) > 0.1 /* Emit on change of 0.1 percent */
      )
    {
      info->progress = current_percent;

      info->progress_at_idle = TRUE;
      queue_idle (info);
    }
  
  G_UNLOCK (progress_info);
}
