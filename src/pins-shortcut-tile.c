/* pins-shortcut-tile.c
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

#include "pins-shortcut-tile.h"

#include "pins-locale-utils-private.h"
#include "pins-shortcut-icon.h"

struct _PinsShortcutTile
{
    GtkBox parent_instance;

    PinsShortcut *shortcut;

    PinsShortcutIcon *icon;
    AdwBin *invisible_glyph;
    GtkLabel *title;
};

G_DEFINE_TYPE (PinsShortcutTile, pins_shortcut_tile, GTK_TYPE_BOX);

PinsShortcutTile *
pins_shortcut_tile_new (void)
{
    return g_object_new (PINS_TYPE_SHORTCUT_TILE, NULL);
}

void
pins_shortcut_tile_update_appearance (PinsShortcutTile *self,
                                      PinsShortcut *shortcut)
{
    const gchar *title_key;
    gboolean invisible;

    title_key
        = _pins_join_key_locale (G_KEY_FILE_DESKTOP_KEY_NAME,
                                 pins_shortcut_get_locale_for_key (
                                     shortcut, G_KEY_FILE_DESKTOP_KEY_NAME));

    gtk_label_set_text (self->title,
                        pins_shortcut_get_string (shortcut, title_key));

    invisible = !pins_shortcut_is_shown (shortcut);

    gtk_widget_set_opacity (GTK_WIDGET (self->icon), invisible ? 0.5 : 1);
    gtk_widget_set_visible (GTK_WIDGET (self->invisible_glyph), invisible);
}

void
key_set_cb (PinsShortcutTile *self, gchar *key, PinsShortcut *shortcut)
{
    pins_shortcut_tile_update_appearance (self, shortcut);
}

void
pins_shortcut_tile_set_shortcut (PinsShortcutTile *self,
                                 PinsShortcut *shortcut)
{
    g_assert (PINS_IS_SHORTCUT (shortcut));

    self->shortcut = g_object_ref (shortcut);

    g_signal_connect_object (self->shortcut, "key-set",
                             G_CALLBACK (key_set_cb), self, G_CONNECT_SWAPPED);

    pins_shortcut_icon_set_shortcut (self->icon, self->shortcut);

    pins_shortcut_tile_update_appearance (self, self->shortcut);
}

static void
pins_shortcut_tile_dispose (GObject *object)
{
    PinsShortcutTile *self = PINS_SHORTCUT_TILE (object);

    g_clear_object (&self->shortcut);

    gtk_widget_dispose_template (GTK_WIDGET (object), PINS_TYPE_SHORTCUT_TILE);

    G_OBJECT_CLASS (pins_shortcut_tile_parent_class)->dispose (object);
}

static void
pins_shortcut_tile_class_init (PinsShortcutTileClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_shortcut_tile_dispose;

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-shortcut-tile.ui");
    g_type_ensure (PINS_TYPE_SHORTCUT_ICON);

    gtk_widget_class_bind_template_child (widget_class, PinsShortcutTile,
                                          icon);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutTile,
                                          invisible_glyph);
    gtk_widget_class_bind_template_child (widget_class, PinsShortcutTile,
                                          title);
}

static GdkContentProvider *
pins_shortcut_tile_drag_prepare_cb (PinsShortcutTile *self, double x, double y,
                                    GtkDragSource *source)
{
    GFile *file = pins_shortcut_get_user_file (self->shortcut);

    return gdk_content_provider_new_typed (G_TYPE_FILE, file);
}

static void
pins_shortcut_tile_drag_begin_cb (PinsShortcutTile *self, GdkDrag *drag,
                                  GtkDragSource *source)
{
    g_autoptr (GdkPaintable) paintable = gtk_widget_paintable_new (
        gtk_widget_get_first_child (GTK_WIDGET (self->icon)));
    gtk_drag_source_set_icon (source, paintable, 0, 0);
}

static void
pins_shortcut_tile_init (PinsShortcutTile *self)
{
    GtkDragSource *drag_source = gtk_drag_source_new ();
    gtk_drag_source_set_actions (drag_source, GDK_ACTION_LINK);

    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect_object (drag_source, "prepare",
                             G_CALLBACK (pins_shortcut_tile_drag_prepare_cb),
                             self, G_CONNECT_SWAPPED);
    g_signal_connect_object (drag_source, "drag-begin",
                             G_CALLBACK (pins_shortcut_tile_drag_begin_cb),
                             self, G_CONNECT_SWAPPED);

    gtk_widget_add_controller (GTK_WIDGET (self),
                               GTK_EVENT_CONTROLLER (drag_source));
}
