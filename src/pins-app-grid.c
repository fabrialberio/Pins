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

#include "pins-app-tile.h"

struct _PinsAppGrid
{
    AdwBin parent_instance;

    gboolean is_loaded;
    GtkGridView *grid_view;
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
item_activated_cb (PinsAppGrid *self, guint position)
{
    g_signal_emit (self, signals[ACTIVATE], 0, position);
}

void
items_changed_cb (GListModel *model, guint position, guint removed,
                  guint added, PinsAppGrid *self)
{
    // Fixes grid not being scrolled to the top when opening the app.
    if (self->is_loaded)
        return;

    if (g_list_model_get_n_items (model) > 0)
        {
            gtk_grid_view_scroll_to (self->grid_view, 0, GTK_LIST_SCROLL_NONE,
                                     NULL);
            self->is_loaded = TRUE;
        }
}

void
pins_app_grid_set_model (PinsAppGrid *self, GListModel *model)
{
    GtkNoSelection *selection_model = gtk_no_selection_new (model);

    gtk_grid_view_set_model (self->grid_view,
                             GTK_SELECTION_MODEL (selection_model));

    g_signal_connect_object (self->grid_view, "activate",
                             G_CALLBACK (item_activated_cb), self,
                             G_CONNECT_SWAPPED);
    g_signal_connect_object (model, "items-changed",
                             G_CALLBACK (items_changed_cb), self, 0);
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
    gtk_widget_class_bind_template_child (widget_class, PinsAppGrid,
                                          grid_view);
}

void
item_setup_cb (GtkSignalListItemFactory *factory, GtkListItem *item)
{
    PinsAppTile *tile = pins_app_tile_new ();

    gtk_list_item_set_child (item, GTK_WIDGET (tile));
}

void
item_bind_cb (GtkSignalListItemFactory *factory, GtkListItem *item)
{
    PinsDesktopFile *desktop_file = gtk_list_item_get_item (item);
    PinsAppTile *tile = PINS_APP_TILE (gtk_list_item_get_child (item));

    g_assert (PINS_IS_DESKTOP_FILE (desktop_file));

    pins_app_tile_set_desktop_file (tile, desktop_file);
}

static void
pins_app_grid_init (PinsAppGrid *self)
{
    GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();

    self->is_loaded = FALSE;

    gtk_widget_init_template (GTK_WIDGET (self));

    g_signal_connect_object (factory, "setup", G_CALLBACK (item_setup_cb),
                             NULL, 0);
    g_signal_connect_object (factory, "bind", G_CALLBACK (item_bind_cb), NULL,
                             0);

    gtk_grid_view_set_factory (self->grid_view, factory);
}
