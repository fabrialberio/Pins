/* pins-app-grid.c
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

#include "pins-app-grid.h"

#include "pins-app-group.h"
#include "pins-desktop-file.h"

struct _PinsAppGrid
{
    AdwBin parent_instance;

    GListModel *model;

    PinsAppGroup *edited_apps_group;
    PinsAppGroup *system_apps_group;
    PinsAppGroup *hidden_apps_group;
};

G_DEFINE_TYPE (PinsAppGrid, pins_app_grid, ADW_TYPE_BIN);

enum
{
    ACTIVATE,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

PinsAppGrid *
pins_app_grid_new (void)
{
    return g_object_new (PINS_TYPE_APP_GRID, NULL);
}

void
pins_app_grid_items_changed_cb (GListModel *model, guint position,
                                guint removed, guint added, PinsAppGrid *self)
{
    g_autoptr (PinsDesktopFile) current = NULL;
    guint i, edited_section_end = 0, system_section_end = 0,
             hidden_section_end = 0;

    for (i = 0; i < g_list_model_get_n_items (model); i++)
        {
            current = PINS_DESKTOP_FILE (g_list_model_get_item (model, i));

            if (pins_desktop_file_is_user_edited (current))
                edited_section_end = i + 1;
            else if (pins_desktop_file_is_shown (current))
                system_section_end = i + 1;
        }

    if (system_section_end < edited_section_end)
        system_section_end = edited_section_end;

    hidden_section_end = i;

    g_warning ("Items-changed @%d -%d +%d, 0 - %d - %d - %d", position,
               removed, added, edited_section_end, system_section_end,
               hidden_section_end);

    pins_app_group_set_model (self->edited_apps_group, self->model, 0,
                              edited_section_end);
    pins_app_group_set_model (self->system_apps_group, self->model,
                              edited_section_end,
                              system_section_end - edited_section_end);
    pins_app_group_set_model (self->hidden_apps_group, self->model,
                              system_section_end,
                              hidden_section_end - edited_section_end);

    // pins_app_group_set_size (self->edited_apps_group, edited_section_end);
    // pins_app_group_set_size (self->system_apps_group,
    //                          system_section_end - edited_section_end);
    // pins_app_group_set_offset (self->system_apps_group, edited_section_end);
    // pins_app_group_set_size (self->hidden_apps_group,
    //                          hidden_section_end - system_section_end);
    // pins_app_group_set_offset (self->hidden_apps_group, system_section_end);
}

void
pins_app_grid_set_model (PinsAppGrid *self, GListModel *model)
{
    self->model = model;

    g_signal_connect_object (G_LIST_MODEL (model), "items-changed",
                             G_CALLBACK (pins_app_grid_items_changed_cb), self,
                             0);

    pins_app_group_set_model (self->edited_apps_group, self->model, 0, 26);
    pins_app_group_set_model (self->system_apps_group, self->model, 26, 48);
    pins_app_group_set_model (self->hidden_apps_group, self->model, 26 + 48,
                              100);
}

static void
pins_app_grid_dispose (GObject *object)
{
    gtk_widget_dispose_template (GTK_WIDGET (object), PINS_TYPE_APP_GRID);

    G_OBJECT_CLASS (pins_app_grid_parent_class)->dispose (object);
}

static void
pins_app_grid_class_init (PinsAppGridClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    object_class->dispose = pins_app_grid_dispose;

    signals[ACTIVATE] = g_signal_new (
        "activate", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, g_cclosure_marshal_VOID__UINT, G_TYPE_NONE, 1, G_TYPE_UINT);

    g_signal_set_va_marshaller (signals[ACTIVATE], G_TYPE_FROM_CLASS (klass),
                                g_cclosure_marshal_VOID__UINTv);

    gtk_widget_class_set_template_from_resource (
        widget_class, "/io/github/fabrialberio/pinapp/pins-app-grid.ui");
    g_type_ensure (PINS_TYPE_APP_GROUP);

    gtk_widget_class_bind_template_child (widget_class, PinsAppGrid,
                                          edited_apps_group);
    gtk_widget_class_bind_template_child (widget_class, PinsAppGrid,
                                          system_apps_group);
    gtk_widget_class_bind_template_child (widget_class, PinsAppGrid,
                                          hidden_apps_group);
}

void
group_activated_cb (PinsAppGrid *self, guint position)
{
    g_signal_emit (self, signals[ACTIVATE], 0, position);
}

static void
pins_app_grid_init (PinsAppGrid *self)
{
    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect_object (self->edited_apps_group, "activate",
                             G_CALLBACK (group_activated_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->system_apps_group, "activate",
                             G_CALLBACK (group_activated_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (self->hidden_apps_group, "activate",
                             G_CALLBACK (group_activated_cb), self,
                             G_CONNECT_SWAPPED);
}
