/* pins-app-group.c
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

#include "pins-app-group.h"

#include "pins-app-tile.h"
#include "pins-desktop-file.h"

struct _PinsAppGroup
{
    GtkBox parent_instance;

    GtkLabel *title;
    GtkFlowBox *flow_box;
};

G_DEFINE_TYPE (PinsAppGroup, pins_app_group, GTK_TYPE_BOX);

enum
{
    PROP_0,
    PROP_TITLE,
    N_PROPS,
};

enum
{
    ACTIVATE,
    N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

PinsAppGroup *
pins_app_group_new (void)
{
    return g_object_new (PINS_TYPE_APP_GROUP, NULL);
}

GtkWidget *
create_widget_func (gpointer item, gpointer user_data)
{
    PinsDesktopFile *desktop_file = PINS_DESKTOP_FILE (item);
    PinsAppTile *tile = pins_app_tile_new ();

    pins_app_tile_set_desktop_file (tile, desktop_file);

    return GTK_WIDGET (tile);
}

void
child_activated_cb (PinsAppGroup *self, GtkFlowBoxChild *child)
{
    guint position = gtk_flow_box_child_get_index (child);

    g_signal_emit (self, signals[ACTIVATE], 0, position);
}

void
pins_app_group_set_model (PinsAppGroup *self, GListModel *model)
{
    gtk_flow_box_bind_model (self->flow_box, model, &create_widget_func, NULL,
                             NULL);

    g_signal_connect_object (self->flow_box, "child-activated",
                             G_CALLBACK (child_activated_cb), self,
                             G_CONNECT_SWAPPED);
}

static void
pins_app_group_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), PINS_TYPE_APP_GROUP);

    G_OBJECT_CLASS (pins_app_group_parent_class)->dispose (object);
}

static void
pins_app_group_set_property (GObject *object, guint prop_id,
                             const GValue *value, GParamSpec *pspec)
{
    PinsAppGroup *self = PINS_APP_GROUP (object);

    switch (prop_id)
        {
        case PROP_TITLE:
            gtk_label_set_label (self->title, g_value_get_string (value));
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
pins_app_group_class_init (PinsAppGroupClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_app_group_dispose;
    object_class->set_property = pins_app_group_set_property;

    properties[PROP_TITLE]
        = g_param_spec_string ("title", "Title", "Title of the app group", "",
                               (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties (object_class, N_PROPS, properties);

    signals[ACTIVATE] = g_signal_new (
        "activate", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, g_cclosure_marshal_VOID__UINT, G_TYPE_NONE, 1, G_TYPE_UINT);

    g_signal_set_va_marshaller (signals[ACTIVATE], G_TYPE_FROM_CLASS (klass),
                                g_cclosure_marshal_VOID__UINTv);

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-app-group.ui");
    g_type_ensure (PINS_TYPE_APP_GROUP);

    gtk_widget_class_bind_template_child (widget_class, PinsAppGroup, title);
    gtk_widget_class_bind_template_child (widget_class, PinsAppGroup,
                                          flow_box);
}

static void
pins_app_group_init (PinsAppGroup *self)
{
    GtkDragSource *drag_source = gtk_drag_source_new ();
    gtk_drag_source_set_actions (drag_source, GDK_ACTION_LINK);

    gtk_widget_init_template (GTK_WIDGET (self));
}
