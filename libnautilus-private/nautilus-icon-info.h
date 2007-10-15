#ifndef NAUTILUS_ICON_INFO_H
#define NAUTILUS_ICON_INFO_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdktypes.h>
#include <gio/gicon.h>

G_BEGIN_DECLS

typedef struct _NautilusIconInfo      NautilusIconInfo;
typedef struct _NautilusIconInfoClass NautilusIconInfoClass;


#define NAUTILUS_TYPE_ICON_INFO                 (nautilus_icon_info_get_type ())
#define NAUTILUS_ICON_INFO(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ICON_INFO, NautilusIconInfo))
#define NAUTILUS_ICON_INFO_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ICON_INFO, NautilusIconInfoClass))
#define NAUTILUS_IS_ICON_INFO(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ICON_INFO))
#define NAUTILUS_IS_ICON_INFO_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ICON_INFO))
#define NAUTILUS_ICON_INFO_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_ICON_INFO, NautilusIconInfoClass))


GType    nautilus_icon_info_get_type (void) G_GNUC_CONST;

NautilusIconInfo *    nautilus_icon_info_new_for_pixbuf     (GdkPixbuf         *pixbuf);
NautilusIconInfo *    nautilus_icon_info_lookup             (GIcon             *icon,
							     int                size);

GdkPixbuf *           nautilus_icon_info_get_pixbuf           (NautilusIconInfo  *icon);
GdkPixbuf *           nautilus_icon_info_get_pixbuf_nodefault (NautilusIconInfo  *icon);
GdkPixbuf *           nautilus_icon_info_get_pixbuf_at_size   (NautilusIconInfo  *icon,
							       gsize              forced_size);
gboolean              nautilus_icon_info_get_embedded_rect    (NautilusIconInfo  *icon,
							       GdkRectangle      *rectangle);
gboolean              nautilus_icon_info_get_attach_points    (NautilusIconInfo  *icon,
							       GdkPoint         **points,
							       gint              *n_points);
G_CONST_RETURN gchar *nautilus_icon_info_get_display_name     (NautilusIconInfo  *icon);

void                  nautilus_icon_info_clear_caches         (void);

G_END_DECLS

#endif /* NAUTILUS_ICON_INFO_H */

