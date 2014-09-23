/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nautilus-pathbar-button-label.c
 *
 * Copyright (C) 2014  Carlos Soriano <carlos.soriano89@gmail.com>
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors Carlos Soriano <carlos.soriano89@gmail.com>
 */

/*
 * GObject that provide the same requested size for default and bold
 * label markup.
 */

#include "nautilus-pathbar-button-label.h"
#include "nautilus-pathbar.h"

#define NAUTILUS_PATH_BAR_LABEL_MAX_WIDTH 250
#define NAUTILUS_PATH_BAR_LABEL_MIN_WIDTH 50

struct _NautilusPathbarButtonLabelPrivate
{
	GtkLabel *label;
	GtkLabel *bold_label;
	gchar	 *dir_name;
};

G_DEFINE_TYPE_WITH_PRIVATE (NautilusPathbarButtonLabel, nautilus_pathbar_button_label, GTK_TYPE_BOX)

NautilusPathbarButtonLabel *
nautilus_pathbar_button_label_new (void)
{
	return g_object_new (NAUTILUS_TYPE_PATHBAR_BUTTON_LABEL, NULL);
}

static void
nautilus_pathbar_button_label_finalize (GObject *object)
{
	NautilusPathbarButtonLabelPrivate *priv = NAUTILUS_PATHBAR_BUTTON_LABEL (object)->priv;

	G_OBJECT_CLASS (nautilus_pathbar_button_label_parent_class)->finalize (object);

	//g_object_unref(priv->label);
	//g_object_unref(priv->bold_label);

	g_free(priv->dir_name);
}

void
nautilus_pathbar_button_label_set_text(NautilusPathbarButtonLabel *self,
				       gchar *dir_name)
{
	g_free(self->priv->dir_name);
	self->priv->dir_name = g_strdup(dir_name);

	char *markup = g_markup_printf_escaped ("<b>%s</b>", dir_name);

	if (gtk_label_get_use_markup (self->priv->label)) {
		gtk_label_set_markup (self->priv->label, markup);
	} else {
		gtk_label_set_text (self->priv->label, dir_name);
	}

	gtk_label_set_markup (self->priv->bold_label, markup);
	g_free (markup);
}

void
nautilus_pathbar_button_label_set_bold(NautilusPathbarButtonLabel *self,
				       gboolean	*bold)
{
	if (bold) {
		if (self->priv->dir_name) {
			char *markup;
			markup = g_markup_printf_escaped ("<b>%s</b>", self->priv->dir_name);
			gtk_label_set_markup (self->priv->label, markup);
			g_free (markup);
		}

		gtk_label_set_use_markup (self->priv->label, TRUE);

	} else {
		gtk_label_set_use_markup (self->priv->label, FALSE);
		if (self->priv->dir_name) {
			gtk_label_set_text(self->priv->label, self->priv->dir_name);
		}
  	}
}

static void
nautilus_pathbar_button_label_get_preferred_width (GtkWidget *widget,
						   gint      *minimum,
						   gint      *natural)
{
	int width;
	GtkRequisition nat_req, bold_req;
	GtkRequisition nat_min, bold_min;
	NautilusPathbarButtonLabel *self;

 	self = NAUTILUS_PATHBAR_BUTTON_LABEL(widget);
	*minimum = *natural = 0;

	gtk_widget_get_preferred_size (GTK_WIDGET(self->priv->label), &nat_min, &nat_req);
	// We have to show the widget temporary to get a valid size requirement
	//gtk_widget_show(GTK_WIDGET(self->priv->bold_label));
	gtk_widget_get_preferred_size (GTK_WIDGET(self->priv->bold_label), &bold_min, &bold_req);
	//gtk_widget_hide(GTK_WIDGET(self->priv->bold_label));

	width = MAX (nat_req.width, bold_req.width);
	*natural = MIN (width, NAUTILUS_PATH_BAR_LABEL_MAX_WIDTH);
	*minimum = MAX (nat_min.width, bold_min.width);
	g_print("mins %i %i %i %i\n", nat_min.width, nat_req.width, bold_min.width, bold_req.width);
}

static void
nautilus_pathbar_button_label_get_preferred_height (GtkWidget *widget,
						    gint      *minimum,
						    gint      *natural)
{
	int height;
	GtkRequisition nat_req, bold_req;
	NautilusPathbarButtonLabel *self;

 	self = NAUTILUS_PATHBAR_BUTTON_LABEL(widget);
	*minimum = *natural = 0;

	gtk_widget_get_preferred_size (GTK_WIDGET(self->priv->label), NULL, &nat_req);
	// We have to show the widget temporary to get a valid size requirement
	//gtk_widget_show(GTK_WIDGET(self->priv->bold_label));
	gtk_widget_get_preferred_size (GTK_WIDGET(self->priv->bold_label), &bold_req, NULL);
	//gtk_widget_hide(GTK_WIDGET(self->priv->bold_label));

	*natural = MAX (nat_req.height, bold_req.height);
}

static void
nautilus_pathbar_button_label_class_init (NautilusPathbarButtonLabelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->finalize = nautilus_pathbar_button_label_finalize;

	widget_class->get_preferred_width = nautilus_pathbar_button_label_get_preferred_width;
	widget_class->get_preferred_height = nautilus_pathbar_button_label_get_preferred_height;
}

static void
nautilus_pathbar_button_label_init (NautilusPathbarButtonLabel *self)
{
	self->priv = nautilus_pathbar_button_label_get_instance_private (self);

	self->priv->label = gtk_label_new(NULL);
	self->priv->bold_label = gtk_label_new(NULL);

	gtk_label_set_ellipsize (self->priv->label, PANGO_ELLIPSIZE_MIDDLE);
	gtk_label_set_ellipsize (self->priv->bold_label, PANGO_ELLIPSIZE_MIDDLE);
	gtk_label_set_single_line_mode (self->priv->label, TRUE);
	gtk_label_set_single_line_mode (self->priv->bold_label, TRUE);
	gtk_widget_set_no_show_all (GTK_WIDGET(self->priv->bold_label), TRUE);

	gtk_box_pack_start(GTK_BOX(self), GTK_WIDGET(self->priv->label), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(self), GTK_WIDGET(self->priv->bold_label), FALSE, FALSE, 0);
}
