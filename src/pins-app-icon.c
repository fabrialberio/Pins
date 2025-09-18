/* pins-app-icon.c
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

#include "pins-app-icon.h"

#define DEFAULT_ICON_NAME "application-x-executable"

struct _PinsAppIcon
{
    GtkWidget parent_instance;

    GtkImage *image;
};

G_DEFINE_TYPE (PinsAppIcon, pins_app_icon, GTK_TYPE_WIDGET)

enum
{
    PROP_0,
    PROP_PIXEL_SIZE,
    N_PROPS,
};

static GParamSpec *properties[N_PROPS];

void
pins_app_icon_set_icon_name (PinsAppIcon *self, gchar *icon_name)
{
    GtkIconTheme *theme
        = gtk_icon_theme_get_for_display (gdk_display_get_default ());
    g_autofree gchar *host_filename
        = g_build_filename ("/run/host", icon_name, NULL);

    if (!g_strcmp0 (icon_name, ""))
        gtk_image_set_from_icon_name (self->image, DEFAULT_ICON_NAME);

    if (gtk_icon_theme_has_icon (theme, icon_name)
        || gtk_icon_theme_has_icon (
            theme, g_strconcat (icon_name, "-symbolic", NULL)))
        gtk_image_set_from_icon_name (self->image, icon_name);
    else if (g_file_test (icon_name, G_FILE_TEST_IS_REGULAR))
        gtk_image_set_from_file (self->image, icon_name);
    else if (g_file_test (host_filename, G_FILE_TEST_IS_REGULAR))
        gtk_image_set_from_file (self->image, host_filename);
    else
        gtk_image_set_from_icon_name (self->image, DEFAULT_ICON_NAME);
}

void
pins_app_icon_key_set_cb (PinsDesktopFile *desktop_file, gchar *key,
                          PinsAppIcon *self)
{
    g_assert (PINS_IS_APP_ICON (self));

    if (g_strcmp0 (key, G_KEY_FILE_DESKTOP_KEY_ICON) == 0)
        {
            gchar *icon_name = pins_desktop_file_get_string (
                desktop_file, G_KEY_FILE_DESKTOP_KEY_ICON);

            pins_app_icon_set_icon_name (self, icon_name);
        }
}

void
pins_app_icon_set_desktop_file (PinsAppIcon *self,
                                PinsDesktopFile *desktop_file)
{
    gchar *icon_name = NULL;

    g_assert (PINS_IS_DESKTOP_FILE (desktop_file));

    icon_name = pins_desktop_file_get_string (desktop_file,
                                              G_KEY_FILE_DESKTOP_KEY_ICON);

    g_signal_connect_object (desktop_file, "key-set",
                             G_CALLBACK (pins_app_icon_key_set_cb), self, 0);

    pins_app_icon_set_icon_name (self, icon_name);
}

static void
pins_app_icon_dispose (GObject *object)
{
    PinsAppIcon *self = PINS_APP_ICON (object);

    gtk_widget_unparent (GTK_WIDGET (self->image));

    G_OBJECT_CLASS (pins_app_icon_parent_class)->dispose (object);
}

static void
pins_app_icon_get_property (GObject *object, guint prop_id, GValue *value,
                            GParamSpec *pspec)
{
    PinsAppIcon *self = PINS_APP_ICON (object);

    switch (prop_id)
        {
        case PROP_PIXEL_SIZE:
            g_value_set_int (value, gtk_image_get_pixel_size (self->image));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
pins_app_icon_set_property (GObject *object, guint prop_id,
                            const GValue *value, GParamSpec *pspec)
{
    PinsAppIcon *self = PINS_APP_ICON (object);

    switch (prop_id)
        {
        case PROP_PIXEL_SIZE:
            gtk_image_set_pixel_size (self->image, g_value_get_int (value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
pins_app_icon_class_init (PinsAppIconClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->get_property = pins_app_icon_get_property;
    object_class->set_property = pins_app_icon_set_property;
    object_class->dispose = pins_app_icon_dispose;

    properties[PROP_PIXEL_SIZE] = g_param_spec_int (
        "pixel-size", "Pixel Size", "Pixel size of the app icon", 0, G_MAXINT,
        32, (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_properties (object_class, N_PROPS, properties);

    gtk_widget_class_set_layout_manager_type (widget_class,
                                              GTK_TYPE_BIN_LAYOUT);
}

static void
pins_app_icon_init (PinsAppIcon *self)
{
    self->image = GTK_IMAGE (g_object_new (
        GTK_TYPE_IMAGE, "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
        "icon-name", DEFAULT_ICON_NAME, NULL));
    gtk_widget_set_parent (GTK_WIDGET (self->image), GTK_WIDGET (self));

    gtk_widget_add_css_class (GTK_WIDGET (self->image), "icon-dropshadow");
}
