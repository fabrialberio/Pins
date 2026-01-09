/* pins-app-tile.c
 *
 * Copyright 2024 Fabrizio
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pins-app-tile.h"

#include "pins-app-icon.h"
#include "pins-locale-utils-private.h"

struct _PinsAppTile
{
    GtkBox parent_instance;

    PinsDesktopFile *desktop_file;

    PinsAppIcon *icon;
    AdwBin *invisible_glyph;
    GtkLabel *title;
};

G_DEFINE_TYPE (PinsAppTile, pins_app_tile, GTK_TYPE_BOX);

PinsAppTile *
pins_app_tile_new (void)
{
    return g_object_new (PINS_TYPE_APP_TILE, NULL);
}

void
pins_app_tile_update_appearance (PinsAppTile *self,
                                 PinsDesktopFile *desktop_file)
{
    const gchar *title_key;
    gboolean invisible;

    title_key = _pins_join_key_locale (
        G_KEY_FILE_DESKTOP_KEY_NAME,
        pins_desktop_file_get_locale_for_key (desktop_file,
                                              G_KEY_FILE_DESKTOP_KEY_NAME));

    gtk_label_set_text (
        self->title, pins_desktop_file_get_string (desktop_file, title_key));

    invisible = !pins_desktop_file_is_shown (desktop_file);

    gtk_widget_set_opacity (GTK_WIDGET (self->icon), invisible ? 0.5 : 1);
    gtk_widget_set_visible (GTK_WIDGET (self->invisible_glyph), invisible);
}

void
key_set_cb (PinsAppTile *self, gchar *key, PinsDesktopFile *desktop_file)
{
    pins_app_tile_update_appearance (self, desktop_file);
}

void
pins_app_tile_set_desktop_file (PinsAppTile *self,
                                PinsDesktopFile *desktop_file)
{
    g_assert (PINS_IS_DESKTOP_FILE (desktop_file));

    self->desktop_file = g_object_ref (desktop_file);

    g_signal_connect_object (self->desktop_file, "key-set",
                             G_CALLBACK (key_set_cb), self, G_CONNECT_SWAPPED);

    pins_app_icon_set_desktop_file (self->icon, self->desktop_file);

    pins_app_tile_update_appearance (self, self->desktop_file);
}

static void
pins_app_tile_dispose (GObject *object)
{
    PinsAppTile *self = PINS_APP_TILE (object);

    g_clear_object (&self->desktop_file);

    gtk_widget_dispose_template (GTK_WIDGET (object), PINS_TYPE_APP_TILE);

    G_OBJECT_CLASS (pins_app_tile_parent_class)->dispose (object);
}

static void
pins_app_tile_class_init (PinsAppTileClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_app_tile_dispose;

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-app-tile.ui");
    g_type_ensure (PINS_TYPE_APP_ICON);

    gtk_widget_class_bind_template_child (widget_class, PinsAppTile, icon);
    gtk_widget_class_bind_template_child (widget_class, PinsAppTile,
                                          invisible_glyph);
    gtk_widget_class_bind_template_child (widget_class, PinsAppTile, title);
}

static GdkContentProvider *
pins_app_tile_drag_prepare_cb (PinsAppTile *self, double x, double y,
                               GtkDragSource *source)
{
    GFile *file = pins_desktop_file_get_user_file (self->desktop_file);

    return gdk_content_provider_new_typed (G_TYPE_FILE, file);
}

static void
pins_app_tile_drag_begin_cb (PinsAppTile *self, GdkDrag *drag,
                             GtkDragSource *source)
{
    g_autoptr (GdkPaintable) paintable = gtk_widget_paintable_new (
        gtk_widget_get_first_child (GTK_WIDGET (self->icon)));
    gtk_drag_source_set_icon (source, paintable, 0, 0);
}

static void
pins_app_tile_init (PinsAppTile *self)
{
    GtkDragSource *drag_source = gtk_drag_source_new ();
    gtk_drag_source_set_actions (drag_source, GDK_ACTION_LINK);

    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect_object (drag_source, "prepare",
                             G_CALLBACK (pins_app_tile_drag_prepare_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (drag_source, "drag-begin",
                             G_CALLBACK (pins_app_tile_drag_begin_cb), self,
                             G_CONNECT_SWAPPED);

    gtk_widget_add_controller (GTK_WIDGET (self),
                               GTK_EVENT_CONTROLLER (drag_source));
}
